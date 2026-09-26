/**
 * @file RoutesSetup.cc
 * @brief 内置路由注册（原 main.cc 后半部分）
 *
 * 包含：
 *   - /profile/{dir}/{file}     上传文件服务（头像、通用上传）
 *   - /iconfont-sys.woff2       iconfont 字体文件
 *   - /health                   健康检查（nginx upstream_check / k8s probe）
 *   - /version                  版本信息
 *   - /api/video/enabled        随机视频开关
 *   - /api/video/random         随机视频 URL（外部 API 302 跟随）
 *   - /api/video/player         内嵌 HTML 播放器页面
 *   - /ssl-config               HTTPS/SSL 证书管理页（HTML，自带鉴权）
 */

#include "AppBootstrap.h"

namespace boot {

void registerBuiltinRoutes(AppContext& /*ctx*/) {
    // 静态文件服务：/profile/{dir}/{file} → uploads/{dir}/{file}
    // 用于头像(/profile/avatar/xxx)、通用上传(/profile/upload/xxx)等
    auto serveUpload = [](const drogon::HttpRequestPtr &,
                          std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                          const std::string &dir, const std::string &file) {
        std::string filePath = "uploads/" + dir + "/" + file;
        if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath)) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            cb(resp);
            return;
        }
        cb(drogon::HttpResponse::newFileResponse(filePath));
    };
    drogon::app().registerHandler("/profile/{dir}/{file}", serveUpload, {drogon::Get});

    // ── iconfont 字体文件路由 ──────────────────────────────────────
    drogon::app().registerHandler("/iconfont-sys.woff2",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            static std::string fontData;
            static std::once_flag once;
            std::call_once(once, []() {
                std::ifstream f("iconfont-sys.woff2", std::ios::binary);
                if (f) fontData = std::string(std::istreambuf_iterator<char>(f), {});
            });
            auto resp = drogon::HttpResponse::newHttpResponse();
            if (fontData.empty()) {
                resp->setStatusCode(drogon::k404NotFound);
            } else {
                resp->setContentTypeString("font/woff2");
                resp->addHeader("Cache-Control", "public,max-age=86400");
                resp->setBody(fontData);
            }
            cb(resp);
        }, {drogon::Get});

    // ── /health 健康检查（nginx upstream_check / k8s liveness probe）──────
    drogon::app().registerHandler("/health",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto& db = DatabaseService::instance();
            bool dbOk = db.isConnected() || db.isUsingSqlite();
            Json::Value j;
            j["status"] = dbOk ? "UP" : "DEGRADED";
            j["db"]     = db.backendInfo();
            j["cache"]  = MemCache::backendInfo();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(j);
            resp->setStatusCode(dbOk ? drogon::k200OK : drogon::k503ServiceUnavailable);
            cb(resp);
        }, {drogon::Get});

    // ── /version 版本信息 ──────────────────────────────────────────────
    drogon::app().registerHandler("/version",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            Json::Value j;
            j["app"]     = "ruoyi-cpp";
            j["version"] = "1.0.0";
            cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }, {drogon::Get});

    // ── 随机视频开关（无需登录，前端用于决定是否显示菜单）──────────────
    // GET /api/video/enabled → {"enabled":true}
    drogon::app().registerHandler("/api/video/enabled",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto& db = DatabaseService::instance();
            auto res = db.queryParams(
                "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1",
                {"sys.video.enabled"});
            bool enabled = true;
            if (res.ok() && res.rows() > 0) {
                std::string val = res.str(0, 0);
                enabled = !(val == "false" || val == "0");
            }
            Json::Value j;
            j["enabled"] = enabled;
            auto r = drogon::HttpResponse::newHttpJsonResponse(j);
            r->addHeader("Access-Control-Allow-Origin", "*");
            cb(r);
        }, {drogon::Get});

    // ── 随机视频接口 ───────────────────────────────────────────────────
    // GET /api/video/random  → {"url":"https://...mp4"}
    drogon::app().registerHandler("/api/video/random",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // 用 drogon HttpClient 跟随外部 API 的 302 跳转，取最终 mp4 URL
            auto client = drogon::HttpClient::newHttpClient("http://api.yujn.cn");
            auto extReq = drogon::HttpRequest::newHttpRequest();
            extReq->setPath("/api/zzxjj.php");
            extReq->setParameter("type", "video");
            extReq->setMethod(drogon::Get);
            client->sendRequest(extReq,
                [cb](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
                    Json::Value j;
                    if (result == drogon::ReqResult::Ok) {
                        // 302 Location 就是 mp4 直链
                        std::string url = resp->getHeader("location");
                        if (url.empty()) url = std::string(resp->body());
                        j["url"] = url;
                        j["ok"]  = true;
                    } else {
                        j["ok"]  = false;
                        j["url"] = "";
                    }
                    auto r = drogon::HttpResponse::newHttpJsonResponse(j);
                    r->addHeader("Access-Control-Allow-Origin", "*");
                    cb(r);
                });
        }, {drogon::Get});

    // GET /api/video/player  → 内嵌 HTML 播放器页面
    drogon::app().registerHandler("/api/video/player",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            static const std::string html = R"html(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>随机视频</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d0d0d;display:flex;flex-direction:column;align-items:center;
     justify-content:center;min-height:100vh;font-family:sans-serif;color:#fff}
h2{margin-bottom:16px;font-size:1.2rem;opacity:.7;letter-spacing:2px}
.wrap{position:relative;width:min(420px,95vw);background:#1a1a1a;border-radius:16px;
      overflow:hidden;box-shadow:0 8px 40px #0008}
video{width:100%;display:block;max-height:75vh;background:#000;object-fit:contain}
.ctrl{display:flex;gap:12px;padding:12px;background:#111}
button{flex:1;padding:10px;border:none;border-radius:8px;cursor:pointer;
       font-size:.95rem;font-weight:600;transition:.2s}
#btnNext{background:#e94560;color:#fff}
#btnNext:hover{background:#c73652}
#btnDl{background:#2a2a2a;color:#aaa}
#btnDl:hover{background:#3a3a3a;color:#fff}
#status{font-size:.75rem;opacity:.5;padding:4px 12px 8px;text-align:center}
</style>
</head>
<body>
<h2>随机视频</h2>
<div class="wrap">
  <video id="v" autoplay playsinline muted loop></video>
  <div class="ctrl">
    <button id="btnNext" onclick="next()">▶ 下一个</button>
    <button id="btnDl" onclick="dl()">⬇ 下载</button>
  </div>
  <div id="status">加载中...</div>
</div>
<script>
let cur='';
async function next(){
  document.getElementById('status').textContent='加载中...';
  try{
    const r=await fetch('/api/video/random');
    const d=await r.json();
    if(d.ok&&d.url){
      cur=d.url;
      const v=document.getElementById('v');
      v.src=cur;
      v.load();
      v.play().catch(()=>{});
      document.getElementById('status').textContent='';
    }else{
      document.getElementById('status').textContent='获取失败，请重试';
    }
  }catch(e){
    document.getElementById('status').textContent='网络错误: '+e.message;
  }
}
function dl(){
  if(!cur)return;
  const a=document.createElement('a');
  a.href=cur;a.download='video.mp4';a.target='_blank';a.click();
}
next();
</script>
</body>
</html>)html";
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(html);
            cb(resp);
        }, {drogon::Get});

    // ── SSL/HTTPS 配置管理页（无需前端，浏览器直接访问）─────────────────
    drogon::app().registerHandler("/ssl-config",
        [](const drogon::HttpRequestPtr& req,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // 鉴权：Authorization header / Cookie Admin-Token / query param / localhost
            auto token = SecurityUtils::getToken(req);
            if (token.empty()) {
                // 标准 RuoYi Vue2 前端将 JWT 存在 Cookie 'Admin-Token'
                const std::string& cookieHdr = req->getHeader("cookie");
                const std::string key = "Admin-Token=";
                auto pos = cookieHdr.find(key);
                if (pos != std::string::npos) {
                    pos += key.size();
                    auto end = cookieHdr.find(';', pos);
                    token = cookieHdr.substr(pos, end == std::string::npos ? end : end - pos);
                }
            }
            if (token.empty()) token = req->getParameter("token");
            bool ok = false;
            if (!token.empty()) {
                try {
                    auto uuid    = JwtUtils::parseUuid(token);
                    auto userKey = SecurityUtils::getTokenKey(uuid);
                    ok = (bool)TokenCache::instance().get(userKey);
                } catch (...) {}
            }
            if (!ok) {
                const auto& peer = req->getPeerAddr().toIp();
                ok = (peer == "127.0.0.1" || peer == "::1" || peer == "0.0.0.0");
            }
            if (!ok) {
                auto r = drogon::HttpResponse::newHttpResponse();
                r->setContentTypeCode(drogon::CT_TEXT_HTML);
                r->setBody(R"HTML(<!DOCTYPE html><html><head><meta charset="UTF-8"></head><body><script>
(function(){
  var t='';
  try{var u=new URL(window.location.href);t=u.searchParams.get('token')||'';}catch(e){}
  if(!t&&window.parent!==window){try{t=window.parent.sessionStorage.getItem('Admin-Token')||'';}catch(e){}}
  if(!t){try{t=sessionStorage.getItem('Admin-Token')||'';}catch(e){}}
  if(t){var u=new URL(window.location.href);u.searchParams.set('token',t);window.location.replace(u.toString());}
  else{document.body.innerHTML='<div style="text-align:center;padding:60px;font-family:sans-serif"><h2>&#128274; 请先登录后携带 token 访问</h2><p>示例：/ssl-config?token=eyJhbG...</p></div>';}
})();
</script></body></html>)HTML");
                cb(r); return;
            }
            std::string tok = token;
            std::string html = R"html(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HTTPS / SSL 配置</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#0f1117;color:#e2e8f0;padding:16px}
h1{font-size:1.4rem;margin-bottom:20px;color:#7dd3fc;display:flex;align-items:center;gap:8px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:16px;margin-bottom:20px}
.card{background:#1e2330;border-radius:12px;padding:20px;border:1px solid #2d3748}
.card h2{font-size:.95rem;color:#94a3b8;margin-bottom:14px;text-transform:uppercase;letter-spacing:.05em}
.badge{display:inline-block;padding:2px 10px;border-radius:20px;font-size:.8rem;font-weight:600}
.ok{background:#065f46;color:#6ee7b7}.fail{background:#7f1d1d;color:#fca5a5}
label{display:block;font-size:.85rem;color:#94a3b8;margin-bottom:4px;margin-top:10px}
input[type=text],input[type=number]{width:100%;padding:8px 10px;background:#0f1117;border:1px solid #374151;
  border-radius:6px;color:#e2e8f0;font-size:.9rem}
input[type=file]{width:100%;padding:6px;background:#0f1117;border:1px solid #374151;
  border-radius:6px;color:#94a3b8;font-size:.85rem}
.toggle{display:flex;align-items:center;gap:10px;margin-top:10px}
.toggle input{width:40px;height:22px;accent-color:#3b82f6;cursor:pointer}
btn,button{display:inline-block;padding:9px 20px;border-radius:8px;border:none;cursor:pointer;font-size:.88rem;font-weight:600;transition:.2s}
.btn-primary{background:#3b82f6;color:#fff}.btn-primary:hover{background:#2563eb}
.btn-success{background:#059669;color:#fff}.btn-success:hover{background:#047857}
.btn-warn{background:#d97706;color:#fff}.btn-warn:hover{background:#b45309}
.actions{display:flex;gap:10px;margin-top:16px;flex-wrap:wrap}
#msg{margin-top:14px;padding:10px 14px;border-radius:8px;display:none;font-size:.88rem}
.msg-ok{background:#065f46;color:#6ee7b7;display:block!important}
.msg-err{background:#7f1d1d;color:#fca5a5;display:block!important}
.hint{font-size:.78rem;color:#64748b;margin-top:6px}
pre{background:#0f1117;padding:10px;border-radius:6px;font-size:.75rem;color:#6ee7b7;overflow-x:auto;margin-top:6px;max-height:80px}
</style>
</head>
<body>
<h1>&#128274; HTTPS / SSL 证书管理</h1>
<div class="grid" id="statusGrid">
  <div class="card"><h2>当前状态</h2><div id="statusHtml">加载中...</div></div>
  <div class="card"><h2>证书预览</h2><pre id="certPreview">-</pre></div>
</div>
<div class="grid">
  <div class="card">
    <h2>上传证书 (.pem / .crt / .cer)</h2>
    <input type="file" id="certFile" accept=".pem,.crt,.cer">
    <div class="actions"><button class="btn-primary" onclick="uploadCert()">上传证书</button></div>
  </div>
  <div class="card">
    <h2>上传私钥 (.pem / .key)</h2>
    <input type="file" id="keyFile" accept=".pem,.key">
    <div class="actions"><button class="btn-primary" onclick="uploadKey()">上传私钥</button></div>
  </div>
</div>
<div class="card" style="max-width:560px">
  <h2>配置</h2>
  <label>HTTP 端口</label>
  <input type="number" id="httpPort" value="18080" min="1" max="65535">
  <label>HTTPS 端口</label>
  <input type="number" id="httpsPort" value="18443" min="1" max="65535">
  <div class="toggle">
    <label style="margin:0">启用 HTTPS</label>
    <input type="checkbox" id="enabled">
  </div>
  <div class="toggle">
    <label style="margin:0">强制 HTTP → HTTPS 跳转</label>
    <input type="checkbox" id="forceHttps">
  </div>
  <div id="msg"></div>
  <div class="actions">
    <button class="btn-success" onclick="saveConfig()">保存配置</button>
  </div>
  <p class="hint">&#9888;&#65039; 配置保存后需<b>重启后端服务</b>方可生效</p>
</div>
<script>
function getCookie(n){const m=document.cookie.match(new RegExp('(?:^|; )'+n+'=([^;]*)'));return m?decodeURIComponent(m[1]):'';}
const TOKEN = getCookie('Admin-Token') || new URLSearchParams(location.search).get('token') || '';
const H = {'Authorization':'Bearer '+TOKEN,'Content-Type':'application/json'};
async function api(url,method,body){
  const r=await fetch(url+'?token='+TOKEN,{method,headers:method==='GET'?{}:H,body:body?JSON.stringify(body):undefined});
  return r.json();
}
async function load(){
  const d=await api('/system/ssl/config','GET');
  if(d.code!==200){document.getElementById('statusHtml').innerHTML='<span class="badge fail">查询失败</span>';return;}
  const c=d.data;
  document.getElementById('httpPort').value=c.httpPort||18080;
  document.getElementById('httpsPort').value=c.httpsPort||18443;
  document.getElementById('enabled').checked=c.enabled;
  document.getElementById('forceHttps').checked=c.forceHttps;
  document.getElementById('certPreview').textContent=c.certPreview||'（未上传）';
  document.getElementById('statusHtml').innerHTML=`
    <div style="display:flex;flex-wrap:wrap;gap:8px;margin-bottom:6px">
      <span class="badge ${c.enabled?'ok':'fail'}">${c.enabled?'HTTPS 已启用':'HTTPS 未启用'}</span>
      <span class="badge ${c.certExists?'ok':'fail'}">${c.certExists?'证书已上传':'证书未上传'}</span>
      <span class="badge ${c.certInDb?'ok':'fail'}">${c.certInDb?'DB: cert ✓':'DB: cert 无'}</span>
      <span class="badge ${c.keyInDb?'ok':'fail'}">${c.keyInDb?'DB: key ✓':'DB: key 无'}</span>
    </div>
    <div class="hint">HTTP:${c.httpPort} / HTTPS:${c.httpsPort}${c.forceHttps?' | 强制跳转':''}</div>`;
}
function showMsg(txt,ok){
  const el=document.getElementById('msg');
  el.textContent=txt;el.className=ok?'msg-ok':'msg-err';
  setTimeout(()=>el.className='',4000);
}
async function uploadCert(){
  const f=document.getElementById('certFile').files[0];
  if(!f){showMsg('请选择证书文件',false);return;}
  const fd=new FormData();fd.append('file',f);
  const r=await fetch('/system/ssl/uploadCert?token='+TOKEN,{method:'POST',body:fd});
  const d=await r.json();
  showMsg(d.msg,d.code===200);if(d.code===200)load();
}
async function uploadKey(){
  const f=document.getElementById('keyFile').files[0];
  if(!f){showMsg('请选择私钥文件',false);return;}
  const fd=new FormData();fd.append('file',f);
  const r=await fetch('/system/ssl/uploadKey?token='+TOKEN,{method:'POST',body:fd});
  const d=await r.json();
  showMsg(d.msg,d.code===200);if(d.code===200)load();
}
async function saveConfig(){
  const body={
    enabled:document.getElementById('enabled').checked,
    httpsPort:parseInt(document.getElementById('httpsPort').value),
    httpPort:parseInt(document.getElementById('httpPort').value),
    forceHttps:document.getElementById('forceHttps').checked
  };
  const d=await api('/system/ssl/config','PUT',body);
  showMsg(d.msg,d.code===200);
}
load();
</script>
</body></html>)html";
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(html);
            cb(resp);
        }, {drogon::Get});
}

} // namespace boot
