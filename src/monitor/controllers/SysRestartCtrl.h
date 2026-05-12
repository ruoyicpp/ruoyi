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
  body{font-family:-apple-system,BlinkMacSystemFont,"Helvetica Neue",Arial,sans-serif;
       background:#fff;color:#303133;margin:0;padding:20px;font-size:14px}
  .card{max-width:720px;background:#fff;border:1px solid #ebeef5;border-radius:2px;padding:20px}
  .title{font-size:16px;font-weight:600;color:#303133;margin:0 0 16px;
         padding-bottom:12px;border-bottom:1px solid #ebeef5}
  .field{display:flex;align-items:center;padding:8px 0;color:#606266}
  .field .label{width:120px;color:#909399}
  .field .num{font-size:18px;color:#409eff;font-weight:600}
  .tip{color:#909399;font-size:13px;line-height:1.6;margin:8px 0}
  .tip.danger{color:#f56c6c}
  .row{margin-top:16px;display:flex;gap:8px}
  button{padding:7px 16px;border-radius:2px;border:1px solid #dcdfe6;cursor:pointer;
         font-size:13px;background:#fff;color:#606266}
  button:hover{color:#409eff;border-color:#c6e2ff;background:#fff}
  button.primary{background:#409eff;border-color:#409eff;color:#fff}
  button.primary:hover{background:#66b1ff;border-color:#66b1ff;color:#fff}
  button.danger{background:#f56c6c;border-color:#f56c6c;color:#fff}
  button.danger:hover{background:#f78989;border-color:#f78989;color:#fff}
  button:disabled{background:#fff;border-color:#ebeef5;color:#c0c4cc;cursor:not-allowed}
  #status{margin-top:12px;font-size:13px;display:none;color:#606266}
  #status.ok{color:#67c23a;display:block}
  #status.err{color:#f56c6c;display:block}
</style>
</head>
<body>
<div class="card">
  <div class="title">系统重启</div>

  <div class="field">
    <span class="label">当前在线用户</span>
    <span class="num" id="onlineCount">-</span>
  </div>

  <div class="tip">重启会让所有在线用户掉线，正在进行的请求会被中断。</div>
  <div class="tip danger">依赖外部守护进程（systemd / Docker / Windows Service / pm2）才能自动拉起；否则只是关停。</div>

  <div class="row">
    <button onclick="refresh()">刷新</button>
    <button class="danger" id="restartBtn" onclick="doRestart()">立即重启</button>
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
      showStatus('ok', (d.msg || '已发送停机指令') + '。如已配置守护进程，约 5~10 秒后会自动拉起。');
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
