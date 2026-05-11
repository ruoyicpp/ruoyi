#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <trantor/utils/Logger.h>
#include "../services/DatabaseService.h"

// f15 消息通知中心服务
//
// 支持的渠道类型（channel_type 字段）：
//   * "dingtalk"  钉钉自定义机器人（加签：HMAC-SHA256 + Base64 URL-encode）
//   * "feishu"    飞书自定义机器人（加签：HMAC-SHA256 over "timestamp\n<secret>" → Base64）
//   * "wxwork"    企业微信机器人（无签名，URL key 即凭证）
//   * "webhook"   通用 webhook（X-Signature: HMAC-SHA256-hex(secret, body)）
//
// 统一对外接口：sendToChannel(channelId, title, content)
namespace NotifyService {

// ── HMAC-SHA256 helpers ─────────────────────────────────────────────────────
inline std::vector<unsigned char> hmacSha256Raw(const std::string& key, const std::string& data) {
    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int  len = 0;
    HMAC(EVP_sha256(),
         key.data(),  (int)key.size(),
         (const unsigned char*)data.data(), (int)data.size(),
         buf, &len);
    return {buf, buf + len};
}

inline std::string toHex(const std::vector<unsigned char>& bytes) {
    static const char* hex = "0123456789abcdef";
    std::string out; out.reserve(bytes.size() * 2);
    for (auto b : bytes) {
        out += hex[b >> 4];
        out += hex[b & 0xF];
    }
    return out;
}

inline std::string base64Encode(const std::vector<unsigned char>& bytes) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    BIO_write(bio, bytes.data(), (int)bytes.size());
    BIO_flush(bio);
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    std::string out(mem->data, mem->length);
    BIO_free_all(bio);
    return out;
}

// 钉钉加签：sign = urlEncode(base64(HMAC-SHA256(secret, timestamp + "\n" + secret)))
inline std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out; out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

// ── 各渠道适配器（构造 webhook payload + 必要 query 参数）────────────────────

// 钉钉：Markdown / Text 消息 + 加签
inline std::pair<std::string, std::string> buildDingtalk(const std::string& webhookUrl,
                                                         const std::string& secret,
                                                         const std::string& title,
                                                         const std::string& content) {
    long long ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string url = webhookUrl;
    if (!secret.empty()) {
        std::string stringToSign = std::to_string(ts) + "\n" + secret;
        auto raw = hmacSha256Raw(secret, stringToSign);
        std::string sign = urlEncode(base64Encode(raw));
        url += (webhookUrl.find('?') == std::string::npos ? "?" : "&")
             + std::string("timestamp=") + std::to_string(ts) + "&sign=" + sign;
    }
    Json::Value body;
    body["msgtype"] = "markdown";
    body["markdown"]["title"] = title;
    body["markdown"]["text"]  = "### " + title + "\n\n" + content;
    Json::StreamWriterBuilder wb; wb["indentation"] = "";
    return {url, Json::writeString(wb, body)};
}

// 飞书：text 消息 + 加签
inline std::pair<std::string, std::string> buildFeishu(const std::string& webhookUrl,
                                                       const std::string& secret,
                                                       const std::string& title,
                                                       const std::string& content) {
    Json::Value body;
    body["msg_type"] = "text";
    body["content"]["text"] = "[" + title + "] " + content;
    if (!secret.empty()) {
        long long ts = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string stringToSign = std::to_string(ts) + "\n" + secret;
        auto raw = hmacSha256Raw(stringToSign, "");   // 飞书算法：以 "ts\n<secret>" 为 key，data 空
        body["timestamp"] = std::to_string(ts);
        body["sign"]      = base64Encode(raw);
    }
    Json::StreamWriterBuilder wb; wb["indentation"] = "";
    return {webhookUrl, Json::writeString(wb, body)};
}

// 企业微信机器人：text 消息（无签名，URL key 即凭证）
inline std::pair<std::string, std::string> buildWxwork(const std::string& webhookUrl,
                                                       const std::string& /*secret*/,
                                                       const std::string& title,
                                                       const std::string& content) {
    Json::Value body;
    body["msgtype"] = "text";
    body["text"]["content"] = "[" + title + "] " + content;
    Json::StreamWriterBuilder wb; wb["indentation"] = "";
    return {webhookUrl, Json::writeString(wb, body)};
}

// 通用 webhook：附加 X-Signature: hex(HMAC-SHA256(secret, body)) 头由调用方设置
inline std::pair<std::string, std::string> buildGeneric(const std::string& webhookUrl,
                                                        const std::string& /*secret*/,
                                                        const std::string& title,
                                                        const std::string& content) {
    Json::Value body;
    body["title"]     = title;
    body["content"]   = content;
    body["timestamp"] = (Json::Int64)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    Json::StreamWriterBuilder wb; wb["indentation"] = "";
    return {webhookUrl, Json::writeString(wb, body)};
}

// 解析 URL "https://host[:port]/path" → (baseUrl, path)
inline std::pair<std::string, std::string> splitUrl(const std::string& url) {
    auto schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return {url, "/"};
    auto pathStart = url.find('/', schemeEnd + 3);
    if (pathStart == std::string::npos) return {url, "/"};
    return {url.substr(0, pathStart), url.substr(pathStart)};
}

// ── 发送（异步）────────────────────────────────────────────────────────────
// 返回 true 表示请求已发出（不保证渠道侧成功）
inline bool sendToChannel(long channelId,
                          const std::string& title,
                          const std::string& content) {
    auto& db = DatabaseService::instance();
    auto res = db.queryParams(
        "SELECT channel_type, webhook_url, secret, enabled "
        "FROM sys_notify_channel WHERE id=$1",
        {std::to_string(channelId)});
    if (!res.ok() || res.rows() == 0) {
        LOG_WARN << "[Notify] channel not found id=" << channelId; return false;
    }
    std::string type = res.str(0, 0);
    std::string url  = res.str(0, 1);
    std::string sec  = res.str(0, 2);
    if (res.intVal(0, 3) == 0) {
        LOG_WARN << "[Notify] channel disabled id=" << channelId; return false;
    }

    std::string finalUrl, body;
    std::string extraHeader;          // 通用 webhook 的 X-Signature 头
    if (type == "dingtalk") {
        auto p = buildDingtalk(url, sec, title, content); finalUrl = p.first; body = p.second;
    } else if (type == "feishu") {
        auto p = buildFeishu(url, sec, title, content); finalUrl = p.first; body = p.second;
    } else if (type == "wxwork") {
        auto p = buildWxwork(url, sec, title, content); finalUrl = p.first; body = p.second;
    } else {
        auto p = buildGeneric(url, sec, title, content); finalUrl = p.first; body = p.second;
        if (!sec.empty()) extraHeader = toHex(hmacSha256Raw(sec, body));
    }

    auto [base, path] = splitUrl(finalUrl);
    auto client = drogon::HttpClient::newHttpClient(base);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath(path);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body);
    if (!extraHeader.empty()) req->addHeader("X-Signature", extraHeader);

    client->sendRequest(req,
        [channelId, title](drogon::ReqResult r, const drogon::HttpResponsePtr& resp) {
            if (r != drogon::ReqResult::Ok) {
                LOG_WARN << "[Notify] channel=" << channelId << " send failed (network)"; return;
            }
            int code = resp->getStatusCode();
            if (code >= 200 && code < 300) {
                LOG_INFO << "[Notify] channel=" << channelId << " title='" << title << "' delivered";
            } else {
                LOG_WARN << "[Notify] channel=" << channelId << " HTTP " << code
                         << " body=" << resp->getBody().substr(0, 200);
            }
        }, 10.0);
    return true;
}

// ── 站内消息：写 sys_message 表 ──────────────────────────────────────────
inline bool sendInbox(long userId,
                      const std::string& title,
                      const std::string& content,
                      const std::string& level = "info") {
    return DatabaseService::instance().execParams(
        "INSERT INTO sys_message(user_id, title, content, level) "
        "VALUES($1, $2, $3, $4)",
        {std::to_string(userId), title, content, level});
}

}   // namespace NotifyService
