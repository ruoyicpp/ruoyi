#pragma once
#include <drogon/drogon.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include "../../common/AjaxResult.h"
#include "../../common/OperLogUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../common/TokenCache.h"
#include "../../system/services/TokenService.h"
#include "../../log/LogIndexer.h"

// 系统日志文件查看器
// GET  /monitor/logfile/list          — 日志文件列表
// GET  /monitor/logfile/download      — 下载日志文件（?name=xxx）
// GET  /monitor/logfile/clean         — 删除日志文件（?name=xxx）
// GET  /monitor/logfile/search       — Manticore 全文检索（?q=&level=&file=&limit=）
// GET  /monitor/logfile/page          — 内嵌 HTML 页面（供 iframe 使用）
class SysLogFileCtrl : public drogon::HttpController<SysLogFileCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysLogFileCtrl::list,     "/monitor/logfile/list",     drogon::Get);
        ADD_METHOD_TO(SysLogFileCtrl::download,  "/monitor/logfile/download", drogon::Get);
        ADD_METHOD_TO(SysLogFileCtrl::clean,     "/monitor/logfile/clean",    drogon::Delete);
        ADD_METHOD_TO(SysLogFileCtrl::search,    "/monitor/logfile/search",   drogon::Get);
        ADD_METHOD_TO(SysLogFileCtrl::page,      "/monitor/logfile/page",     drogon::Get);
    METHOD_LIST_END

    // 日志文件列表
    void list(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }

        namespace fs = std::filesystem;
        std::string logDir = "./logs";
        Json::Value arr(Json::arrayValue);
        if (fs::exists(logDir)) {
            for (auto &entry : fs::directory_iterator(logDir)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                if (ext != ".log" && ext != ".jsonl") continue;
                Json::Value f;
                f["fileName"] = entry.path().filename().string();
                f["fileSize"] = (Json::Int64)entry.file_size();
                auto mtime = fs::last_write_time(entry);
                f["fileTime"] = (Json::Int64)std::chrono::duration_cast<std::chrono::seconds>(
                    mtime.time_since_epoch()).count();
                arr.append(f);
            }
        }
        Json::Value r = AjaxResult::success();
        r["data"] = arr;
        RESP_JSON(cb, r);
    }

    // 下载日志文件内容（返回文本，前端展示）
    void download(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }

        std::string name = req->getParameter("name");
        if (name.find("..") != std::string::npos || name.find('/') != std::string::npos) {
            RESP_ERR(cb, "非法文件名"); return;
        }
        std::string path = "./logs/" + name;
        std::ifstream f(path, std::ios::binary);
        if (!f) { RESP_ERR(cb, "文件不存在"); return; }

        std::ostringstream ss; ss << f.rdbuf();
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setBody(ss.str());
        resp->setContentTypeString("text/plain; charset=utf-8");
        cb(resp);
    }

    // 删除日志文件
    void clean(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user || !SecurityUtils::isAdmin(user->userId)) { RESP_401(cb); return; }

        std::string name = req->getParameter("name");
        if (name.find("..") != std::string::npos || name.find('/') != std::string::npos) {
            RESP_ERR(cb, "非法文件名"); return;
        }
        std::error_code ec;
        std::filesystem::remove("./logs/" + name, ec);
        if (ec) { RESP_ERR(cb, "删除失败: " + ec.message()); return; }
        OperLogUtils::write(req, "日志文件", BusinessType::CLEAN, "name=" + name);
        RESP_MSG(cb, "操作成功");
    }

    // Manticore 全文检索：?q=关键词&level=ERROR&file=ruoyi.log&limit=200
    // 未启用 log.manticore 时返回 501，前端回退文件模式
    void search(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }

        std::string endpoint, index;
        if (!LogIndexer::instance().queryTarget(endpoint, index)) {
            auto r = AjaxResult::error("日志索引未启用（log.manticore.enabled=false）");
            auto resp = drogon::HttpResponse::newHttpJsonResponse(r);
            resp->setStatusCode(drogon::k501NotImplemented);
            cb(resp); return;
        }

        std::string q     = req->getParameter("q");
        std::string level = req->getParameter("level");
        std::string file  = req->getParameter("file");
        int limit = 200;
        try { int l = std::stoi(req->getParameter("limit")); if (l > 0 && l <= 1000) limit = l; }
        catch (...) {}

        // 组装 Manticore /search JSON：
        //   {"index":idx,"query":{"bool":{"must":[{match:{msg:q}}],
        //    "filter":[{term:{level:L}},{term:{file:F}}]}},"sort":[{"ts":{"order":"desc"}}],"limit":N}
        Json::Value query;
        query["index"] = index;
        if (!q.empty()) {
            Json::Value m; m["match"]["msg"] = q;
            query["query"]["bool"]["must"].append(m);
        } else {
            Json::Value a; a["match_all"] = Json::objectValue;
            query["query"]["bool"]["must"].append(a);
        }
        if (!level.empty()) {
            Json::Value f; f["term"]["level"] = level;
            query["query"]["bool"]["filter"].append(f);
        }
        if (!file.empty()) {
            Json::Value f; f["term"]["file"] = file;
            query["query"]["bool"]["filter"].append(f);
        }
        Json::Value sort; sort["ts"]["order"] = "desc";
        query["sort"].append(sort);
        query["limit"] = limit;

        auto cbPtr = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
        ManticoreClient::search(endpoint, query,
            [cbPtr](bool ok, int status, const std::string& body) {
                if (!ok || status != 200) {
                    auto r = AjaxResult::error("Manticore 查询失败");
                    (*cbPtr)(drogon::HttpResponse::newHttpJsonResponse(r));
                    return;
                }
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setBody(body);
                resp->setContentTypeString("application/json; charset=utf-8");
                (*cbPtr)(resp);
            });
    }

    // 内嵌 HTML 页面
    void page(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        (void)req;
        std::string html = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head><meta charset="UTF-8"><title>系统日志</title>
<style>
  body{font-family:monospace;background:#1e1e1e;color:#d4d4d4;margin:0;padding:16px}
  .toolbar{display:flex;gap:8px;margin-bottom:12px;align-items:center;flex-wrap:wrap}
  select,button{padding:6px 12px;border-radius:4px;border:1px solid #444;background:#2d2d2d;color:#d4d4d4;cursor:pointer}
  button:hover{background:#3a3a3a}
  input[type=text]{padding:6px 10px;border-radius:4px;border:1px solid #444;background:#2d2d2d;color:#d4d4d4;width:260px}
  .hit-file{color:#569cd6;font-size:11px}
  #content{white-space:pre-wrap;word-break:break-all;height:calc(100vh - 100px);overflow:auto;
            background:#111;padding:12px;border-radius:4px;font-size:13px;line-height:1.5}
  .info{color:#9cdcfe}.warn{color:#dcdcaa}.error{color:#f44747}.debug{color:#6a9955}
</style>
</head>
<body>
<div class="toolbar">
  <select id="fileSelect" onchange="loadFile()"><option value="">-- 选择日志文件 --</option></select>
  <button onclick="loadFile()">刷新</button>
  <button onclick="clearContent()">清空显示</button>
  <label><input type="checkbox" id="autoRefresh" onchange="toggleAuto()"> 自动刷新(5s)</label>
</div>
<div class="toolbar">
  <input type="text" id="q" placeholder="全文搜索（Manticore）" onkeydown="if(event.key==='Enter')doSearch()">
  <select id="qLevel">
    <option value="">全部级别</option><option>DEBUG</option><option>INFO</option>
    <option>WARN</option><option>ERROR</option><option>FATAL</option>
  </select>
  <button onclick="doSearch()">搜索</button>
  <span id="searchStatus" style="font-size:12px;color:#888"></span>
</div>
<div id="content">请选择日志文件...</div>
<script>
// token 提取优先级：
//   1) URL 查询串 ?token=xxx（直接访问场景）
//   2) parent.sessionStorage 'Admin-Token'（同源 iframe，菜单嵌入场景）
//   3) 本窗口 sessionStorage（直接打开但已登录过，部分浏览器同源 iframe 共享）
// 菜单 path 配置为 http://<frontend-host>/dev-api/monitor/logfile/page，
// 经前端 webpack-dev-server 代理转发到后端，与父窗口同源 → 可访问 parent.sessionStorage
let token = '';
try {
  const u = new URL(window.location.href);
  token = u.searchParams.get('token') || '';
  if (!token && window.parent !== window) {
    try { token = window.parent.sessionStorage.getItem('Admin-Token') || ''; } catch(e){}
  }
  if (!token) token = sessionStorage.getItem('Admin-Token') || '';
} catch(e){}
if (!token) {
  document.getElementById('content').textContent = '未获取到登录凭证。请通过菜单栏「日志查看」打开（自动注入 token），或在 URL 末尾加 ?token=xxx';
}
const headers = { 'Authorization': 'Bearer ' + token };

// 同目录相对路径请求：iframe 在 .../dev-api/monitor/logfile/page 时 → .../dev-api/monitor/logfile/list ✓
// 直接访问 .../monitor/logfile/page 时 → .../monitor/logfile/list ✓
// 末尾的 page 用 dirname 截掉，避免 list 拼成 page/list
const apiBase = window.location.pathname.replace(/\/[^/]*$/, '');
let autoTimer = null;

async function loadFiles() {
  const st = document.getElementById('searchStatus');
  try {
    const r = await fetch(apiBase + '/list', { headers });
    const d = await r.json();
    if (d.code !== 200) { st.textContent = 'list失败: ' + (d.msg || ('code=' + d.code)); return; }
    const sel = document.getElementById('fileSelect');
    const cur = sel.value;
    sel.innerHTML = '<option value="">-- 选择日志文件 --</option>';
    const arr = d.data || [];
    if (!arr.length) st.textContent = 'logs目录无 .log/.jsonl/.txt 文件';
    arr.sort((a,b) => b.fileTime - a.fileTime).forEach(f => {
      const opt = document.createElement('option');
      opt.value = f.fileName;
      opt.text  = f.fileName + ' (' + (f.fileSize/1024).toFixed(1) + ' KB)';
      if (f.fileName === cur) opt.selected = true;
      sel.appendChild(opt);
    });
  } catch(e) {
    st.textContent = 'list请求异常: ' + e.message;
  }
}

async function loadFile() {
  const name = document.getElementById('fileSelect').value;
  if (!name) return;
  const r = await fetch(apiBase + '/download?name=' + encodeURIComponent(name), { headers });
  if (!r.ok) { document.getElementById('content').textContent = '加载失败'; return; }
  const text = await r.text();
  const el = document.getElementById('content');
  el.innerHTML = text.split('\n').map(line => {
    if (line.includes('ERROR') || line.includes('"level":"error"')) return '<span class="error">' + esc(line) + '</span>';
    if (line.includes('WARN')  || line.includes('"level":"warn"'))  return '<span class="warn">'  + esc(line) + '</span>';
    if (line.includes('DEBUG') || line.includes('"level":"debug"')) return '<span class="debug">' + esc(line) + '</span>';
    return '<span class="info">' + esc(line) + '</span>';
  }).join('\n');
  el.scrollTop = el.scrollHeight;
}

async function doSearch() {
  const q = document.getElementById('q').value.trim();
  const level = document.getElementById('qLevel').value;
  const st = document.getElementById('searchStatus');
  let url = apiBase + '/search?limit=300';
  if (q) url += '&q=' + encodeURIComponent(q);
  if (level) url += '&level=' + level;
  st.textContent = '搜索中...';
  try {
    const r = await fetch(url, { headers });
    if (r.status === 501) { st.textContent = '全文索引未启用（log.manticore.enabled=false），仅支持文件查看'; return; }
    const d = await r.json();
    const hits = d.hits && d.hits.hits ? d.hits.hits : [];
    st.textContent = '命中 ' + (d.hits && d.hits.total !== undefined ? d.hits.total : hits.length) + ' 条';
    const el = document.getElementById('content');
    el.innerHTML = hits.map(h => {
      const s = h._source || {};
      const cls = s.level === 'ERROR' || s.level === 'FATAL' ? 'error'
                : s.level === 'WARN' ? 'warn' : s.level === 'DEBUG' ? 'debug' : 'info';
      return '<div class="hit-file">[' + esc(s.file||'') + ']</div><span class="' + cls + '">' + esc(s.msg||'') + '</span>';
    }).join('\n');
  } catch(e) { st.textContent = '搜索失败: ' + e.message; }
}

function esc(s) { return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
function clearContent() { document.getElementById('content').textContent = ''; }
function toggleAuto() {
  if (document.getElementById('autoRefresh').checked) {
    autoTimer = setInterval(loadFile, 5000);
  } else { clearInterval(autoTimer); autoTimer = null; }
}

loadFiles();
</script>
</body></html>)HTML";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(html);
        cb(resp);
    }
};
