#pragma once
#include <drogon/drogon.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include "../../common/AjaxResult.h"
#include "../../common/SecurityUtils.h"
#include "../../common/TokenCache.h"
#include "../../system/services/TokenService.h"

// 系统日志文件查看器
// GET  /monitor/logfile/list          — 日志文件列表
// GET  /monitor/logfile/download      — 下载日志文件（?name=xxx）
// GET  /monitor/logfile/clean         — 删除日志文件（?name=xxx）
// GET  /monitor/logfile/page          — 内嵌 HTML 页面（供 iframe 使用）
class SysLogFileCtrl : public drogon::HttpController<SysLogFileCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysLogFileCtrl::list,     "/monitor/logfile/list",     drogon::Get);
        ADD_METHOD_TO(SysLogFileCtrl::download,  "/monitor/logfile/download", drogon::Get);
        ADD_METHOD_TO(SysLogFileCtrl::clean,     "/monitor/logfile/clean",    drogon::Delete);
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
        RESP_MSG(cb, "操作成功");
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
<div id="content">请选择日志文件...</div>
<script>
let token = '';
try { token = window.parent.sessionStorage.getItem('Admin-Token') || sessionStorage.getItem('Admin-Token') || ''; } catch(e){}
const headers = { 'Authorization': 'Bearer ' + token };
let autoTimer = null;

async function loadFiles() {
  const r = await fetch('/monitor/logfile/list', { headers });
  const d = await r.json();
  if (d.code !== 200) return;
  const sel = document.getElementById('fileSelect');
  const cur = sel.value;
  sel.innerHTML = '<option value="">-- 选择日志文件 --</option>';
  (d.data || []).sort((a,b) => b.fileTime - a.fileTime).forEach(f => {
    const opt = document.createElement('option');
    opt.value = f.fileName;
    opt.text  = f.fileName + ' (' + (f.fileSize/1024).toFixed(1) + ' KB)';
    if (f.fileName === cur) opt.selected = true;
    sel.appendChild(opt);
  });
}

async function loadFile() {
  const name = document.getElementById('fileSelect').value;
  if (!name) return;
  const r = await fetch('/monitor/logfile/download?name=' + encodeURIComponent(name), { headers });
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
