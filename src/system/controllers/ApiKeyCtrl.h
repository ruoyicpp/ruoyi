/**
 * @file ApiKeyCtrl.h
 * @brief API Key 管理控制器
 * 
 * 功能概述：
 *   - API Key 创建：生成新的 API Key（仅返回一次明文）
 *   - API Key 列表：查看所有 API Key（不显示明文）
 *   - API Key 更新：修改 API Key 的启用状态、名称、过期时间
 *   - API Key 删除：删除 API Key 并失效缓存
 *   - API Key 管理页面：自包含的 HTML 管理界面
 * 
 * API 端点：
 *   - POST   /system/apikey       - 创建 API Key
 *   - GET    /system/apikey/list  - API Key 列表
 *   - PUT    /system/apikey/{id}  - 更新 API Key
 *   - DELETE /system/apikey/{id}  - 删除 API Key
 *   - GET    /system/apikey/page  - 管理页面（HTML）
 * 
 * 权限要求：
 *   - system:apikey:create - 创建 API Key
 *   - system:apikey:list   - 查看 API Key 列表
 *   - system:apikey:edit   - 修改 API Key
 *   - system:apikey:remove - 删除 API Key
 * 
 * 核心特性：
 *   - 安全存储：数据库仅存储 SHA256 哈希，不存储明文
 *   - 一次性返回：创建时仅返回一次明文 key，之后无法查看
 *   - 过期管理：支持设置 API Key 过期时间
 *   - 启用/禁用：支持启用和禁用 API Key，无需删除
 *   - 缓存管理：修改或删除时自动失效缓存
 * 
 * 使用场景：
 *   - 第三方集成：为第三方应用提供 API 访问权限
 *   - 自动化脚本：为自动化脚本提供认证凭证
 *   - 移动应用：为移动应用提供 API 访问权限
 *   - 微服务调用：为微服务之间的调用提供认证
 */

#pragma once
#include <drogon/drogon.h>
#include "../../common/AjaxResult.h"
#include "../../common/ApiKeyService.h"
#include "../../common/PageUtils.h"
#include "../../common/OperLogUtils.h"
#include "../../filters/PermFilter.h"
#include "../../system/services/TokenService.h"

/**
 * @class ApiKeyCtrl
 * @brief API Key 管理控制器
 * 
 * 对应 RuoYi-Vue 中的 ApiKeyController，提供 API Key 管理的所有 API 端点。
 * 所有操作都需要 JWT 认证，并根据权限字符串进行权限检查。
 */
class ApiKeyCtrl : public drogon::HttpController<ApiKeyCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ApiKeyCtrl::create, "/system/apikey",       drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(ApiKeyCtrl::list,   "/system/apikey/list",  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(ApiKeyCtrl::update, "/system/apikey/{id}",  drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(ApiKeyCtrl::remove, "/system/apikey/{id}",  drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(ApiKeyCtrl::page,   "/system/apikey/page",  drogon::Get);   // InnerLink iframe 内嵌 HTML
    METHOD_LIST_END

    // ── GET /system/apikey/page — 自包含 HTML 管理页（无前端依赖）─────────
    void page(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        (void)req;
        std::string html = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head><meta charset="UTF-8"><title>API Key 管理</title>
<style>
  body{font-family:-apple-system,BlinkMacSystemFont,"Helvetica Neue",Arial,sans-serif;
       background:#fff;color:#303133;margin:0;padding:20px;font-size:14px}
  .title{font-size:16px;font-weight:600;color:#303133;margin:0 0 16px;
         padding-bottom:12px;border-bottom:1px solid #ebeef5}
  .toolbar{margin-bottom:12px;display:flex;gap:8px;align-items:center;flex-wrap:wrap}
  input,select{padding:6px 10px;border-radius:2px;border:1px solid #dcdfe6;font-size:13px;color:#606266}
  input:focus,select:focus{outline:none;border-color:#409eff}
  button{padding:6px 14px;border-radius:2px;border:1px solid #dcdfe6;cursor:pointer;
         font-size:13px;background:#fff;color:#606266}
  button:hover{color:#409eff;border-color:#c6e2ff}
  button.primary{background:#409eff;border-color:#409eff;color:#fff}
  button.primary:hover{background:#66b1ff;border-color:#66b1ff;color:#fff}
  button.danger{color:#f56c6c;border-color:#fbc4c4}
  button.danger:hover{background:#fef0f0;border-color:#f56c6c}
  table{width:100%;background:#fff;border-collapse:collapse;font-size:13px;
        border:1px solid #ebeef5}
  th,td{padding:10px 12px;text-align:left;border-bottom:1px solid #ebeef5;color:#606266}
  th{background:#fff;font-weight:500;color:#909399}
  tr:last-child td{border-bottom:none}
  .pill{font-size:12px;color:#606266}
  .on{color:#67c23a}.off{color:#f56c6c}
  .modal-bg{display:none;position:fixed;inset:0;background:rgba(0,0,0,.5);
            align-items:center;justify-content:center;z-index:99}
  .modal-bg.show{display:flex}
  .modal{background:#fff;padding:20px;border-radius:2px;width:480px;max-width:90vw;
         border:1px solid #ebeef5}
  .modal h2{margin:0 0 16px;font-size:16px;font-weight:600;
            padding-bottom:12px;border-bottom:1px solid #ebeef5}
  .modal label{display:block;margin-top:12px;font-size:13px;color:#606266}
  .modal input{width:100%;box-sizing:border-box;margin-top:4px}
  .modal .row{display:flex;gap:8px;margin-top:20px;justify-content:flex-end}
  .keyshow{padding:12px;margin:8px 0;word-break:break-all;font-family:Consolas,monospace;
           font-size:13px;color:#303133;border:1px solid #ebeef5}
  .keyshow strong{color:#f56c6c}
</style></head>
<body>
<div class="title">API Key 管理</div>
<div class="toolbar">
  <input id="qName" placeholder="按名称搜索" style="width:200px">
  <button class="primary" onclick="load()">查询</button>
  <button onclick="openCreate()">创建 Key</button>
</div>
<table>
  <thead><tr><th>ID</th><th>名称</th><th>前缀</th><th>用户ID</th><th>启用</th><th>过期</th><th>最近使用</th><th>创建人</th><th>操作</th></tr></thead>
  <tbody id="tb"><tr><td colspan="9" style="text-align:center;padding:30px">加载中…</td></tr></tbody>
</table>

<!-- 创建弹窗 -->
<div class="modal-bg" id="mCreate"><div class="modal">
  <h2>创建 API Key</h2>
  <label>名称 *<input id="fName"></label>
  <label>绑定用户 ID *（继承其权限） <input id="fUserId" type="number"></label>
  <label>过期时间（可选，YYYY-MM-DD HH:MM:SS）<input id="fExpire" placeholder="留空=永不过期"></label>
  <label>备注 <input id="fRemark"></label>
  <div class="row"><button onclick="close_('mCreate')">取消</button><button class="primary" onclick="doCreate()">创建</button></div>
</div></div>

<!-- key 显示 -->
<div class="modal-bg" id="mKey"><div class="modal">
  <h2>仅本次显示</h2>
  <div class="keyshow"><strong>请立即复制保存：</strong><br><span id="newKey"></span></div>
  <p style="font-size:13px;color:#909399;margin:8px 0">关闭后将无法再次查看此 Key（DB 仅存 SHA-256 哈希）。</p>
  <div class="row"><button onclick="close_('mKey')">关闭</button><button class="primary" onclick="copyKey()">复制</button></div>
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
const API = location.pathname.replace(/\/[^/]*$/, '');   // 去掉末尾 /page

async function load() {
  const name = document.getElementById('qName').value;
  const r = await fetch(API + '/list?pageNum=1&pageSize=50&name=' + encodeURIComponent(name), { headers: H });
  const d = await r.json();
  const tb = document.getElementById('tb');
  if (d.code !== 200 || !d.rows) { tb.innerHTML = '<tr><td colspan="9">'+(d.msg||'加载失败')+'</td></tr>'; return; }
  if (d.rows.length === 0) { tb.innerHTML = '<tr><td colspan="9" style="text-align:center;padding:30px">无数据</td></tr>'; return; }
  tb.innerHTML = d.rows.map(it => `<tr>
    <td>${it.id}</td><td>${esc(it.name)}</td>
    <td><code>${esc(it.keyPrefix)}***</code></td>
    <td>${it.userId}</td>
    <td><span class="pill ${it.enabled?'on':'off'}">${it.enabled?'启用':'禁用'}</span></td>
    <td>${it.expireTime||'永不过期'}</td>
    <td>${it.lastUsedAt||'-'}</td>
    <td>${esc(it.createBy)}</td>
    <td>
      <button onclick="toggle(${it.id}, '${esc(it.name)}', ${it.enabled?0:1})">${it.enabled?'禁用':'启用'}</button>
      <button class="danger" onclick="del(${it.id})">删除</button>
    </td></tr>`).join('');
}

function openCreate(){ document.getElementById('mCreate').classList.add('show'); }
function close_(id){ document.getElementById(id).classList.remove('show'); }
function esc(s){ return String(s||'').replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }

async function doCreate() {
  const body = {
    name: document.getElementById('fName').value.trim(),
    userId: parseInt(document.getElementById('fUserId').value),
    expireTime: document.getElementById('fExpire').value.trim(),
    remark: document.getElementById('fRemark').value.trim()
  };
  if (!body.name || !body.userId) { alert('名称和用户ID必填'); return; }
  const r = await fetch(API, { method:'POST', headers:H, body:JSON.stringify(body) });
  const d = await r.json();
  if (d.code !== 200) { alert(d.msg||'失败'); return; }
  close_('mCreate');
  document.getElementById('newKey').textContent = d.data.key;
  document.getElementById('mKey').classList.add('show');
  load();
}

function copyKey() { navigator.clipboard.writeText(document.getElementById('newKey').textContent); }

async function toggle(id, name, enabled) {
  if (!confirm((enabled?'启用':'禁用')+' Key "'+name+'"？')) return;
  const r = await fetch(API + '/' + id, { method:'PUT', headers:H, body: JSON.stringify({enabled, name}) });
  const d = await r.json(); if (d.code !== 200) alert(d.msg||'失败'); else load();
}

async function del(id) {
  if (!confirm('删除此 Key？关联的脚本/CI 将无法继续访问')) return;
  const r = await fetch(API + '/' + id, { method:'DELETE', headers:H });
  const d = await r.json(); if (d.code !== 200) alert(d.msg||'失败'); else load();
}

if (!token) document.getElementById('tb').innerHTML = '<tr><td colspan="9" style="text-align:center;padding:30px;color:#cf1322">未获取到 Token，请通过菜单打开或加 ?token=xxx</td></tr>';
else load();
</script>
</body></html>)HTML";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(html);
        cb(resp);
    }

    // ── POST /system/apikey ───────────────────────────────────────
    void create(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:apikey:add");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }

        std::string name    = (*json).get("name", "").asString();
        long        userId  = (*json).get("userId", 0).asInt64();
        std::string expire  = (*json).get("expireTime", "").asString();   // YYYY-MM-DD HH:MM:SS
        std::string remark  = (*json).get("remark", "").asString();

        if (name.empty()) { RESP_ERR(cb, "name 不能为空"); return; }
        if (userId <= 0)  { RESP_ERR(cb, "userId 必须 > 0"); return; }

        // 校验关联用户存在
        auto& db = DatabaseService::instance();
        auto uRes = db.queryParams(
            "SELECT user_id FROM sys_user WHERE user_id=$1",
            {std::to_string(userId)});
        if (!uRes.ok() || uRes.rows() == 0) { RESP_ERR(cb, "关联用户不存在"); return; }

        std::string key     = ApiKeyService::generateKey();
        std::string keyHash = ApiKeyService::hashKey(key);
        std::string prefix  = ApiKeyService::keyPrefix(key);

        auto curUser = TokenService::instance().getLoginUser(req);
        std::string creator = curUser ? curUser->userName : "system";

        bool ok;
        if (expire.empty()) {
            ok = db.execParams(
                "INSERT INTO sys_apikey(name,key_hash,key_prefix,user_id,enabled,create_by,remark) "
                "VALUES($1,$2,$3,$4,1,$5,$6)",
                {name, keyHash, prefix, std::to_string(userId), creator, remark});
        } else {
            ok = db.execParams(
                "INSERT INTO sys_apikey(name,key_hash,key_prefix,user_id,expire_time,enabled,create_by,remark) "
                "VALUES($1,$2,$3,$4,$5,1,$6,$7)",
                {name, keyHash, prefix, std::to_string(userId), expire, creator, remark});
        }
        if (!ok) { RESP_ERR(cb, "创建失败"); return; }

        LOG_OPER_PARAM(req, "创建 API Key: " + name, BusinessType::INSERT, "userId=" + std::to_string(userId));

        Json::Value r = AjaxResult::success();
        r["data"] = Json::Value(Json::objectValue);
        r["data"]["key"]       = key;        // ★ 仅此一次返回明文 ★
        r["data"]["keyPrefix"] = prefix;
        r["data"]["name"]      = name;
        r["msg"] = "请妥善保管，此 key 仅本次返回，无法找回";
        RESP_JSON(cb, r);
    }

    // ── GET /system/apikey/list ───────────────────────────────────
    void list(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:apikey:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto name = req->getParameter("name");

        std::string sql = "SELECT id,name,key_prefix,user_id,expire_time,enabled,last_used_at,create_by,create_time,remark "
                          "FROM sys_apikey WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        if (!name.empty()) { sql += " AND name LIKE $" + std::to_string(idx++); params.push_back("%" + name + "%"); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY id DESC LIMIT " + std::to_string(page.pageSize)
             + " OFFSET " + std::to_string(page.offset());
        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);

        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]          = (Json::Int64)res.longVal(i, 0);
            j["name"]        = res.str(i, 1);
            j["keyPrefix"]   = res.str(i, 2);    // 如 "ak_a1b2c3d4"
            j["userId"]      = (Json::Int64)res.longVal(i, 3);
            j["expireTime"]  = res.str(i, 4);
            j["enabled"]     = res.intVal(i, 5);
            j["lastUsedAt"]  = res.str(i, 6);
            j["createBy"]    = res.str(i, 7);
            j["createTime"]  = res.str(i, 8);
            j["remark"]      = res.str(i, 9);
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    // ── PUT /system/apikey/{id} ───────────────────────────────────
    void update(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        CHECK_PERM(req, cb, "system:apikey:edit");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }

        std::string name    = (*json).get("name", "").asString();
        std::string remark  = (*json).get("remark", "").asString();
        int         enabled = (*json).get("enabled", 1).asInt();
        std::string expire  = (*json).get("expireTime", "").asString();

        auto& db = DatabaseService::instance();
        bool ok;
        if (expire.empty()) {
            ok = db.execParams(
                "UPDATE sys_apikey SET name=$1, remark=$2, enabled=$3 WHERE id=$4",
                {name, remark, std::to_string(enabled), std::to_string(id)});
        } else {
            ok = db.execParams(
                "UPDATE sys_apikey SET name=$1, remark=$2, enabled=$3, expire_time=$4 WHERE id=$5",
                {name, remark, std::to_string(enabled), expire, std::to_string(id)});
        }
        if (!ok) { RESP_ERR(cb, "更新失败"); return; }

        // 失效全部缓存（不知道哪个 key 对应此 id，保守清空）
        ApiKeyService::instance().invalidateAll();
        LOG_OPER(req, "更新 API Key id=" + std::to_string(id), BusinessType::UPDATE);
        Json::Value r = AjaxResult::success();
        RESP_JSON(cb, r);
    }

    // ── DELETE /system/apikey/{id} ────────────────────────────────
    void remove(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        CHECK_PERM(req, cb, "system:apikey:remove");
        auto& db = DatabaseService::instance();
        bool ok = db.execParams("DELETE FROM sys_apikey WHERE id=$1", {std::to_string(id)});
        if (!ok) { RESP_ERR(cb, "删除失败"); return; }

        ApiKeyService::instance().invalidateAll();
        LOG_OPER(req, "删除 API Key id=" + std::to_string(id), BusinessType::REMOVE);
        Json::Value r = AjaxResult::success();
        RESP_JSON(cb, r);
    }
};
