#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include "../services/DatabaseService.h"
#include "../common/SecurityUtils.h"
#include "../system/services/TokenService.h"
#include "../services/KoboldCppService.h"
#include <sstream>

// ─── 快捷宏 ────────────────────────────────────────────────────────────────
#ifndef AI_OK
#define AI_OK(cb, data) do { \
    Json::Value _r; _r["code"]=200; _r["msg"]="ok"; _r["data"]=(data); \
    auto _resp=drogon::HttpResponse::newHttpJsonResponse(_r); cb(_resp); } while(0)
#define AI_ERR(cb, msg) do { \
    Json::Value _r; _r["code"]=500; _r["msg"]=(msg); \
    auto _resp=drogon::HttpResponse::newHttpJsonResponse(_r); cb(_resp); } while(0)
#define AI_UNAUTH(cb) do { \
    Json::Value _r; _r["code"]=401; _r["msg"]="请先登录"; \
    auto _resp=drogon::HttpResponse::newHttpJsonResponse(_r); \
    _resp->setStatusCode(drogon::k401Unauthorized); cb(_resp); } while(0)
#endif

class AiChatCtrl : public drogon::HttpController<AiChatCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(AiChatCtrl::index,          "/ai",                        drogon::Get);
        ADD_METHOD_TO(AiChatCtrl::apiInfo,         "/ai/api/info",               drogon::Get);
        ADD_METHOD_TO(AiChatCtrl::createSession,   "/ai/api/session",            drogon::Post);
        ADD_METHOD_TO(AiChatCtrl::listSessions,    "/ai/api/sessions",           drogon::Get);
        ADD_METHOD_TO(AiChatCtrl::deleteSession,   "/ai/api/session/{id}",       drogon::Delete);
        ADD_METHOD_TO(AiChatCtrl::getMessages,     "/ai/api/session/{id}/messages", drogon::Get);
        ADD_METHOD_TO(AiChatCtrl::chat,            "/ai/api/chat",               drogon::Post);
        ADD_METHOD_TO(AiChatCtrl::clearSession,    "/ai/api/session/{id}/clear", drogon::Post);
    METHOD_LIST_END

    // ── GET /ai  — 内嵌 HTML 聊天页面 ──────────────────────────────────────
    void index(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(chatHtml());
        cb(resp);
    }

    // ── GET /ai/api/info ────────────────────────────────────────────────────
    void apiInfo(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        Json::Value d;
        d["ready"]   = KoboldCppService::instance().isReady();
        d["backend"] = "KoboldCpp";
        Json::Value models(Json::arrayValue);
        Json::Value m1; m1["id"]="kobold"; m1["name"]="本地模型(KoboldCpp)"; m1["available"]=KoboldCppService::instance().isReady(); models.append(m1);
        Json::Value m2; m2["id"]="xfai";   m2["name"]="讯飞星火(云端)";         m2["available"]=true; models.append(m2);
        d["models"] = models;
        AI_OK(cb, d);
    }

    // ── POST /ai/api/session — 新建会话 ─────────────────────────────────────
    void createSession(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto u = getUser(req);
        if (!u) { AI_UNAUTH(cb); return; }

        auto body  = req->getJsonObject();
        std::string title = body ? (*body).get("title","新对话").asString() : "新对话";
        std::string model = body ? (*body).get("model","xfai").asString() : "xfai";
        std::string modelName = (model == "kobold") ? "KoboldCpp" : "讯飞星火";
        if (title.size() > 100) title = title.substr(0, 100);

        auto res = DatabaseService::instance().queryParams(
            "INSERT INTO ai_chat_session(user_id,user_name,title,model_name,create_time,update_time) "
            "VALUES($1,$2,$3,$4,NOW(),NOW()) RETURNING session_id",
            {std::to_string(u->userId), u->userName, title, modelName});
        if (!res.ok() || res.rows() == 0) { AI_ERR(cb, "创建会话失败"); return; }

        Json::Value d;
        d["session_id"] = (Json::Int64)std::stoll(res.str(0,0));
        d["title"]      = title;
        AI_OK(cb, d);
    }

    // ── GET /ai/api/sessions — 会话列表 ─────────────────────────────────────
    void listSessions(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto u = getUser(req);
        if (!u) { AI_UNAUTH(cb); return; }

        auto res = DatabaseService::instance().queryParams(
            "SELECT session_id,title,model_name,update_time FROM ai_chat_session "
            "WHERE user_id=$1 ORDER BY update_time DESC LIMIT 50",
            {std::to_string(u->userId)});

        Json::Value list(Json::arrayValue);
        for (int i = 0; i < res.rows(); ++i) {
            Json::Value s;
            s["session_id"]  = (Json::Int64)std::stoll(res.str(i,0));
            s["title"]       = res.str(i,1);
            s["model_name"]  = res.str(i,2);
            s["update_time"] = res.str(i,3);
            list.append(s);
        }
        AI_OK(cb, list);
    }

    // ── DELETE /ai/api/session/{id} — 删除会话 ──────────────────────────────
    void deleteSession(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       long long id) {
        auto u = getUser(req);
        if (!u) { AI_UNAUTH(cb); return; }

        DatabaseService::instance().execParams(
            "DELETE FROM ai_chat_message WHERE session_id=$1", {std::to_string(id)});
        DatabaseService::instance().execParams(
            "DELETE FROM ai_chat_session WHERE session_id=$1 AND user_id=$2",
            {std::to_string(id), std::to_string(u->userId)});

        Json::Value d; d["deleted"] = true;
        AI_OK(cb, d);
    }

    // ── GET /ai/api/session/{id}/messages — 消息历史 ─────────────────────────
    void getMessages(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     long long id) {
        auto u = getUser(req);
        if (!u) { AI_UNAUTH(cb); return; }

        auto res = DatabaseService::instance().queryParams(
            "SELECT msg_id,role,content,create_time FROM ai_chat_message "
            "WHERE session_id=$1 ORDER BY msg_id ASC",
            {std::to_string(id)});

        Json::Value list(Json::arrayValue);
        for (int i = 0; i < res.rows(); ++i) {
            Json::Value m;
            m["msg_id"]      = (Json::Int64)std::stoll(res.str(i,0));
            m["role"]        = res.str(i,1);
            m["content"]     = res.str(i,2);
            m["create_time"] = res.str(i,3);
            list.append(m);
        }
        AI_OK(cb, list);
    }

    // ── POST /ai/api/session/{id}/clear — 清空消息 ───────────────────────────
    void clearSession(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      long long id) {
        auto u = getUser(req);
        if (!u) { AI_UNAUTH(cb); return; }

        DatabaseService::instance().execParams(
            "DELETE FROM ai_chat_message WHERE session_id=$1", {std::to_string(id)});
        DatabaseService::instance().execParams(
            "UPDATE ai_chat_session SET title='新对话',update_time=NOW() "
            "WHERE session_id=$1 AND user_id=$2",
            {std::to_string(id), std::to_string(u->userId)});

        Json::Value d; d["cleared"] = true;
        AI_OK(cb, d);
    }

    // ── POST /ai/api/chat — 发送消息，获取 AI 回复 ─────────────────────────
    void chat(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto u = getUser(req);
        if (!u) { AI_UNAUTH(cb); return; }

        auto body = req->getJsonObject();
        if (!body) { AI_ERR(cb, "请求体格式错误"); return; }

        long long   sessionId = (*body).get("session_id", 0).asInt64();
        std::string userMsg   = (*body).get("message",    "").asString();
        std::string sysPrompt = (*body).get("system_prompt", "你是一个有帮助的AI助手，请用中文回答。").asString();
        int         maxTok    = (*body).get("max_tokens", 512).asInt();
        float       temp      = (float)(*body).get("temperature", 0.7).asDouble();
        std::string model     = (*body).get("model", "kobold").asString();

        if (userMsg.empty()) { AI_ERR(cb, "消息不能为空"); return; }
        if (userMsg.size() > 8000) { AI_ERR(cb, "消息过长，请精简后重试"); return; }

        auto& db = DatabaseService::instance();
        std::string modelName = (model == "xfai") ? "讯飞星火" : "KoboldCpp";

        // 1. 验证 session 归属（sessionId==0 则自动创建）
        if (sessionId == 0) {
            std::string autoTitle = userMsg.size() > 30 ? userMsg.substr(0,30) + "..." : userMsg;
            auto ins = db.queryParams(
                "INSERT INTO ai_chat_session(user_id,user_name,title,model_name,create_time,update_time) "
                "VALUES($1,$2,$3,$4,NOW(),NOW()) RETURNING session_id",
                {std::to_string(u->userId), u->userName, autoTitle, modelName});
            if (!ins.ok() || ins.rows() == 0) { AI_ERR(cb, "创建会话失败"); return; }
            sessionId = std::stoll(ins.str(0,0));
        } else {
            auto chk = db.queryParams(
                "SELECT session_id FROM ai_chat_session WHERE session_id=$1 AND user_id=$2",
                {std::to_string(sessionId), std::to_string(u->userId)});
            if (!chk.ok() || chk.rows() == 0) { AI_ERR(cb, "会话不存在或无权限"); return; }
        }

        // 2. 存储用户消息
        db.execParams(
            "INSERT INTO ai_chat_message(session_id,role,content,create_time) VALUES($1,'user',$2,NOW())",
            {std::to_string(sessionId), userMsg});

        // 3. 拉取最近 20 条历史消息
        auto hist = db.queryParams(
            "SELECT role,content FROM ai_chat_message WHERE session_id=$1 ORDER BY msg_id DESC LIMIT 20",
            {std::to_string(sessionId)});
        bool firstTurn = (hist.rows() <= 1);
        std::string titleStr = userMsg.size() > 30 ? userMsg.substr(0,30)+"..." : userMsg;

        // 共用：保存回复、更新会话、响应
        auto finalize = [=](const std::string &reply) {
            auto& db2 = DatabaseService::instance();
            db2.execParams(
                "INSERT INTO ai_chat_message(session_id,role,content,create_time) VALUES($1,'assistant',$2,NOW())",
                {std::to_string(sessionId), reply});
            db2.execParams(
                "UPDATE ai_chat_session SET update_time=NOW() WHERE session_id=$1",
                {std::to_string(sessionId)});
            if (firstTurn)
                db2.execParams("UPDATE ai_chat_session SET title=$1 WHERE session_id=$2",
                               {titleStr, std::to_string(sessionId)});
            Json::Value d;
            d["session_id"] = (Json::Int64)sessionId;
            d["reply"]      = reply;
            AI_OK(cb, d);
        };

        if (model == "xfai") {
            // 4a. 讯飞星火（异步 pearktrue API）
            try {
                auto client = drogon::HttpClient::newHttpClient("https://api.pearktrue.cn");
                auto xfReq  = drogon::HttpRequest::newHttpRequest();
                xfReq->setPath("/api/xfai/");
                xfReq->setParameter("message", userMsg);
                xfReq->setMethod(drogon::Get);
                client->sendRequest(xfReq,
                    [finalize](drogon::ReqResult result,
                               const drogon::HttpResponsePtr &resp) mutable {
                        std::string reply;
                        if (result == drogon::ReqResult::Ok && resp) {
                            try {
                                auto j = resp->getJsonObject();
                                if (j && (*j)["code"].asInt() == 200)
                                    reply = (*j)["data"].get("answer", "").asString();
                            } catch (...) {}
                        }
                        if (reply.empty()) reply = "⚠️ 讯飞星火暂时无法响应，请稍后重试。";
                        finalize(reply);
                    }, 15.0);
            } catch (...) {
                finalize("⚠️ 讯飞星火请求失败，请稍后重试。");
            }
        } else {
            // 4b. KoboldCpp（同步）
            std::string reply;
            if (!KoboldCppService::instance().isReady()) {
                reply = "⚠️ AI 引擎尚未就绪，请稍候或检查 KoboldCpp 服务状态。";
            } else {
                std::vector<std::pair<std::string,std::string>> msgs;
                for (int i = hist.rows()-1; i >= 0; --i)
                    msgs.push_back({hist.str(i,0), hist.str(i,1)});
                std::ostringstream prompt;
                prompt << sysPrompt << "\n\n";
                for (auto& [role, content] : msgs)
                    prompt << (role == "user" ? "用户：" : "助手：") << content << "\n";
                prompt << "助手：";
                reply = KoboldCppService::instance().generate(prompt.str(), temp, maxTok);
                if (reply.empty()) reply = KoboldCppService::instance().lastError();
                if (reply.empty()) reply = "AI 未返回内容，请重试。";
                if (reply.size() >= 6 && reply.substr(0, 6) == "助手：") reply = reply.substr(6);
                else if (reply.size() >= 5 && reply.substr(0, 5) == "助手:")  reply = reply.substr(5);
            }
            finalize(reply);
        }
    }

private:
    // ── 从请求头获取登录用户 ────────────────────────────────────────────────
    std::optional<LoginUser> getUser(const drogon::HttpRequestPtr& req) {
        return TokenService::instance().getLoginUser(req);
    }

    // ── 内嵌聊天 UI（ChatGPT 风格，无需前端）──────────────────────────────
    static std::string chatHtml() {
        return R"HTMLEOF(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AI 智能助手</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#1a1a2e;color:#e0e0e0;height:100vh;display:flex;overflow:hidden}
body.in-iframe #sidebar{display:none!important}
/* 侧边栏 */
#sidebar{width:260px;background:#16213e;display:flex;flex-direction:column;padding:12px;gap:8px;border-right:1px solid #0f3460;flex-shrink:0}
#newChatBtn{background:#0f3460;border:1px solid #4a9eff;color:#4a9eff;padding:10px;border-radius:8px;cursor:pointer;font-size:14px;transition:.2s}
#newChatBtn:hover{background:#4a9eff;color:#fff}
#sessionList{flex:1;overflow-y:auto;display:flex;flex-direction:column;gap:4px}
.session-item{padding:8px 10px;border-radius:6px;cursor:pointer;font-size:13px;color:#b0b0c0;display:flex;justify-content:space-between;align-items:center;word-break:break-all}
.session-item:hover,.session-item.active{background:#0f3460;color:#fff}
.session-item .del-btn{opacity:0;font-size:16px;color:#f56c6c;flex-shrink:0;margin-left:4px;line-height:1}
.session-item:hover .del-btn{opacity:1}
#modelStatus{font-size:11px;color:#666;padding:4px 2px;border-top:1px solid #0f3460;padding-top:8px}
#modelStatus span{color:#4a9eff}
/* 主区域 */
#main{flex:1;display:flex;flex-direction:column;overflow:hidden}
#chatTitle{padding:14px 20px;border-bottom:1px solid #0f3460;font-size:15px;font-weight:600;color:#a0c4ff;background:#16213e;display:flex;align-items:center;gap:12px}
#chatTitle .spacer{flex:1}
#activeModelBadge{font-size:12px;color:#dbeafe;background:#1e3a5f;border:1px solid #315a87;border-radius:999px;padding:4px 10px}
#inlineNewBtn{display:none;background:#0f3460;border:1px solid #4a9eff;color:#4a9eff;padding:6px 14px;border-radius:6px;cursor:pointer;font-size:13px;white-space:nowrap;transition:.2s}
#inlineNewBtn:hover{background:#4a9eff;color:#fff}
body.in-iframe #inlineNewBtn{display:inline-block}
#messages{flex:1;overflow-y:auto;padding:20px;display:flex;flex-direction:column;gap:16px}
.msg{display:flex;gap:12px;max-width:820px;align-self:flex-start}
.msg.user{flex-direction:row-reverse;align-self:flex-end}
.avatar{width:36px;height:36px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:16px;flex-shrink:0}
.msg.user .avatar{background:#4a9eff}
.msg.assistant .avatar{background:#7c3aed}
.bubble{padding:10px 14px;border-radius:12px;font-size:14px;line-height:1.6;max-width:680px;white-space:pre-wrap;word-break:break-word}
.msg.user .bubble{background:#0f3460;border-bottom-right-radius:2px}
.msg.assistant .bubble{background:#1e293b;border-bottom-left-radius:2px}
.bubble.typing::after{content:'▋';animation:blink .8s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:0}}
/* 输入区 */
#inputArea{padding:16px 20px;border-top:1px solid #0f3460;background:#16213e;display:flex;gap:10px;align-items:flex-end}
#msgInput{flex:1;background:#1e293b;border:1px solid #0f3460;color:#e0e0e0;border-radius:10px;padding:10px 14px;font-size:14px;resize:none;max-height:160px;min-height:44px;outline:none;font-family:inherit;line-height:1.5}
#msgInput:focus{border-color:#4a9eff}
#msgInput::placeholder{color:#555}
#sendBtn{background:#4a9eff;color:#fff;border:none;border-radius:10px;padding:10px 18px;cursor:pointer;font-size:14px;white-space:nowrap;transition:.2s;height:44px}
#sendBtn:hover:not(:disabled){background:#3a8ede}
#sendBtn:disabled{background:#2a5080;cursor:not-allowed}
#settingsRow{padding:4px 20px 0;display:flex;gap:12px;align-items:center;font-size:12px;color:#666}
#settingsRow label{display:flex;align-items:center;gap:4px}
#settingsRow input[type=range]{width:80px;accent-color:#4a9eff}
/* 欢迎页 */
#welcome{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:16px;color:#555;padding:40px}
#welcome h2{color:#4a9eff;font-size:22px}
#welcome p{font-size:14px;text-align:center;max-width:400px;line-height:1.8}
/* 滚动条 */
::-webkit-scrollbar{width:5px}
::-webkit-scrollbar-track{background:transparent}
::-webkit-scrollbar-thumb{background:#0f3460;border-radius:3px}
</style>
</head>
<body>
<div id="sidebar">
  <button id="newChatBtn">＋ 新对话</button>
  <div id="sessionList"></div>
  <div id="modelStatus">AI引擎：<span id="modelReady">检测中...</span></div>
</div>
<div id="main">
  <div id="chatTitle"><span>AI 智能助手</span><span class="spacer"></span><span id="activeModelBadge">当前模型：讯飞星火</span><button id="inlineNewBtn">＋ 新对话</button></div>
  <div id="messages">
    <div id="welcome">
      <h2>👋 你好，我是 AI 助手</h2>
      <p>点击左侧「新对话」开始聊天，我会尽力为你解答问题。</p>
    </div>
  </div>
  <div id="settingsRow">
    <label>模型 <select id="modelSel" style="background:#1e293b;color:#4a9eff;border:1px solid #0f3460;border-radius:4px">
      <option value="xfai">⚡ 讯飞星火(云端)</option>
      <option value="kobold">🖥️ 本地模型(KoboldCpp)</option>
    </select></label>
    <label>温度 <input type="range" id="tempRange" min="0" max="100" value="70">
      <span id="tempVal">0.7</span></label>
    <label>最大字数 <select id="maxTokSel" style="background:#1e293b;color:#aaa;border:1px solid #0f3460;border-radius:4px">
      <option value="256">256</option>
      <option value="512" selected>512</option>
      <option value="1024">1024</option>
    </select></label>
  </div>
  <div id="inputArea">
    <textarea id="msgInput" placeholder="输入消息… (Ctrl+Enter 发送)" rows="1"></textarea>
    <button id="sendBtn">发送</button>
  </div>
</div>

<script>
function getCookie(name){
  const m = document.cookie.match(new RegExp('(?:^|; )' + name + '=([^;]*)'));
  return m ? decodeURIComponent(m[1]) : '';
}
const token = getCookie('Admin-Token')
  || new URLSearchParams(location.search).get('token')
  || localStorage.getItem('Admin-Token')
  || sessionStorage.getItem('Admin-Token')
  || '';
let currentSession = null;
let sending = false;

// ── 工具 ─────────────────────────────────────────────────────────────────
const API = (path, opt={}) => {
  const url = '/ai/api' + path + (token ? (path.includes('?') ? '&' : '?') + 'token=' + encodeURIComponent(token) : '');
  const headers = {...(opt.headers||{})};
  if (token) headers['Authorization'] = 'Bearer ' + token;
  if (!headers['Content-Type'] && !(opt.body instanceof FormData)) headers['Content-Type'] = 'application/json';
  return fetch(url, {
    ...opt,
    headers
  }).then(r=>r.json());
};

const $ = id => document.getElementById(id);
function scrollBottom(){ const m=$('messages'); m.scrollTop=m.scrollHeight; }
function getModelLabel(v){ return v==='kobold' ? 'KoboldCpp' : '讯飞星火'; }
function updateActiveModel(){ $('activeModelBadge').textContent = '当前模型：' + getModelLabel($('modelSel').value); }

// ── 检测 AI 状态 ──────────────────────────────────────────────────────────
async function checkModel(){
  try{
    const r = await API('/info');
    const models = (r.data && r.data.models) || [];
    const byId = {};
    models.forEach(m=>byId[m.id]=m);
    const koboldOk = !!(byId.kobold && byId.kobold.available);
    const xfaiOk   = !(byId.xfai) || !!byId.xfai.available;
    $('modelReady').innerHTML =
      `讯飞星火:<span style="color:${xfaiOk?'#4caf50':'#f44336'}">${xfaiOk?'可用✓':'异常✗'}</span> `+
      `KoboldCpp:<span style="color:${koboldOk?'#4caf50':'#f44336'}">${koboldOk?'就绪✓':'离线✗'}</span>`;
  }catch(e){$('modelReady').textContent='连接失败';}
}

// ── 会话列表 ─────────────────────────────────────────────────────────────
async function loadSessions(){
  const r = await API('/sessions');
  const list = r.data || [];
  const el = $('sessionList');
  el.innerHTML='';
  list.forEach(s=>{
    const div=document.createElement('div');
    div.className='session-item'+(currentSession===s.session_id?' active':'');
    div.innerHTML=`<span style="overflow:hidden;text-overflow:ellipsis;white-space:nowrap">${esc(s.title||'新对话')}</span>`
      +`<span class="del-btn" title="删除">×</span>`;
    div.querySelector('.del-btn').onclick=async e=>{
      e.stopPropagation();
      if(!confirm('删除此对话？'))return;
      await API('/session/'+s.session_id,{method:'DELETE'});
      if(currentSession===s.session_id){currentSession=null;showWelcome();}
      loadSessions();
    };
    div.onclick=()=>openSession(s.session_id, s.title);
    el.appendChild(div);
  });
}

function esc(s){ const d=document.createElement('div');d.textContent=s;return d.innerHTML; }

// ── 会话操作 ─────────────────────────────────────────────────────────────
async function openSession(id, title){
  currentSession = id;
  $('chatTitle').textContent = title || '对话';
  const r = await API('/session/'+id+'/messages');
  const msgs = r.data || [];
  const el = $('messages');
  el.innerHTML='';
  msgs.forEach(m=>appendMsg(m.role, m.content));
  scrollBottom();
  loadSessions();
}

function showWelcome(){
  $('chatTitle').innerHTML=`<span>AI 智能助手</span><span class="spacer"></span><span id="activeModelBadge">当前模型：${getModelLabel($('modelSel').value)}</span><button id="inlineNewBtn">＋ 新对话</button>`;
  $('inlineNewBtn').onclick = doNewChat;
  $('messages').innerHTML=`<div id="welcome" style="flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:16px;color:#555;padding:40px">
    <h2 style="color:#4a9eff;font-size:22px">👋 你好，我是 AI 助手</h2>
    <p style="font-size:14px;text-align:center;max-width:400px;line-height:1.8">点击左侧「新对话」开始聊天，我会尽力为你解答问题。</p></div>`;
}

const doNewChat = async ()=>{
  const r = await API('/session',{method:'POST',body:JSON.stringify({title:'新对话', model:$('modelSel').value})});
  if(r.code===200){
    await openSession(r.data.session_id,'新对话');
    loadSessions();
  } else if(r.code===401){
    alert('请先登录系统后再使用 AI 助手');
  }
};

// ── 消息渲染 ─────────────────────────────────────────────────────────────
function appendMsg(role, content, typing=false){
  const el=$('messages');
  const div=document.createElement('div');
  div.className='msg '+role;
  const icon=role==='user'?'👤':'🤖';
  div.innerHTML=`<div class="avatar">${icon}</div>`
    +`<div class="bubble${typing?' typing':''}">${esc(content)}</div>`;
  el.appendChild(div);
  return div.querySelector('.bubble');
}

// ── 发送消息 ─────────────────────────────────────────────────────────────
async function send(){
  if(sending)return;
  const input=$('msgInput');
  const msg=input.value.trim();
  if(!msg)return;
  if(!token){alert('请先登录系统后再使用 AI 助手');return;}

  sending=true;
  $('sendBtn').disabled=true;
  input.value='';
  input.style.height='auto';

  appendMsg('user', msg);
  const bubble=appendMsg('assistant','', true);
  scrollBottom();

  const temp = $('tempRange').value/100;
  const maxTok = parseInt($('maxTokSel').value);

  try{
    const model = $('modelSel').value;
    const r = await API('/chat',{method:'POST',body:JSON.stringify({
      session_id: currentSession||0,
      message: msg,
      temperature: temp,
      max_tokens: maxTok,
      model: model
    })});
    if(r.code===200){
      if(!currentSession){
        currentSession=r.data.session_id;
        loadSessions();
      }
      bubble.classList.remove('typing');
      bubble.textContent=r.data.reply;
    } else if(r.code===401){
      bubble.classList.remove('typing');
      bubble.textContent='⚠️ 请先登录系统';
    } else {
      bubble.classList.remove('typing');
      bubble.textContent='错误：'+(r.msg||'未知错误');
    }
  }catch(e){
    bubble.classList.remove('typing');
    bubble.textContent='网络错误：'+e.message;
  }

  sending=false;
  $('sendBtn').disabled=false;
  scrollBottom();
  loadSessions();
}

$('newChatBtn').onclick = doNewChat;
$('inlineNewBtn').onclick = doNewChat;
$('sendBtn').onclick = send;
$('modelSel').onchange = updateActiveModel;
$('tempRange').oninput = e => $('tempVal').textContent = (e.target.value/100).toFixed(2);
$('msgInput').addEventListener('keydown', e=>{
  if(e.key==='Enter' && (e.ctrlKey || e.metaKey)){ e.preventDefault(); send(); }
});

updateActiveModel();
checkModel();
loadSessions();
showWelcome();
</script>
</body></html>

)HTMLEOF";
    }
};
