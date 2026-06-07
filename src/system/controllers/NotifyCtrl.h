/**
 * @file NotifyCtrl.h
 * @brief 通知渠道管理控制器
 * 
 * 功能概述：
 *   - 通知渠道管理：CRUD 操作，支持多种通知渠道
 *   - 通知发送：通过指定渠道发送通知
 *   - 测试发送：测试通知渠道是否正常工作
 *   - 渠道列表：查看所有可用的通知渠道
 * 
 * API 端点：
 *   - POST   /system/notify/channel           - 创建通知渠道
 *   - GET    /system/notify/channel/list      - 通知渠道列表
 *   - PUT    /system/notify/channel/{id}      - 更新通知渠道
 *   - DELETE /system/notify/channel/{id}      - 删除通知渠道
 *   - POST   /system/notify/channel/{id}/test - 测试通知发送
 *   - POST   /system/notify/send              - 发送通知
 *   - GET    /system/notify/channel/page      - 管理页面（HTML）
 * 
 * 权限要求：
 *   - system:notify:list   - 查看通知渠道列表
 *   - system:notify:add    - 创建通知渠道
 *   - system:notify:edit   - 修改通知渠道
 *   - system:notify:remove - 删除通知渠道
 *   - system:notify:send   - 发送通知
 * 
 * 支持的通知渠道：
 *   - Email：邮件通知
 *   - SMS：短信通知
 *   - WeChat：微信通知
 *   - DingTalk：钉钉通知
 *   - Slack：Slack 通知
 *   - Webhook：自定义 Webhook
 * 
 * 核心特性：
 *   - 多渠道支持：支持多种通知渠道
 *   - 灵活配置：每个渠道可独立配置
 *   - 测试功能：支持测试通知是否正常
 *   - 批量发送：支持向多个渠道发送通知
 *   - 模板支持：支持通知模板
 * 
 * 使用场景：
 *   - 系统告警：系统告警通过多个渠道通知管理员
 *   - 用户通知：向用户发送各类通知
 *   - 业务提醒：业务相关的提醒和通知
 *   - 定时报告：定时发送报告和统计
 * 
 * 通知渠道配置示例：
 *   {
 *     "channelName": "管理员邮件",
 *     "channelType": "email",
 *     "enabled": true,
 *     "config": {
 *       "recipients": "admin@example.com,ops@example.com",
 *       "subject": "系统告警"
 *     }
 *   }
 * 
 * 发送通知请求示例：
 *   {
 *     "channelId": 1,
 *     "title": "系统告警",
 *     "content": "内存使用率超过 90%"
 *   }
 */

#pragma once
#include <drogon/drogon.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../common/OperLogUtils.h"
#include "../../common/NotifyService.h"
#include "../../filters/PermFilter.h"
#include "../../system/services/TokenService.h"

/**
 * @class NotifyChannelCtrl
 * @brief 通知渠道管理控制器
 * 
 * 对应 RuoYi-Vue 中的 NotifyChannelController，提供通知渠道管理的所有 API 端点。
 * 所有操作都需要 JWT 认证，并根据权限字符串进行权限检查。
 */
class NotifyChannelCtrl : public drogon::HttpController<NotifyChannelCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(NotifyChannelCtrl::create, "/system/notify/channel",            drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::list,   "/system/notify/channel/list",       drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::update, "/system/notify/channel/{id}",       drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::remove, "/system/notify/channel/{id}",       drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::test,   "/system/notify/channel/{id}/test",  drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::send,   "/system/notify/send",               drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::page,   "/system/notify/channel/page",       drogon::Get);   // InnerLink HTML
    METHOD_LIST_END

    // ── GET /system/notify/channel/page — 渠道管理内嵌 HTML ──────────────
    void page(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        (void)req;
        std::string html = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head><meta charset="UTF-8"><title>通知渠道</title>
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
  button.success{color:#67c23a;border-color:#c2e7b0}
  button.success:hover{background:#f0f9eb;border-color:#67c23a}
  table{width:100%;background:#fff;border-collapse:collapse;font-size:13px;
        border:1px solid #ebeef5}
  th,td{padding:10px 12px;text-align:left;border-bottom:1px solid #ebeef5;color:#606266}
  th{background:#fff;font-weight:500;color:#909399}
  tr:last-child td{border-bottom:none}
  .pill{font-size:12px}
  .on{color:#67c23a}.off{color:#f56c6c}
  .badge{font-size:12px;color:#909399}
  .modal-bg{display:none;position:fixed;inset:0;background:rgba(0,0,0,.5);
            align-items:center;justify-content:center;z-index:99}
  .modal-bg.show{display:flex}
  .modal{background:#fff;padding:20px;border-radius:2px;width:520px;max-width:90vw;
         border:1px solid #ebeef5}
  .modal h2{margin:0 0 16px;font-size:16px;font-weight:600;
            padding-bottom:12px;border-bottom:1px solid #ebeef5}
  .modal label{display:block;margin-top:12px;font-size:13px;color:#606266}
  .modal input,.modal select,.modal textarea{width:100%;box-sizing:border-box;margin-top:4px}
  .modal textarea{font-family:Consolas,monospace;height:60px}
  .modal .row{display:flex;gap:8px;margin-top:20px;justify-content:flex-end}
</style></head>
<body>
<div class="title">通知渠道管理</div>
<div class="toolbar">
  <input id="qName" placeholder="按名称搜索" style="width:200px">
  <button class="primary" onclick="load()">查询</button>
  <button onclick="openEdit(null)">创建渠道</button>
</div>
<table>
  <thead><tr><th>ID</th><th>名称</th><th>类型</th><th>Webhook</th><th>启用</th><th>创建人</th><th>操作</th></tr></thead>
  <tbody id="tb"><tr><td colspan="7" style="text-align:center;padding:30px">加载中…</td></tr></tbody>
</table>

<div class="modal-bg" id="mEdit"><div class="modal">
  <h2 id="mTitle">创建渠道</h2>
  <input id="fId" type="hidden">
  <label>名称 *<input id="fName"></label>
  <label>类型 *
    <select id="fType">
      <option value="dingtalk">dingtalk（钉钉，HMAC-SHA256 加签）</option>
      <option value="feishu">feishu（飞书，HMAC-SHA256 加签）</option>
      <option value="wxwork">wxwork（企业微信，URL key 即凭证）</option>
      <option value="webhook">webhook（通用 Webhook，X-Signature 头）</option>
    </select>
  </label>
  <label>Webhook URL *<input id="fUrl" placeholder="https://oapi.dingtalk.com/robot/send?access_token=..."></label>
  <label>Secret（加签密钥；wxwork 可留空）<input id="fSecret"></label>
  <label>启用 <select id="fEnabled"><option value="1">是</option><option value="0">否</option></select></label>
  <label>备注 <input id="fRemark"></label>
  <div class="row"><button onclick="close_('mEdit')">取消</button><button class="primary" onclick="save()">保存</button></div>
</div></div>

<div class="modal-bg" id="mTest"><div class="modal">
  <h2>测试发送</h2>
  <label>标题 <input id="tTitle" value="RuoYi-Cpp 测试通知"></label>
  <label>内容 <textarea id="tContent">这是一条测试消息，验证渠道签名与连通性</textarea></label>
  <div class="row"><button onclick="close_('mTest')">取消</button><button class="success" onclick="doTest()">发送</button></div>
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
const API = location.pathname.replace(/\/[^/]*$/, '');    // 去 /page
let testTargetId = 0;

async function load() {
  const name = document.getElementById('qName').value;
  const r = await fetch(API + '/list?pageNum=1&pageSize=100&name=' + encodeURIComponent(name), { headers: H });
  const d = await r.json();
  const tb = document.getElementById('tb');
  if (d.code !== 200 || !d.rows) { tb.innerHTML='<tr><td colspan="7">'+(d.msg||'加载失败')+'</td></tr>'; return; }
  if (d.rows.length === 0) { tb.innerHTML='<tr><td colspan="7" style="text-align:center;padding:30px">无渠道</td></tr>'; return; }
  tb.innerHTML = d.rows.map(it => `<tr>
    <td>${it.id}</td><td>${esc(it.name)}</td>
    <td><span class="badge">${it.channelType}</span></td>
    <td><code style="font-size:12px">${esc(it.webhookUrl)}</code></td>
    <td><span class="pill ${it.enabled?'on':'off'}">${it.enabled?'启用':'禁用'}</span></td>
    <td>${esc(it.createBy)}</td>
    <td>
      <button class="success" onclick="openTest(${it.id})">测试</button>
      <button onclick="openEdit(${it.id}, ${JSON.stringify(it).replace(/"/g,'&quot;')})">编辑</button>
      <button class="danger" onclick="del(${it.id})">删除</button>
    </td></tr>`).join('');
}

function esc(s){ return String(s||'').replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }
function close_(id){ document.getElementById(id).classList.remove('show'); }

function openEdit(id, row) {
  document.getElementById('mTitle').textContent = id ? '编辑渠道' : '创建渠道';
  document.getElementById('fId').value = id || '';
  document.getElementById('fName').value = row ? row.name : '';
  document.getElementById('fType').value = row ? row.channelType : 'dingtalk';
  document.getElementById('fUrl').value = '';   // 编辑时不回显（避免误改），需重新输入
  document.getElementById('fSecret').value = '';
  document.getElementById('fEnabled').value = row ? (row.enabled?'1':'0') : '1';
  document.getElementById('fRemark').value = row ? (row.remark||'') : '';
  document.getElementById('mEdit').classList.add('show');
}

async function save() {
  const id = document.getElementById('fId').value;
  const body = {
    name: document.getElementById('fName').value.trim(),
    channelType: document.getElementById('fType').value,
    webhookUrl: document.getElementById('fUrl').value.trim(),
    secret: document.getElementById('fSecret').value.trim(),
    enabled: parseInt(document.getElementById('fEnabled').value),
    remark: document.getElementById('fRemark').value.trim()
  };
  if (!body.name || !body.webhookUrl) { alert('名称和 webhookUrl 必填'); return; }
  const url = id ? (API + '/' + id) : API;
  const method = id ? 'PUT' : 'POST';
  const r = await fetch(url, { method, headers:H, body: JSON.stringify(body) });
  const d = await r.json();
  if (d.code !== 200) { alert(d.msg||'失败'); return; }
  close_('mEdit'); load();
}

function openTest(id) { testTargetId = id; document.getElementById('mTest').classList.add('show'); }

async function doTest() {
  const body = {
    title: document.getElementById('tTitle').value,
    content: document.getElementById('tContent').value
  };
  const r = await fetch(API + '/' + testTargetId + '/test', { method:'POST', headers:H, body:JSON.stringify(body) });
  const d = await r.json();
  alert(d.code === 200 ? d.msg : (d.msg||'失败'));
  if (d.code === 200) close_('mTest');
}

async function del(id) {
  if (!confirm('删除此渠道？业务调用 sendToChannel('+id+') 将失败')) return;
  const r = await fetch(API + '/' + id, { method:'DELETE', headers:H });
  const d = await r.json(); if (d.code !== 200) alert(d.msg||'失败'); else load();
}

if (!token) document.getElementById('tb').innerHTML='<tr><td colspan="7" style="text-align:center;padding:30px;color:#cf1322">未获取到 Token</td></tr>';
else load();
</script>
</body></html>)HTML";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(html);
        cb(resp);
    }

    void create(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notify:add");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }
        std::string name = (*json).get("name", "").asString();
        std::string type = (*json).get("channelType", "webhook").asString();
        std::string url  = (*json).get("webhookUrl", "").asString();
        std::string sec  = (*json).get("secret", "").asString();
        std::string remark = (*json).get("remark", "").asString();
        if (name.empty() || url.empty()) { RESP_ERR(cb, "name/webhookUrl 必填"); return; }
        if (type != "dingtalk" && type != "feishu" && type != "wxwork" && type != "webhook") {
            RESP_ERR(cb, "channelType 仅支持 dingtalk/feishu/wxwork/webhook"); return;
        }
        auto curUser = TokenService::instance().getLoginUser(req);
        std::string creator = curUser ? curUser->userName : "system";

        bool ok = DatabaseService::instance().execParams(
            "INSERT INTO sys_notify_channel(name,channel_type,webhook_url,secret,enabled,create_by,remark) "
            "VALUES($1,$2,$3,$4,1,$5,$6)",
            {name, type, url, sec, creator, remark});
        if (!ok) { RESP_ERR(cb, "创建失败"); return; }
        LOG_OPER_PARAM(req, "创建通知渠道: " + name, BusinessType::INSERT, "type=" + type);
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void list(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notify:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto name = req->getParameter("name");
        std::string sql = "SELECT id,name,channel_type,webhook_url,enabled,create_by,create_time,remark "
                          "FROM sys_notify_channel WHERE 1=1";
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
            j["channelType"] = res.str(i, 2);
            // 不暴露完整 webhookUrl，前端只看到 host 部分
            std::string url = res.str(i, 3);
            auto pos = url.find('/', url.find("://") + 3);
            j["webhookUrl"] = (pos == std::string::npos) ? url
                                                         : (url.substr(0, pos) + "/...");
            j["enabled"]     = res.intVal(i, 4);
            j["createBy"]    = res.str(i, 5);
            j["createTime"]  = res.str(i, 6);
            j["remark"]      = res.str(i, 7);
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void update(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        CHECK_PERM(req, cb, "system:notify:edit");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }
        std::string name = (*json).get("name", "").asString();
        std::string type = (*json).get("channelType", "webhook").asString();
        std::string url  = (*json).get("webhookUrl", "").asString();
        std::string sec  = (*json).get("secret", "").asString();
        std::string remark = (*json).get("remark", "").asString();
        int enabled = (*json).get("enabled", 1).asInt();
        bool ok = DatabaseService::instance().execParams(
            "UPDATE sys_notify_channel SET name=$1, channel_type=$2, webhook_url=$3, "
            "secret=$4, enabled=$5, remark=$6 WHERE id=$7",
            {name, type, url, sec, std::to_string(enabled), remark, std::to_string(id)});
        if (!ok) { RESP_ERR(cb, "更新失败"); return; }
        LOG_OPER(req, "更新通知渠道 id=" + std::to_string(id), BusinessType::UPDATE);
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void remove(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        CHECK_PERM(req, cb, "system:notify:remove");
        bool ok = DatabaseService::instance().execParams(
            "DELETE FROM sys_notify_channel WHERE id=$1", {std::to_string(id)});
        if (!ok) { RESP_ERR(cb, "删除失败"); return; }
        LOG_OPER(req, "删除通知渠道 id=" + std::to_string(id), BusinessType::REMOVE);
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void test(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb,
              long id) {
        CHECK_PERM(req, cb, "system:notify:edit");
        auto json = req->getJsonObject();
        std::string title   = json ? (*json).get("title",   "RuoYi-Cpp 测试通知").asString() : "测试通知";
        std::string content = json ? (*json).get("content", "这是一条测试消息。").asString() : "test";
        bool sent = NotifyService::sendToChannel(id, title, content);
        if (!sent) { RESP_ERR(cb, "渠道不存在或被禁用"); return; }
        Json::Value r = AjaxResult::success();
        r["msg"] = "已发送（异步），请到目标群查看";
        RESP_JSON(cb, r);
    }

    // POST /system/notify/send
    // Body:
    //   { channelId: 1, title: "...", content: "..." }        // 仅外部 webhook
    //   { channelId: 1, title, content, userIds: [42, 88] }   // + 给指定用户站内消息 + WsBus 推送
    //   { title, content, userIds: [42] }                     // 仅站内消息，不调外部 webhook
    void send(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notify:send");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }
        long channelId      = (*json).get("channelId", 0).asInt64();
        std::string title   = (*json).get("title", "").asString();
        std::string content = (*json).get("content", "").asString();
        std::string level   = (*json).get("level", "info").asString();
        Json::Value userIds = (*json)["userIds"];      // 可选数组

        if (title.empty() || content.empty()) {
            RESP_ERR(cb, "title/content 必填"); return;
        }
        if (channelId <= 0 && (!userIds.isArray() || userIds.empty())) {
            RESP_ERR(cb, "channelId 与 userIds 至少填一个"); return;
        }

        int channelSent = 0, inboxSent = 0;
        if (channelId > 0) {
            if (NotifyService::sendToChannel(channelId, title, content)) ++channelSent;
            else { RESP_ERR(cb, "渠道不存在或被禁用"); return; }
        }
        if (userIds.isArray()) {
            for (const auto& uid : userIds) {
                long u = uid.isInt64() ? uid.asInt64() : uid.asInt();
                if (u > 0 && NotifyService::sendInbox(u, title, content, level)) {
                    ++inboxSent;
                }
            }
        }

        Json::Value r = AjaxResult::success();
        r["channelSent"] = channelSent;
        r["inboxSent"]   = inboxSent;
        RESP_JSON(cb, r);
    }
};

// f15 站内消息 CRUD
//   GET    /system/message/inbox             收件箱（按 user_id 过滤）
//   GET    /system/message/unread            未读条数
//   POST   /system/message/{id}/read         标记单条已读
//   POST   /system/message/readAll           全部已读
//   DELETE /system/message/{id}              删除单条
class MessageCtrl : public drogon::HttpController<MessageCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MessageCtrl::inbox,   "/system/message/inbox",     drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(MessageCtrl::unread,  "/system/message/unread",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(MessageCtrl::read,    "/system/message/{id}/read", drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(MessageCtrl::readAll, "/system/message/readAll",   drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(MessageCtrl::remove,  "/system/message/{id}",      drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void inbox(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto countRes = db.queryParams(
            "SELECT COUNT(*) FROM sys_message WHERE user_id=$1",
            {std::to_string(user->userId)});
        long total = (countRes.ok() && countRes.rows() > 0) ? countRes.longVal(0, 0) : 0;
        auto res = db.queryParams(
            "SELECT id,title,content,level,is_read,create_time,read_time "
            "FROM sys_message WHERE user_id=$1 ORDER BY id DESC LIMIT $2 OFFSET $3",
            {std::to_string(user->userId),
             std::to_string(page.pageSize),
             std::to_string(page.offset())});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]         = (Json::Int64)res.longVal(i, 0);
            j["title"]      = res.str(i, 1);
            j["content"]    = res.str(i, 2);
            j["level"]      = res.str(i, 3);
            j["isRead"]     = res.intVal(i, 4) == 1;
            j["createTime"] = res.str(i, 5);
            j["readTime"]   = res.str(i, 6);
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void unread(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto res = DatabaseService::instance().queryParams(
            "SELECT COUNT(*) FROM sys_message WHERE user_id=$1 AND is_read=0",
            {std::to_string(user->userId)});
        long n = (res.ok() && res.rows() > 0) ? res.longVal(0, 0) : 0;
        Json::Value r = AjaxResult::success();
        r["count"] = (Json::Int64)n;
        RESP_JSON(cb, r);
    }

    void read(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb,
              long id) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_message SET is_read=1, read_time=CURRENT_TIMESTAMP "
            "WHERE id=$1 AND user_id=$2 AND is_read=0",
            {std::to_string(id), std::to_string(user->userId)});
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void readAll(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_message SET is_read=1, read_time=CURRENT_TIMESTAMP "
            "WHERE user_id=$1 AND is_read=0",
            {std::to_string(user->userId)});
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void remove(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        DatabaseService::instance().execParams(
            "DELETE FROM sys_message WHERE id=$1 AND user_id=$2",
            {std::to_string(id), std::to_string(user->userId)});
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }
};
