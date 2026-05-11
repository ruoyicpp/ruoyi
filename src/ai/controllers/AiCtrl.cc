#include "AiCtrl.h"
#include "services/KoboldCppService.h"
#include "services/WhisperService.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <thread>

using namespace drogon;

// ── 讯飞星火 Lite 1.5 fallback 客户端 ─────────────────────────────
// 当本地 KoboldCpp 不可用时降级到外部 API：
//   GET https://api.pearapi.ai/api/xfai/?message=<urlencoded>
// 返回 {"code":200,"msg":"success","model":"...","total_tokens":N,
//        "message":"...","answer":"..."}
//
// 配置 config.json -> "ai_fallback":
//   {
//     "enabled": true,
//     "provider": "spark",
//     "url": "https://api.pearapi.ai",
//     "path": "/api/xfai/",
//     "timeout_ms": 15000
//   }
namespace {
    // 单个外部 AI provider 的配置
    struct Provider {
        std::string name;        // 显示名（图灵 / spark-lite-1.5 等）
        std::string baseUrl;     // https://v2.xxapi.cn
        std::string path;        // /api/turing
        std::string param;       // msg / message
        // 响应 JSON 中 answer 字段的点号路径："data" / "data.answer" / "answer"
        std::string answerPath;
        // total_tokens 路径（可选）
        std::string tokensPath;
    };

    struct FallbackCfg {
        bool      enabled   = true;
        int       timeoutMs = 15000;
        std::vector<Provider> providers;
    };

    // 内置默认 provider 链：图灵优先（快）+ 讯飞星火兜底（质量好）
    void appendDefaultProviders(std::vector<Provider>& v) {
        v.push_back({"turing",         "https://v2.xxapi.cn",  "/api/turing",
                     "msg",     "data",        ""});
        v.push_back({"spark-lite-1.5", "https://api.pearapi.ai", "/api/xfai/",
                     "message", "data.answer", "data.total_tokens"});
    }

    FallbackCfg loadFallbackCfg() {
        FallbackCfg c;
        try {
            const auto& cc = drogon::app().getCustomConfig();
            if (cc.isMember("ai_fallback")) {
                const auto& f = cc["ai_fallback"];
                c.enabled   = f.get("enabled", true).asBool();
                c.timeoutMs = f.get("timeout_ms", c.timeoutMs).asInt();
                // 新结构: providers 数组
                if (f.isMember("providers") && f["providers"].isArray()) {
                    for (const auto& p : f["providers"]) {
                        Provider pr;
                        pr.name       = p.get("name", "unnamed").asString();
                        pr.baseUrl    = p.get("url", "").asString();
                        pr.path       = p.get("path", "").asString();
                        pr.param      = p.get("param", "message").asString();
                        pr.answerPath = p.get("answer_path", "data.answer").asString();
                        pr.tokensPath = p.get("tokens_path", "").asString();
                        if (!pr.baseUrl.empty() && !pr.path.empty())
                            c.providers.push_back(std::move(pr));
                    }
                }
                // 兼容旧结构: 顶层 url/path（spark 单 provider）
                if (c.providers.empty() && f.isMember("url")) {
                    Provider pr;
                    pr.name       = f.get("provider", "spark").asString();
                    pr.baseUrl    = f.get("url",  "https://api.pearapi.ai").asString();
                    pr.path       = f.get("path", "/api/xfai/").asString();
                    pr.param      = "message";
                    pr.answerPath = "data.answer";
                    pr.tokensPath = "data.total_tokens";
                    c.providers.push_back(pr);
                }
            }
        } catch (...) {}
        if (c.providers.empty()) appendDefaultProviders(c.providers);
        return c;
    }

    // 按点号路径取 JSON 字符串（如 "data.answer"）；若节点本身是字符串直接返回
    std::string jsonPathString(const Json::Value& root, const std::string& path) {
        if (path.empty()) return "";
        const Json::Value* node = &root;
        size_t pos = 0;
        while (pos < path.size()) {
            size_t dot = path.find('.', pos);
            std::string key = path.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
            if (!node->isObject() || !node->isMember(key)) return "";
            node = &(*node)[key];
            if (dot == std::string::npos) break;
            pos = dot + 1;
        }
        if (node->isString()) return node->asString();
        if (node->isNumeric()) return std::to_string(node->asInt64());
        return "";
    }
    int jsonPathInt(const Json::Value& root, const std::string& path) {
        if (path.empty()) return 0;
        const Json::Value* node = &root;
        size_t pos = 0;
        while (pos < path.size()) {
            size_t dot = path.find('.', pos);
            std::string key = path.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
            if (!node->isObject() || !node->isMember(key)) return 0;
            node = &(*node)[key];
            if (dot == std::string::npos) break;
            pos = dot + 1;
        }
        return node->isNumeric() ? node->asInt() : 0;
    }

    // 简单 URL 编码（保留字母数字 + _-.~，其它转 %XX）
    std::string urlEncode(const std::string& s) {
        static const char* HEX = "0123456789ABCDEF";
        std::string out;
        out.reserve(s.size() * 3);
        for (unsigned char c : s) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                || (c >= '0' && c <= '9') || c == '-' || c == '_'
                || c == '.' || c == '~') {
                out.push_back((char)c);
            } else {
                out.push_back('%');
                out.push_back(HEX[c >> 4]);
                out.push_back(HEX[c & 0xF]);
            }
        }
        return out;
    }

    // 一次性检测 curl 是否在 PATH 中（线程安全，结果缓存）
    bool curlAvailable() {
        static const bool ok = []() {
#ifdef _WIN32
            // Windows 自带 C:\Windows\System32\curl.exe (Win10 1803+)
            // 也兼容 PATH 中其它位置
            int rc = std::system("curl --version > NUL 2>&1");
#else
            // Linux: 容器/distroless/Alpine 默认可能没 curl
            int rc = std::system("curl --version > /dev/null 2>&1");
#endif
            if (rc != 0) {
                LOG_ERROR << "[Spark] curl 未找到（Windows 需 Win10 1803+ 自带；"
                             "Linux 需 apt/yum/apk install curl）。"
                             "AI fallback 将不可用";
            }
            return rc == 0;
        }();
        return ok;
    }

    // 同步调用 curl 命令获取响应；返回 (success, body)
    // 用 _popen/popen 调子进程读 stdout
    std::pair<bool, std::string> curlGet(const std::string& url, int timeoutSec) {
        if (!curlAvailable()) {
            return {false, "curl not installed (please install: apt/yum/apk add curl)"};
        }
#ifdef _WIN32
        // Windows _popen 用 cmd /c 跑，cmd 会"去掉首尾引号配对"——
        // 所以需在外层再包一对 " " 让 cmd 把外层的去掉，里面的引号才得以保留
        std::string innerCmd = "curl -s -m " + std::to_string(timeoutSec)
                             + " --connect-timeout 5 \"" + url + "\"";
        std::string cmd = "\"" + innerCmd + "\"";
        FILE* p = _popen(cmd.c_str(), "rb");
#else
        // Linux: 单引号包裹 url（urlEncode 已把 ' 转为 %27，单引号包裹安全）
        std::string cmd = "curl -s -m " + std::to_string(timeoutSec)
                        + " --connect-timeout 5 '" + url + "'";
        FILE* p = popen(cmd.c_str(), "r");
#endif
        if (!p) return {false, "popen failed"};
        std::string out;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), p)) > 0)
            out.append(buf, n);
#ifdef _WIN32
        int rc = _pclose(p);
#else
        int rc = pclose(p);
#endif
        if (rc != 0 && out.empty()) return {false, "curl exit=" + std::to_string(rc)};
        return {true, out};
    }

    // 按 provider 配置解析响应 → (success, answer, tokens, err)
    void parseProviderResponse(const Provider& p, const std::string& body,
                               bool& success, std::string& answer,
                               int& tokens, std::string& err) {
        success = false; answer.clear(); tokens = 0; err.clear();
        if (body.empty()) { err = "empty response body"; return; }
        Json::Value root;
        Json::CharReaderBuilder rb;
        std::string errs;
        std::istringstream is(body);
        if (!Json::parseFromStream(rb, is, &root, &errs)) {
            LOG_WARN << "[" << p.name << "] parse JSON failed errs=" << errs
                     << " body=" << body.substr(0, 200);
            err = "invalid response from upstream"; return;
        }
        int code = root.get("code", -1).asInt();
        if (code != 200 && code != 0) {
            err = root.get("msg", "upstream error").asString();
            return;
        }
        answer = jsonPathString(root, p.answerPath);
        tokens = p.tokensPath.empty() ? 0 : jsonPathInt(root, p.tokensPath);
        if (answer.empty()) { err = "upstream returned empty answer"; return; }
        success = true;
    }

    // 调用单个 provider（drogon HttpClient + TLS）
    using FbCallback = std::function<void(bool, const std::string& answer,
                                           int tokens, const std::string& providerName)>;

    void callProvider(const Provider& p, const std::string& message,
                       int timeoutMs, FbCallback cb) {
        auto client = drogon::HttpClient::newHttpClient(p.baseUrl);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath(p.path);
        req->setParameter(p.param, message);

        client->sendRequest(req,
            [p, cb = std::move(cb)](drogon::ReqResult result,
                                     const drogon::HttpResponsePtr& resp) {
                if (result != drogon::ReqResult::Ok || !resp) {
                    LOG_WARN << "[" << p.name << "] HTTP failed result=" << (int)result;
                    cb(false, "unreachable", 0, p.name);
                    return;
                }
                std::string body{resp->getBody()};
                LOG_INFO << "[" << p.name << "] status=" << (int)resp->statusCode()
                         << " body_len=" << body.size();
                bool ok; std::string answer, err; int tokens;
                parseProviderResponse(p, body, ok, answer, tokens, err);
                if (ok) cb(true, answer, tokens, p.name);
                else    cb(false, err, 0, p.name);
            },
            timeoutMs / 1000.0);
    }

    // 链式尝试 providers：第一个成功就返回，全部失败时回调汇总错误
    // 注意：tryIndex 通过 shared_ptr 让 lambda 链多次进入自身
    void callFallbackChain(std::shared_ptr<FallbackCfg> cfg, size_t index,
                           const std::string& message, FbCallback finalCb) {
        if (index >= cfg->providers.size()) {
            finalCb(false, "all providers failed", 0, "");
            return;
        }
        const auto& p = cfg->providers[index];
        callProvider(p, message, cfg->timeoutMs,
            [cfg, index, message, finalCb = std::move(finalCb)]
            (bool ok, const std::string& text, int tokens, const std::string& name) mutable {
                if (ok) {
                    finalCb(true, text, tokens, name);
                    return;
                }
                LOG_WARN << "[Fallback] " << name << " failed: " << text
                         << " - trying next provider";
                // 异步尝试下一个 provider
                callFallbackChain(cfg, index + 1, message, std::move(finalCb));
            });
    }

    // 入口：异步调外部 AI fallback，按 providers 顺序尝试
    void callSpark(const std::string& message, FbCallback cb) {
        auto cfg = std::make_shared<FallbackCfg>(loadFallbackCfg());
        if (!cfg->enabled || cfg->providers.empty()) {
            cb(false, "ai_fallback disabled or no providers", 0, "");
            return;
        }
        callFallbackChain(cfg, 0, message, std::move(cb));
    }
}

// drogon HttpClient 已通过 trantor TLS 支持 HTTPS，curl 子进程逻辑已废弃
// 保留 curlAvailable() / curlGet() 实现作为应急备份（如 OpenSSL 异常时手动切换）

void AiCtrl::chat(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body) {
        Json::Value e; e["msg"] = "Invalid JSON";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }
    std::string message      = (*body).get("message", "").asString();
    std::string systemPrompt = (*body).get("system",  "").asString();
    float  temperature = (float)(*body).get("temperature", -1.0).asDouble();
    int    maxTokens   = (*body).get("max_tokens", -1).asInt();

    if (message.empty()) {
        Json::Value e; e["msg"] = "message is required";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }

    auto& svc = KoboldCppService::instance();
    if (svc.isReady()) {
        std::string result = svc.chat(message, systemPrompt, temperature, maxTokens);
        Json::Value resp;
        resp["code"] = 200;
        resp["data"]["reply"] = result;
        resp["data"]["model"] = "koboldcpp";
        return cb(HttpResponse::newHttpJsonResponse(resp));
    }

    // ── KoboldCpp 不可用，fallback 到外部 AI 提供商链 ──
    LOG_INFO << "[Ai] KoboldCpp unavailable, falling back chain for /ai/chat";
    callSpark(message,
        [cb = std::move(cb)](bool ok, const std::string& text, int tokens,
                              const std::string& providerName) {
            Json::Value resp;
            if (ok) {
                resp["code"] = 200;
                resp["data"]["reply"]        = text;
                resp["data"]["model"]        = providerName;
                resp["data"]["total_tokens"] = tokens;
                resp["data"]["fallback"]     = true;
                cb(HttpResponse::newHttpJsonResponse(resp));
            } else {
                resp["code"] = 503;
                resp["msg"]  = std::string("AI 服务不可用: ") + text;
                auto r = HttpResponse::newHttpJsonResponse(resp);
                r->setStatusCode(k503ServiceUnavailable);
                cb(r);
            }
        });
}

void AiCtrl::generate(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body) {
        Json::Value e; e["msg"] = "Invalid JSON";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }
    std::string prompt     = (*body).get("prompt", "").asString();
    float  temperature = (float)(*body).get("temperature", -1.0).asDouble();
    int    maxTokens   = (*body).get("max_tokens", -1).asInt();

    if (prompt.empty()) {
        Json::Value e; e["msg"] = "prompt is required";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }

    auto& svc = KoboldCppService::instance();
    if (svc.isReady()) {
        std::string result = svc.generate(prompt, temperature, maxTokens);
        Json::Value resp;
        resp["code"] = 200;
        resp["data"]["text"] = result;
        return cb(HttpResponse::newHttpJsonResponse(resp));
    }

    // ── KoboldCpp 不可用，fallback 到外部 AI 提供商链 ──
    LOG_INFO << "[Ai] KoboldCpp unavailable, falling back chain for /ai/generate";
    callSpark(prompt,
        [cb = std::move(cb)](bool ok, const std::string& text, int tokens,
                              const std::string& providerName) {
            Json::Value resp;
            if (ok) {
                resp["code"] = 200;
                resp["data"]["text"]         = text;
                resp["data"]["model"]        = providerName;
                resp["data"]["total_tokens"] = tokens;
                resp["data"]["fallback"]     = true;
                cb(HttpResponse::newHttpJsonResponse(resp));
            } else {
                resp["code"] = 503;
                resp["msg"]  = std::string("AI 服务不可用: ") + text;
                auto r = HttpResponse::newHttpJsonResponse(resp);
                r->setStatusCode(k503ServiceUnavailable);
                cb(r);
            }
        });
}

void AiCtrl::transcribe(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& cb) {
    auto& svc = WhisperService::instance();
    if (!svc.isReady()) {
        Json::Value e; e["code"] = 503; e["msg"] = "KoboldCpp subprocess not running";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k503ServiceUnavailable);
        return cb(r);
    }

    // 接受 raw PCM float32 body 或 JSON {audio: [float...]}
    std::vector<float> audioData;
    auto ct = req->getHeader("content-type");
    if (ct.find("application/json") != std::string::npos) {
        auto body = req->getJsonObject();
        if (body && (*body).isMember("audio")) {
            for (auto& v : (*body)["audio"])
                audioData.push_back(v.asFloat());
        }
    } else {
        // raw bytes → float32 LE
        const auto& raw = req->getBody();
        size_t n = raw.size() / sizeof(float);
        audioData.resize(n);
        memcpy(audioData.data(), raw.data(), n * sizeof(float));
    }

    if (audioData.empty()) {
        Json::Value e; e["msg"] = "No audio data";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }

    std::string text = svc.transcribe(audioData);
    Json::Value resp;
    resp["code"] = 200;
    resp["data"]["text"] = text;
    cb(HttpResponse::newHttpJsonResponse(resp));
}

void AiCtrl::health(const HttpRequestPtr&,
                     std::function<void(const HttpResponsePtr&)>&& cb) {
    Json::Value resp;
    resp["code"] = 200;
    bool koboldReady   = KoboldCppService::instance().isReady();
    bool whisperReady  = WhisperService::instance().isReady();
    auto fbCfg         = loadFallbackCfg();
    resp["data"]["koboldcpp"]     = koboldReady;
    resp["data"]["whisper"]       = whisperReady;
    resp["data"]["fallback_enabled"] = fbCfg.enabled;
    Json::Value provs(Json::arrayValue);
    for (auto& p : fbCfg.providers) provs.append(p.name);
    resp["data"]["fallback_providers"] = provs;
    // 当前生效的对话后端（首选）
    if (koboldReady) {
        resp["data"]["chat_backend"] = "koboldcpp";
    } else if (fbCfg.enabled && !fbCfg.providers.empty()) {
        resp["data"]["chat_backend"] = fbCfg.providers[0].name + " (fallback)";
    } else {
        resp["data"]["chat_backend"] = "none";
    }
    cb(HttpResponse::newHttpJsonResponse(resp));
}

// ── 内置 AI 聊天页 ─────────────────────────────────────────
// GET /ai/page —— 返回完整 HTML 聊天 UI（自包含 CSS+JS，调 /ai/chat 后端）
// sys_menu 里 "AI智能助手" 用 InnerLink 嵌入此 URL
void AiCtrl::page(const HttpRequestPtr&,
                   std::function<void(const HttpResponsePtr&)>&& cb) {
    static const std::string kHtml = R"HTML(<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>AI 智能助手</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "PingFang SC", "Microsoft YaHei", sans-serif; }
body { display: flex; flex-direction: column; background: #f5f7fa; color: #303133; }
.header { padding: 12px 20px; background: #fff; border-bottom: 1px solid #ebeef5; display: flex; align-items: center; justify-content: space-between; }
.header h1 { font-size: 16px; font-weight: 500; }
.backend { font-size: 12px; color: #909399; padding: 4px 10px; background: #f0f2f5; border-radius: 12px; }
.backend.ok { color: #67c23a; background: #f0f9eb; }
.backend.fallback { color: #e6a23c; background: #fdf6ec; }
.backend.err { color: #f56c6c; background: #fef0f0; }
.messages { flex: 1; overflow-y: auto; padding: 20px; display: flex; flex-direction: column; gap: 12px; }
.msg { max-width: 78%; padding: 10px 14px; border-radius: 8px; line-height: 1.55; word-break: break-word; white-space: pre-wrap; }
.msg.user { align-self: flex-end; background: #409eff; color: #fff; }
.msg.bot { align-self: flex-start; background: #fff; border: 1px solid #ebeef5; }
.msg.bot.err { background: #fef0f0; border-color: #fbc4c4; color: #f56c6c; }
.msg.bot .meta { font-size: 11px; color: #909399; margin-top: 4px; display: block; }
.input-bar { background: #fff; border-top: 1px solid #ebeef5; padding: 12px 20px; display: flex; gap: 10px; }
.input-bar textarea { flex: 1; resize: none; padding: 10px 12px; border: 1px solid #dcdfe6; border-radius: 6px; font-size: 14px; line-height: 1.5; outline: none; height: 64px; font-family: inherit; }
.input-bar textarea:focus { border-color: #409eff; }
.input-bar button { padding: 0 22px; background: #409eff; color: #fff; border: 0; border-radius: 6px; cursor: pointer; font-size: 14px; }
.input-bar button:disabled { background: #a0cfff; cursor: not-allowed; }
.input-bar button:hover:not(:disabled) { background: #66b1ff; }
.empty-tip { text-align: center; color: #c0c4cc; padding: 60px 20px; font-size: 14px; }
.empty-tip .emoji { font-size: 48px; margin-bottom: 12px; }
.thinking { display: inline-block; }
.thinking::after { content: '...'; animation: dots 1.4s infinite; }
@keyframes dots { 0% { content: '.'; } 33% { content: '..'; } 66% { content: '...'; } }
</style>
</head>
<body>
<div class="header">
  <h1> AI 智能助手</h1>
  <span class="backend" id="backend">连接中…</span>
</div>
<div class="messages" id="messages">
  <div class="empty-tip">
    <div class="emoji">💬</div>
    <div>给 AI 发个消息开始对话吧</div>
  </div>
</div>
<div class="input-bar">
  <textarea id="input" placeholder="输入消息，Ctrl+Enter 发送" autofocus></textarea>
  <button id="send">发送</button>
</div>
<script>
(function() {
  var msgsEl = document.getElementById('messages');
  var inputEl = document.getElementById('input');
  var sendEl = document.getElementById('send');
  var backendEl = document.getElementById('backend');
  var emptyTip = msgsEl.querySelector('.empty-tip');
  function add(text, who, meta, isErr) {
    if (emptyTip) { emptyTip.remove(); emptyTip = null; }
    var el = document.createElement('div');
    el.className = 'msg ' + who + (isErr ? ' err' : '');
    el.textContent = text;
    if (meta) {
      var m = document.createElement('span');
      m.className = 'meta';
      m.textContent = meta;
      el.appendChild(m);
    }
    msgsEl.appendChild(el);
    msgsEl.scrollTop = msgsEl.scrollHeight;
    return el;
  }
  function checkHealth() {
    // 相对路径：iframe 在 /dev-api/ai/page 下时，'health' 解析到 /dev-api/ai/health
    // 经前端 vue-cli proxy 转发到后端 /ai/health；合并部署时同源直达 /ai/health
    fetch('health').then(function(r){ return r.json(); }).then(function(j){
      var be = (j && j.data && j.data.chat_backend) || 'none';
      if (be === 'koboldcpp') {
        backendEl.textContent = '本地 KoboldCpp ✓';
        backendEl.className = 'backend ok';
      } else if (be === 'none') {
        backendEl.textContent = '无可用 AI 后端';
        backendEl.className = 'backend err';
      } else {
        backendEl.textContent = be;
        backendEl.className = 'backend fallback';
      }
    }).catch(function(){
      backendEl.textContent = '连接失败';
      backendEl.className = 'backend err';
    });
  }
  function send() {
    var text = inputEl.value.trim();
    if (!text) return;
    add(text, 'user');
    inputEl.value = '';
    sendEl.disabled = true;
    var thinking = add('', 'bot');
    var t = document.createElement('span');
    t.className = 'thinking';
    t.textContent = 'AI 正在思考';
    thinking.appendChild(t);
    fetch('chat', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ message: text })
    }).then(function(r){ return r.json(); }).then(function(j){
      thinking.remove();
      if (j && j.code === 200 && j.data) {
        var meta = j.data.model || '';
        if (j.data.total_tokens) meta += '  · ' + j.data.total_tokens + ' tokens';
        if (j.data.fallback) meta += '  · fallback';
        add(j.data.reply || j.data.text || '(空回复)', 'bot', meta);
      } else {
        add((j && j.msg) || '请求失败', 'bot', null, true);
      }
    }).catch(function(e){
      thinking.remove();
      add('网络错误：' + e, 'bot', null, true);
    }).finally(function(){
      sendEl.disabled = false;
      inputEl.focus();
    });
  }
  sendEl.addEventListener('click', send);
  inputEl.addEventListener('keydown', function(e){
    if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) { e.preventDefault(); send(); }
  });
  checkHealth();
})();
</script>
</body>
</html>
)HTML";

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->addHeader("Cache-Control", "no-store");
    // 允许在 ruoyi InnerLink iframe 中嵌入
    resp->addHeader("X-Frame-Options", "SAMEORIGIN");
    resp->setBody(kHtml);
    cb(resp);
}
