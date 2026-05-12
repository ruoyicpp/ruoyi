// RuoYi-Cpp 远程授权 API（移植自 ruoyi-server LicenseApiCtrl）
//
// 部署在中央服务上，供客户端 ruoyi-cpp 实例远程验证许可证 + 心跳保活。
// 配合现有 LicenseManager.h（客户端验证本地 license.lic 文件签名）形成完整许可链路。
//
// API:
//   POST /api/license/verify       - 客户端启动时验证（首次绑定硬件指纹）
//   POST /api/license/heartbeat    - 心跳保活（定期调用，更新 last_heartbeat）
//   POST /api/license/issue        - 签发新许可证（管理员，返回 license_key + license.lic 内容）
//   GET  /api/license/list         - 已签发列表（管理员）
//   DELETE /api/license/{id}       - 删除（管理员）
//   GET  /api/license/{id}/export  - 重新导出 license.lic（根据 DB 重建签名文件）
//
// DB 表 sys_license（DatabaseInit.cc）：
//   id, license_key, licensee, fp_hash, fp_primary, expire_date, max_users,
//   features, grace_days, machine_id, mac, cpu, status, created_at, last_heartbeat
#pragma once
#include <drogon/HttpController.h>
#include <openssl/rand.h>
#include <ctime>
#include <cstdio>
#include "../AjaxResult.h"
#include "../HardwareFingerprint.h"
#include "../LicenseManager.h"
#include "../OperLogUtils.h"
#include "../../services/DatabaseService.h"
#include "../../filters/PermFilter.h"

class LicenseApiCtrl : public drogon::HttpController<LicenseApiCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(LicenseApiCtrl::verify,    "/api/license/verify",       drogon::Post);
        ADD_METHOD_TO(LicenseApiCtrl::heartbeat, "/api/license/heartbeat",    drogon::Post);
        ADD_METHOD_TO(LicenseApiCtrl::issue,     "/api/license/issue",        drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(LicenseApiCtrl::list,      "/api/license/list",         drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(LicenseApiCtrl::remove,    "/api/license/{id}",         drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(LicenseApiCtrl::exportLic, "/api/license/{id}/export",  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(LicenseApiCtrl::page,       "/api/license/page",         drogon::Get);   // InnerLink HTML
    METHOD_LIST_END

    // ── GET /api/license/page — 内嵌管理 HTML ─────────────────────
    void page(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        (void)req;
        std::string html = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head><meta charset="UTF-8"><title>License 管理</title>
<style>
  body{font-family:-apple-system,BlinkMacSystemFont,"Helvetica Neue",Arial,sans-serif;
       background:#fff;color:#303133;margin:0;padding:20px;font-size:14px}
  .title{font-size:16px;font-weight:600;color:#303133;margin:0 0 16px;
         padding-bottom:12px;border-bottom:1px solid #ebeef5}
  .toolbar{margin-bottom:12px;display:flex;gap:8px;align-items:center;flex-wrap:wrap}
  input,select,textarea{padding:6px 10px;border-radius:2px;border:1px solid #dcdfe6;
                        font-size:13px;color:#606266}
  input:focus,select:focus,textarea:focus{outline:none;border-color:#409eff}
  button{padding:6px 14px;border-radius:2px;border:1px solid #dcdfe6;cursor:pointer;
         font-size:13px;background:#fff;color:#606266}
  button:hover{color:#409eff;border-color:#c6e2ff}
  button.primary{background:#409eff;border-color:#409eff;color:#fff}
  button.primary:hover{background:#66b1ff;border-color:#66b1ff;color:#fff}
  button.danger{color:#f56c6c;border-color:#fbc4c4}
  button.danger:hover{background:#fef0f0;border-color:#f56c6c}
  button.export{color:#67c23a;border-color:#c2e7b0}
  button.export:hover{background:#f0f9eb;border-color:#67c23a}
  table{width:100%;background:#fff;border-collapse:collapse;font-size:12px;
        border:1px solid #ebeef5}
  th,td{padding:9px 10px;text-align:left;border-bottom:1px solid #ebeef5;color:#606266}
  th{background:#fff;font-weight:500;color:#909399}
  tr:last-child td{border-bottom:none}
  code{font-family:Consolas,monospace;font-size:12px;color:#303133}
  .pill{font-size:12px}
  .on{color:#67c23a}.off{color:#f56c6c}
  .modal-bg{display:none;position:fixed;inset:0;background:rgba(0,0,0,.5);
            align-items:center;justify-content:center;z-index:99}
  .modal-bg.show{display:flex}
  .modal{background:#fff;padding:20px;border-radius:2px;width:520px;max-width:90vw;
         border:1px solid #ebeef5}
  .modal h2{margin:0 0 16px;font-size:16px;font-weight:600;
            padding-bottom:12px;border-bottom:1px solid #ebeef5}
  .modal label{display:block;margin-top:12px;font-size:13px;color:#606266}
  .modal input,.modal select,.modal textarea{width:100%;box-sizing:border-box;margin-top:4px}
  .modal textarea{font-family:Consolas,monospace;height:120px}
  .modal .row{display:flex;gap:8px;margin-top:20px;justify-content:flex-end}
  .keyshow{padding:12px;margin:8px 0;word-break:break-all;
           font-family:Consolas,monospace;font-size:12px;
           color:#303133;border:1px solid #ebeef5}
</style></head>
<body>
<div class="title">License 授权管理</div>
<div class="toolbar">
  <button class="primary" onclick="load()">刷新</button>
  <button onclick="openIssue()">签发新 License</button>
</div>
<table>
  <thead><tr>
    <th>ID</th><th>License Key</th><th>被授权方</th><th>到期</th>
    <th>用户上限</th><th>功能</th><th>状态</th><th>最后心跳</th><th>操作</th>
  </tr></thead>
  <tbody id="tb"><tr><td colspan="9" style="text-align:center;padding:30px">加载中…</td></tr></tbody>
</table>

<div class="modal-bg" id="mIssue"><div class="modal">
  <h2>签发新 License</h2>
  <label>被授权方 *<input id="fLicensee"></label>
  <label>到期日期（YYYY-MM-DD 或 PERPETUAL 永久） <input id="fExpire" value="PERPETUAL"></label>
  <label>用户数上限（0=不限） <input id="fMax" type="number" value="0"></label>
  <label>功能列表（如 FULL / BASIC,PAY,REPORT） <input id="fFeatures" value="FULL"></label>
  <label>宽限期（天） <input id="fGrace" type="number" value="7"></label>
  <label>客户端硬件指纹 fp_hash（可选；留空则首次 verify 时自动绑定）<input id="fFpHash"></label>
  <label>客户端硬件指纹 fp_primary（可选） <input id="fFpPrimary"></label>
  <div class="row"><button onclick="close_('mIssue')">取消</button><button class="primary" onclick="doIssue()">签发</button></div>
</div></div>

<div class="modal-bg" id="mShow"><div class="modal">
  <h2>签发成功</h2>
  <p style="font-size:13px;color:#909399;margin:8px 0">License Key（后续在列表里也能查到）：</p>
  <div class="keyshow" id="newKey"></div>
  <p style="font-size:13px;color:#909399;margin:8px 0">license.lic 文件内容（替换客户端 exe 同目录的同名文件）：</p>
  <div class="keyshow" id="newContent" style="max-height:240px;overflow:auto;white-space:pre-wrap"></div>
  <div class="row">
    <button onclick="close_('mShow')">关闭</button>
    <button onclick="downloadLic()">下载</button>
    <button class="primary" onclick="copyLicContent()">复制</button>
  </div>
</div></div>

<script>
let token = '';
try {
  const u = new URL(location.href);
  token = u.searchParams.get('token') || '';
  if (!token && parent !== window) { try { token = parent.sessionStorage.getItem('Admin-Token') || ''; } catch(e){} }
  if (!token) token = sessionStorage.getItem('Admin-Token') || '';
} catch(e){}
const H = { 'Authorization': 'Bearer '+token, 'Content-Type': 'application/json' };
const API = location.pathname.replace(/\/page$/, '');   // /api/license

async function load() {
  const r = await fetch(API + '/list', { headers: H });
  const d = await r.json();
  const tb = document.getElementById('tb');
  if (d.code !== 200) { tb.innerHTML='<tr><td colspan="9">'+(d.msg||'加载失败')+'</td></tr>'; return; }
  const rows = d.data || [];
  if (rows.length === 0) { tb.innerHTML='<tr><td colspan="9" style="text-align:center;padding:30px">暂无 License</td></tr>'; return; }
  tb.innerHTML = rows.map(it => `<tr>
    <td>${it.id}</td>
    <td><code>${esc(it.license_key)}</code></td>
    <td>${esc(it.licensee)}</td>
    <td>${esc(it.expire_date)}</td>
    <td>${it.max_users || '∞'}</td>
    <td>${esc(it.features)}</td>
    <td><span class="pill ${it.status?'on':'off'}">${it.status?'启用':'禁用'}</span></td>
    <td>${esc(it.last_heartbeat || '-')}</td>
    <td>
      <button class="export" onclick="exportLic(${it.id})">导出</button>
      <button class="danger" onclick="del(${it.id})">删</button>
    </td></tr>`).join('');
}

function esc(s){ return String(s||'').replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }
function close_(id){ document.getElementById(id).classList.remove('show'); }
function openIssue(){ document.getElementById('mIssue').classList.add('show'); }

async function doIssue() {
  const body = {
    licensee: document.getElementById('fLicensee').value.trim(),
    expire_date: document.getElementById('fExpire').value.trim() || 'PERPETUAL',
    max_users: parseInt(document.getElementById('fMax').value) || 0,
    features: document.getElementById('fFeatures').value.trim() || 'FULL',
    grace_days: parseInt(document.getElementById('fGrace').value) || 7,
    fp_hash: document.getElementById('fFpHash').value.trim(),
    fp_primary: document.getElementById('fFpPrimary').value.trim()
  };
  if (!body.licensee) { alert('被授权方必填'); return; }
  const r = await fetch(API + '/issue', { method:'POST', headers:H, body: JSON.stringify(body) });
  const d = await r.json();
  if (d.code !== 200) { alert(d.msg||'签发失败'); return; }
  close_('mIssue');
  document.getElementById('newKey').textContent = d.data.license_key;
  document.getElementById('newContent').textContent = d.data.license_content;
  document.getElementById('mShow').classList.add('show');
  load();
}

async function exportLic(id) {
  const r = await fetch(API + '/' + id + '/export', { headers: H });
  const d = await r.json();
  if (d.code !== 200) { alert(d.msg||'导出失败'); return; }
  document.getElementById('newKey').textContent = '(已签发记录重新导出)';
  document.getElementById('newContent').textContent = d.data.license_content;
  document.getElementById('mShow').classList.add('show');
}

function copyLicContent(){ navigator.clipboard.writeText(document.getElementById('newContent').textContent); }
function downloadLic() {
  const blob = new Blob([document.getElementById('newContent').textContent], {type:'text/plain'});
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'license.lic';
  a.click();
  URL.revokeObjectURL(a.href);
}

async function del(id) {
  if (!confirm('删除 License id='+id+'？关联客户端将无法续期')) return;
  const r = await fetch(API + '/' + id, { method:'DELETE', headers:H });
  const d = await r.json(); if (d.code !== 200) alert(d.msg||'失败'); else load();
}

if (!token) document.getElementById('tb').innerHTML='<tr><td colspan="9" style="text-align:center;padding:30px;color:#cf1322">未获取到 Token</td></tr>';
else load();
</script>
</body></html>)HTML";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(html);
        cb(resp);
    }

    // ── 客户端启动时验证 ─────────────────────────────────────────
    void verify(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "参数错误"); return; }

        std::string licKey    = (*json).get("license_key", "").asString();
        std::string fpHash    = (*json).get("fp_hash", "").asString();
        std::string fpPrimary = (*json).get("fp_primary", "").asString();
        if (licKey.empty() || fpHash.empty()) { RESP_ERR(cb, "缺少 license_key 或 fp_hash"); return; }

        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT id, license_key, licensee, fp_hash, fp_primary, expire_date, "
            "max_users, features, grace_days, status FROM sys_license "
            "WHERE license_key=$1 AND status=1 LIMIT 1", {licKey});

        if (!res.ok() || res.rows() == 0) {
            Json::Value r; r["code"] = 1; r["message"] = "授权码无效或已禁用";
            cb(drogon::HttpResponse::newHttpJsonResponse(r)); return;
        }

        std::string dbFpHash    = res.str(0, 3);
        std::string dbFpPrimary = res.str(0, 4);
        std::string dbLicKey    = res.str(0, 1);

        if (dbFpHash.empty()) {
            // 首次绑定
            db.execParams(
                "UPDATE sys_license SET fp_hash=$1, fp_primary=$2, last_heartbeat=NOW() "
                "WHERE license_key=$3", {fpHash, fpPrimary, dbLicKey});
        } else {
            if (dbFpHash != fpHash && dbFpPrimary != fpPrimary) {
                Json::Value r; r["code"] = 2; r["message"] = "硬件指纹不匹配，此授权码已绑定其他机器";
                cb(drogon::HttpResponse::newHttpJsonResponse(r)); return;
            }
            db.execParams(
                "UPDATE sys_license SET last_heartbeat=NOW() WHERE license_key=$1", {dbLicKey});
        }

        std::string expireDate = res.str(0, 5);
        if (expireDate.empty()) expireDate = "PERPETUAL";
        int daysLeft = (expireDate == "PERPETUAL") ? 99999 : _daysUntil(expireDate);

        Json::Value r;
        r["code"]        = 0;
        r["licensee"]    = res.str(0, 2);
        r["expire_date"] = expireDate;
        r["days_left"]   = daysLeft;
        r["max_users"]   = res.intVal(0, 6);
        r["features"]    = res.str(0, 7).empty() ? "FULL" : res.str(0, 7);
        r["grace_days"]  = res.intVal(0, 8);
        r["message"]     = "";
        cb(drogon::HttpResponse::newHttpJsonResponse(r));
    }

    // ── 心跳 ─────────────────────────────────────────────────────
    void heartbeat(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "参数错误"); return; }

        std::string licKey = (*json).get("license_key", "").asString();
        std::string fpHash = (*json).get("fp_hash", "").asString();
        if (licKey.empty()) { RESP_ERR(cb, "缺少 license_key"); return; }

        bool ok = DatabaseService::instance().execParams(
            "UPDATE sys_license SET last_heartbeat=NOW() "
            "WHERE license_key=$1 AND fp_hash=$2 AND status=1",
            {licKey, fpHash});

        Json::Value r;
        r["code"]    = ok ? 0 : 1;
        r["message"] = ok ? "ok" : "授权码无效";
        cb(drogon::HttpResponse::newHttpJsonResponse(r));
    }

    // ── 管理员签发许可证 ─────────────────────────────────────────
    // 同时生成：
    //   1) sys_license 远程授权记录（license_key, RUOYI-XXXX-XXXX-XXXX）
    //   2) license.lic 本地文件内容（HMAC 签名，可直接覆盖客户端本地文件）
    void issue(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:license:issue");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "参数错误"); return; }

        std::string licensee   = (*json).get("licensee", "").asString();
        std::string expireDate = (*json).get("expire_date", "PERPETUAL").asString();
        int         maxUsers   = (*json).get("max_users", 0).asInt();
        std::string features   = (*json).get("features", "FULL").asString();
        int         graceDays  = (*json).get("grace_days", 7).asInt();
        std::string fpHash     = (*json).get("fp_hash", "").asString();
        std::string fpPrimary  = (*json).get("fp_primary", "").asString();
        std::string machineId  = (*json).get("machine_id", "").asString();
        std::string macAddr    = (*json).get("mac", "").asString();
        std::string cpuInfo    = (*json).get("cpu", "").asString();
        if (licensee.empty()) { RESP_ERR(cb, "被授权方不能为空"); return; }

        std::string key = "RUOYI-" + _genKeySegment() + "-" + _genKeySegment() + "-" + _genKeySegment();

        bool ok = DatabaseService::instance().execParams(
            "INSERT INTO sys_license(license_key, licensee, fp_hash, fp_primary, expire_date, "
            "max_users, features, grace_days, machine_id, mac, cpu, status, created_at) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,1,NOW())",
            {key, licensee, fpHash, fpPrimary, expireDate,
             std::to_string(maxUsers), features, std::to_string(graceDays),
             machineId, macAddr, cpuInfo});
        if (!ok) { RESP_ERR(cb, "签发失败"); return; }

        // 生成完整 license.lic 内容（用本地 LicenseManager::generate 7-arg 签名版本）
        std::string licContent = LicenseManager::generate(
            licensee, fpHash, fpPrimary, expireDate, maxUsers, features, graceDays);

        LOG_OPER_PARAM(req, "签发 License: " + licensee, BusinessType::INSERT,
                       "key=" + key + " expire=" + expireDate);

        Json::Value data;
        data["license_key"]     = key;
        data["license_content"] = licContent;
        data["filename"]        = "license.lic";
        Json::Value r = AjaxResult::success(std::string("签发成功"));
        r["data"] = data;
        RESP_JSON(cb, r);
    }

    // ── 删除许可证 ───────────────────────────────────────────────
    void remove(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                std::string id) {
        CHECK_PERM(req, cb, "system:license:remove");
        if (id.empty()) { RESP_ERR(cb, "id 不能为空"); return; }
        bool ok = DatabaseService::instance().execParams(
            "DELETE FROM sys_license WHERE id=$1", {id});
        if (!ok) { RESP_ERR(cb, "删除失败"); return; }
        LOG_OPER(req, "删除 License id=" + id, BusinessType::REMOVE);
        RESP_MSG(cb, "删除成功");
    }

    // ── 重新导出 license.lic（根据已签发记录重建签名文件）──────────
    void exportLic(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                   std::string id) {
        CHECK_PERM(req, cb, "system:license:list");
        if (id.empty()) { RESP_ERR(cb, "id 不能为空"); return; }
        auto res = DatabaseService::instance().queryParams(
            "SELECT licensee, fp_hash, fp_primary, expire_date, max_users, features, "
            "grace_days FROM sys_license WHERE id=$1 LIMIT 1", {id});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "记录不存在"); return; }

        std::string licensee   = res.str(0, 0);
        std::string fpHash     = res.str(0, 1);
        std::string fpPrimary  = res.str(0, 2);
        std::string expireDate = res.str(0, 3).empty() ? "PERPETUAL" : res.str(0, 3);
        int         maxUsers   = res.intVal(0, 4);
        std::string features   = res.str(0, 5).empty() ? "FULL" : res.str(0, 5);
        int         graceDays  = res.intVal(0, 6);

        if (fpHash.empty() || fpPrimary.empty()) {
            RESP_ERR(cb, "该记录缺少硬件指纹（客户端首次 verify 时绑定），无法生成 license.lic");
            return;
        }

        std::string licContent = LicenseManager::generate(
            licensee, fpHash, fpPrimary, expireDate, maxUsers, features, graceDays);

        Json::Value data;
        data["license_content"] = licContent;
        data["filename"]        = "license.lic";
        Json::Value r = AjaxResult::success(std::string("导出成功"));
        r["data"] = data;
        RESP_JSON(cb, r);
    }

    // ── 管理员查看已签发列表 ─────────────────────────────────────
    void list(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:license:list");
        auto res = DatabaseService::instance().query(
            "SELECT id, license_key, licensee, fp_hash, fp_primary, expire_date, "
            "max_users, features, grace_days, status, created_at, last_heartbeat "
            "FROM sys_license ORDER BY id DESC");

        Json::Value rows(Json::arrayValue);
        if (res.ok()) {
            for (int i = 0; i < res.rows(); ++i) {
                Json::Value j;
                j["id"]             = (Json::Int64)res.longVal(i, 0);
                j["license_key"]    = res.str(i, 1);
                j["licensee"]       = res.str(i, 2);
                j["fp_hash"]        = res.str(i, 3);
                j["fp_primary"]     = res.str(i, 4);
                j["expire_date"]    = res.str(i, 5).empty() ? "PERPETUAL" : res.str(i, 5);
                j["max_users"]      = res.intVal(i, 6);
                j["features"]       = res.str(i, 7).empty() ? "FULL" : res.str(i, 7);
                j["grace_days"]     = res.intVal(i, 8);
                j["status"]         = res.intVal(i, 9);
                j["created_at"]     = res.str(i, 10);
                j["last_heartbeat"] = res.str(i, 11);
                rows.append(j);
            }
        }
        RESP_OK(cb, rows);
    }

private:
    static int _daysUntil(const std::string& ds) {
        if (ds == "PERPETUAL") return 99999;
        int y=0, m=0, d=0;
        if (std::sscanf(ds.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return -99999;
        std::tm t = {};
        t.tm_year = y-1900; t.tm_mon = m-1; t.tm_mday = d;
        t.tm_hour = 23; t.tm_min = 59; t.tm_sec = 59;
        return (int)((std::mktime(&t) - std::time(nullptr)) / 86400);
    }

    static std::string _genKeySegment() {
        static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        unsigned char buf[4];
        RAND_bytes(buf, 4);
        std::string seg;
        for (int i = 0; i < 4; ++i) seg += chars[buf[i] % 35];
        return seg;
    }
};
