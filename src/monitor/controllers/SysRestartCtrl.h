#pragma once
#include <drogon/drogon.h>
#include <thread>
#include <chrono>
#include "../../common/AjaxResult.h"
#include "../../common/TokenCache.h"
#include "../../common/SecurityUtils.h"
#include "../../system/services/TokenService.h"

// 系统重启管理
// GET  /monitor/restart/online   — 当前在线用户数（JSON）
// GET  /monitor/restart/page     — 内嵌 HTML 管理页面（iframe 菜单嵌入）
// POST /monitor/restart/confirm  — 二次确认后触发优雅停机
//
// 实际"重启"依赖外部 wrapper：
//   * Windows Service: SERVICE_FAILURE_RESTART
//   * systemd:         Restart=on-failure
//   * Docker:          restart: unless-stopped
//   * pm2 / supervisor:自动拉起
// 进程仅负责 drogon::app().quit() 优雅退出；如无 wrapper 则只是关停。
class SysRestartCtrl : public drogon::HttpController<SysRestartCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysRestartCtrl::online,  "/monitor/restart/online",  drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysRestartCtrl::page,    "/monitor/restart/page",    drogon::Get);
        ADD_METHOD_TO(SysRestartCtrl::confirm, "/monitor/restart/confirm", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    // ── 在线人数 ─────────────────────────────────────────────────────────
    void online(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        if (!SecurityUtils::isAdmin(user->userId)) {
            Json::Value r = AjaxResult::error("仅管理员可操作");
            RESP_JSON(cb, r);
            return;
        }
        Json::Value r = AjaxResult::success();
        r["online"] = (Json::Int64)TokenCache::instance().size();
        RESP_JSON(cb, r);
    }

    // ── 二次确认 + 触发重启 ─────────────────────────────────────────────
    void confirm(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        if (!SecurityUtils::isAdmin(user->userId)) {
            Json::Value r = AjaxResult::error("仅管理员可操作");
            RESP_JSON(cb, r);
            return;
        }

        // 必须显式确认（防误操作）
        auto json = req->getJsonObject();
        if (!json || !(*json).get("confirmed", false).asBool()) {
            RESP_ERR(cb, "需要二次确认（confirmed=true）"); return;
        }

        LOG_WARN << "[Restart] 管理员 " << user->userName << "(id=" << user->userId
                 << ") 触发了重启，当前在线 " << TokenCache::instance().size() << " 人";

        Json::Value r = AjaxResult::success();
        r["msg"] = "shutdown initiated; relying on external watchdog to restart";
        RESP_JSON(cb, r);

        // 500ms 后异步退出，确保 HTTP 响应能完整发出
        std::thread([]{
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            drogon::app().quit();
        }).detach();
    }

    // ── 内嵌 HTML 管理页 ────────────────────────────────────────────────
    void page(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        (void)req;
        std::string html = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head><meta charset="UTF-8"><title>系统重启</title>
<style>
  body{font-family:'Segoe UI',Arial,sans-serif;background:#f4f6f9;color:#333;
       margin:0;padding:24px;display:flex;justify-content:center}
  .card{max-width:640px;width:100%;background:#fff;border-radius:8px;
        box-shadow:0 2px 12px rgba(0,0,0,0.08);padding:32px}
  h1{margin-top:0;color:#cf1322;font-size:20px}
  .warn{background:#fff3cd;border-left:4px solid #ffc107;padding:12px 16px;
        margin:16px 0;border-radius:4px;font-size:14px}
  .danger{background:#fff1f0;border-left:4px solid #cf1322;padding:12px 16px;
          margin:16px 0;border-radius:4px;font-size:14px}
  .stat{background:#e6f4ff;border-left:4px solid #1890ff;padding:12px 16px;
        margin:16px 0;border-radius:4px;font-size:14px}
  .num{font-size:28px;font-weight:bold;color:#1890ff}
  .row{display:flex;gap:12px;margin-top:24px}
  button{padding:10px 20px;border-radius:4px;border:none;cursor:pointer;
         font-size:14px;flex:1}
  .btn-refresh{background:#1890ff;color:#fff}
  .btn-refresh:hover{background:#40a9ff}
  .btn-restart{background:#cf1322;color:#fff}
  .btn-restart:hover{background:#ff4d4f}
  .btn-restart:disabled{background:#d9d9d9;cursor:not-allowed}
  #status{margin-top:16px;padding:10px;border-radius:4px;display:none}
  #status.ok{background:#f6ffed;color:#52c41a;display:block}
  #status.err{background:#fff1f0;color:#cf1322;display:block}
</style>
</head>
<body>
<div class="card">
  <h1>⚠️ 系统重启管理</h1>

  <div class="warn">
    <strong>请注意：</strong>重启会让所有在线用户掉线，正在进行的请求会被中断。
    建议在业务低峰期操作，或与值班人员协调后再点击。
  </div>

  <div class="stat">
    <div>当前在线用户：</div>
    <div class="num" id="onlineCount">--</div>
  </div>

  <div class="danger">
    <strong>⚠️ 不可恢复：</strong>实际重启依赖外部进程守护（Windows Service / systemd / Docker / pm2）。
    如果未配置守护进程，本操作会让服务<strong>关停而不会自动启动</strong>。
  </div>

  <div class="row">
    <button class="btn-refresh" onclick="refresh()">🔄 刷新在线人数</button>
    <button class="btn-restart" id="restartBtn" onclick="doRestart()">⚡ 立即重启</button>
  </div>

  <div id="status"></div>
</div>

<script>
// token 提取：URL → parent.sessionStorage → 当前 sessionStorage
let token = '';
try {
  const u = new URL(window.location.href);
  token = u.searchParams.get('token') || '';
  if (!token && window.parent !== window) {
    try { token = window.parent.sessionStorage.getItem('Admin-Token') || ''; } catch(e){}
  }
  if (!token) token = sessionStorage.getItem('Admin-Token') || '';
} catch(e){}

const headers = { 'Authorization': 'Bearer ' + token, 'Content-Type': 'application/json' };
const apiBase = window.location.pathname.replace(/\/[^/]*$/, '');

async function refresh() {
  try {
    const r = await fetch(apiBase + '/online', { headers });
    const d = await r.json();
    if (d.code !== 200) { showStatus('err', d.msg || '未授权'); return; }
    document.getElementById('onlineCount').textContent = d.online;
    showStatus('', '');
  } catch(e) { showStatus('err', '网络错误：' + e.message); }
}

async function doRestart() {
  const online = document.getElementById('onlineCount').textContent;
  if (!confirm('再次确认：将关停后端服务，当前在线 ' + online + ' 人。继续？')) return;
  if (!confirm('最后一次确认：你确定要关停服务吗？此操作不可撤销。')) return;

  document.getElementById('restartBtn').disabled = true;
  try {
    const r = await fetch(apiBase + '/confirm', {
      method: 'POST',
      headers,
      body: JSON.stringify({ confirmed: true })
    });
    const d = await r.json();
    if (d.code === 200) {
      showStatus('ok', '✅ ' + (d.msg || '已发送停机指令') + '。如已配置守护进程，5~10 秒后会自动拉起。');
    } else {
      showStatus('err', d.msg || '操作失败');
      document.getElementById('restartBtn').disabled = false;
    }
  } catch(e) {
    showStatus('err', '请求失败：' + e.message);
    document.getElementById('restartBtn').disabled = false;
  }
}

function showStatus(level, msg) {
  const el = document.getElementById('status');
  el.className = level;
  el.textContent = msg;
}

if (!token) {
  showStatus('err', '未获取到登录凭证。请通过菜单栏「系统重启」打开（自动注入 token），或在 URL 末尾加 ?token=xxx');
  document.getElementById('restartBtn').disabled = true;
} else {
  refresh();
}
</script>
</body></html>)HTML";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(html);
        cb(resp);
    }
};
