/**
 * @file SmsService.h
 * @brief 短信通道服务 — 阿里云/腾讯云签名发送 + 验证码 + 模板管理
 *
 * 功能概述：
 *   - 双厂商：阿里云 dysmsapi（HMAC-SHA1 签名 GET）/ 腾讯云 sms（TC3-HMAC-SHA256 POST）
 *   - 验证码：6位随机码，MemCache/Redis 存储（sms:code:<phone>），TTL 可配
 *   - 模板管理：sys_sms_template 表（provider/template_code/sign_name/content）
 *   - 发送日志：sys_sms_log 表（phone/provider/status/response）
 *   - 异步发送：HttpCaller 异步 HTTP，不阻塞业务线程
 *
 * 配置项（config.json → sms）：
 *   - enabled: 总开关（默认 false）
 *   - provider: "aliyun"|"tencent"（默认 aliyun）
 *   - aliyun.access_key_id / access_key_secret / sign_name / endpoint
 *   - tencent.secret_id / secret_key / sdk_app_id / sign_name / endpoint / region
 *   - code_ttl: 验证码有效期秒（默认 300）
 *   - code_length: 验证码位数（默认 6）
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include <drogon/utils/Utilities.h>   // getUuid
#include "../common/HttpCaller.h"
#include "../common/TokenCache.h"   // MemCache
#include "../common/SignUtils.h"    // hmacSha256Hex/sha256Hex/toHex
#include "../services/DatabaseService.h"

/**
 * @class SmsService
 * @brief 短信服务单例
 */
class SmsService {
public:
    struct Config {
        bool        enabled    = false;
        std::string provider   = "aliyun";
        // aliyun
        std::string aliAkId, aliAkSecret, aliSignName;
        std::string aliEndpoint = "https://dysmsapi.aliyuncs.com";
        // tencent
        std::string tcSecretId, tcSecretKey, tcSdkAppId, tcSignName;
        std::string tcEndpoint  = "https://sms.tencentcloudapi.com";
        std::string tcRegion    = "ap-guangzhou";
        // code
        int codeTtl    = 300;
        int codeLength = 6;
    };

    static SmsService& instance() {
        static SmsService inst;
        return inst;
    }

    void init(const Json::Value& cfg) {
        cfg_.enabled  = cfg.get("enabled", false).asBool();
        cfg_.provider = cfg.get("provider", "aliyun").asString();
        auto& a = cfg["aliyun"];
        cfg_.aliAkId      = a.get("access_key_id", "").asString();
        cfg_.aliAkSecret  = a.get("access_key_secret", "").asString();
        cfg_.aliSignName  = a.get("sign_name", "").asString();
        cfg_.aliEndpoint  = a.get("endpoint", cfg_.aliEndpoint).asString();
        auto& t = cfg["tencent"];
        cfg_.tcSecretId   = t.get("secret_id", "").asString();
        cfg_.tcSecretKey  = t.get("secret_key", "").asString();
        cfg_.tcSdkAppId   = t.get("sdk_app_id", "").asString();
        cfg_.tcSignName   = t.get("sign_name", "").asString();
        cfg_.tcEndpoint   = t.get("endpoint", cfg_.tcEndpoint).asString();
        cfg_.tcRegion     = t.get("region", cfg_.tcRegion).asString();
        cfg_.codeTtl      = cfg.get("code_ttl", 300).asInt();
        cfg_.codeLength   = cfg.get("code_length", 6).asInt();
        if (cfg_.enabled)
            LOG_INFO << "[SMS] enabled provider=" << cfg_.provider;
    }

    bool isEnabled() const { return cfg_.enabled; }

    // ── 验证码 ────────────────────────────────────────────────────────

    /// 生成并发送验证码（异步发送，立即返回 code 供测试/日志）
    std::string sendCode(const std::string& phone, const std::string& templateCode) {
        std::string code = genCode(cfg_.codeLength);
        MemCache::instance().setString("sms:code:" + phone, code, cfg_.codeTtl);
        Json::Value params; params["code"] = code;
        send(phone, templateCode, Json::writeString(Json::StreamWriterBuilder(), params));
        return code;
    }

    /// 校验验证码（一次性，校验通过即删除）
    bool verifyCode(const std::string& phone, const std::string& code) {
        auto stored = MemCache::instance().getString("sms:code:" + phone);
        if (!stored || *stored != code) return false;
        MemCache::instance().remove("sms:code:" + phone);
        return true;
    }

    // ── 发送 ──────────────────────────────────────────────────────────

    /**
     * @brief 发送短信（异步，结果写 sys_sms_log）
     * @param phone 手机号
     * @param templateCode 模板编码（厂商侧模板ID）
     * @param templateParamJson 模板参数 JSON 串（如 {"code":"123456"}）
     */
    void send(const std::string& phone, const std::string& templateCode,
              const std::string& templateParamJson) {
        if (!cfg_.enabled) { logSend(phone, templateCode, "disabled", "sms disabled"); return; }
        if (cfg_.provider == "tencent") sendTencent(phone, templateCode, templateParamJson);
        else                            sendAliyun(phone, templateCode, templateParamJson);
    }

private:
    SmsService() = default;

    // ── 阿里云 dysmsapi（HMAC-SHA1 签名 GET）───────────────────────────
    void sendAliyun(const std::string& phone, const std::string& tplCode,
                    const std::string& tplParam) {
        std::map<std::string, std::string> p;
        p["Action"]           = "SendSms";
        p["Version"]          = "2017-05-25";
        p["PhoneNumbers"]     = phone;
        p["SignName"]         = cfg_.aliSignName;
        p["TemplateCode"]     = tplCode;
        p["TemplateParam"]    = tplParam;
        p["AccessKeyId"]      = cfg_.aliAkId;
        p["SignatureMethod"]  = "HMAC-SHA1";
        p["SignatureVersion"] = "1.0";
        p["SignatureNonce"]   = drogon::utils::getUuid();
        p["Format"]           = "JSON";
        p["RegionId"]         = "cn-hangzhou";
        // ISO8601 UTC 时间戳
        std::time_t now = std::time(nullptr);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
        p["Timestamp"] = ts;

        // 排序 + RFC3986 编码拼接
        std::string canonical;
        for (auto& [k, v] : p) {
            if (!canonical.empty()) canonical += "&";
            canonical += urlEncode(k) + "=" + urlEncode(v);
        }
        std::string toSign = "GET&%2F&" + urlEncode(canonical);
        // HMAC-SHA1(secret + "&", toSign) → base64
        std::string sign = base64Encode(
            hmacRaw(EVP_sha1(), cfg_.aliAkSecret + "&", toSign));
        std::string url = cfg_.aliEndpoint + "/?Signature=" + urlEncode(sign)
                        + "&" + canonical;

        HttpCaller::asyncGet(url,
            [this, phone, tplCode](bool ok, int status, const std::string& body) {
                bool success = ok && status == 200 && body.find("\"Code\":\"OK\"") != std::string::npos;
                logSend(phone, tplCode, success ? "success" : "fail",
                        "HTTP " + std::to_string(status) + " " + body.substr(0, 500));
            });
    }

    // ── 腾讯云 sms（TC3-HMAC-SHA256 POST）──────────────────────────────
    void sendTencent(const std::string& phone, const std::string& tplCode,
                     const std::string& tplParam) {
        std::time_t now = std::time(nullptr);
        char dateBuf[16];
        std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", std::gmtime(&now));
        std::string date = dateBuf;
        std::string timestamp = std::to_string(now);

        // 请求体
        Json::Value body;
        body["PhoneNumberSet"].append("+86" + phone);
        body["SmsSdkAppId"]  = cfg_.tcSdkAppId;
        body["SignName"]     = cfg_.tcSignName;
        body["TemplateId"]   = tplCode;
        // TemplateParam 是数组：{"code":"123"} → ["123"]
        Json::Value tp; std::string err;
        Json::CharReaderBuilder rb;
        std::istringstream ss(tplParam);
        if (Json::parseFromStream(rb, ss, &tp, &err) && tp.isObject())
            for (auto& k : tp.getMemberNames()) body["TemplateParamSet"].append(tp[k].asString());
        std::string payload = Json::writeString(Json::StreamWriterBuilder(), body);

        // TC3 签名
        std::string host = "sms.tencentcloudapi.com";
        std::string canonicalHeaders = "content-type:application/json; charset=utf-8\nhost:" + host + "\n";
        std::string signedHeaders = "content-type;host";
        std::string canonicalReq = "POST\n/\n\n" + canonicalHeaders + signedHeaders
                                 + "\n" + SignUtils::sha256Hex(payload);
        std::string credentialScope = date + "/sms/tc3_request";
        std::string toSign = "TC3-HMAC-SHA256\n" + timestamp + "\n" + credentialScope
                           + "\n" + SignUtils::sha256Hex(canonicalReq);
        auto kDate    = hmacRaw(EVP_sha256(), "TC3" + cfg_.tcSecretKey, date);
        auto kService = hmacRaw(EVP_sha256(), kDate, "sms");
        auto kSigning = hmacRaw(EVP_sha256(), kService, "tc3_request");
        std::string signature = SignUtils::toHex(
            (const unsigned char*)hmacRaw(EVP_sha256(), kSigning, toSign).data(), 32);

        std::string auth = "TC3-HMAC-SHA256 Credential=" + cfg_.tcSecretId + "/" + credentialScope
                         + ", SignedHeaders=" + signedHeaders + ", Signature=" + signature;

        // 腾讯云 API 3.0：签名走 Authorization 头 + X-TC-* 头
        std::vector<std::pair<std::string, std::string>> headers = {
            {"Authorization",  auth},
            {"X-TC-Action",    "SendSms"},
            {"X-TC-Version",   "2021-01-11"},
            {"X-TC-Timestamp", timestamp},
            {"X-TC-Region",    cfg_.tcRegion},
        };
        HttpCaller::asyncPost(cfg_.tcEndpoint + "/", payload,
            "application/json; charset=utf-8", headers,
            [this, phone, tplCode](bool ok, int status, const std::string& respBody) {
                bool success = ok && status == 200 &&
                               respBody.find("\"SendStatusSet\"") != std::string::npos;
                logSend(phone, tplCode, success ? "success" : "fail",
                        "HTTP " + std::to_string(status) + " " + respBody.substr(0, 500));
            });
    }

    // ── 发送日志 ──────────────────────────────────────────────────────
    void logSend(const std::string& phone, const std::string& tplCode,
                 const std::string& status, const std::string& response) {
        DatabaseService::instance().execParams(
            "INSERT INTO sys_sms_log(phone, template_code, provider, status, response) "
            "VALUES($1,$2,$3,$4,$5)",
            {phone, tplCode, cfg_.provider, status, response});
    }

    // ── 工具 ──────────────────────────────────────────────────────────

    static std::string genCode(int len) {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> d(0, 9);
        std::string s;
        for (int i = 0; i < len; ++i) s += char('0' + d(rng));
        return s;
    }

    /// RFC3986 URL 编码（阿里云签名要求）
    static std::string urlEncode(const std::string& s) {
        std::ostringstream oss;
        for (unsigned char c : s) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                oss << c;
            else
                oss << '%' << std::uppercase << std::hex << std::setw(2)
                    << std::setfill('0') << (int)c;
        }
        return oss.str();
    }

    /// HMAC 原始字节（供链式签名）
    static std::string hmacRaw(const EVP_MD* md, const std::string& key,
                               const std::string& data) {
        unsigned char buf[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        HMAC(md, key.data(), (int)key.size(),
             (const unsigned char*)data.data(), (int)data.size(), buf, &len);
        return std::string((const char*)buf, len);
    }

    /// Base64 编码
    static std::string base64Encode(const std::string& in) {
        static const char* tbl =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        int val = 0, bits = -6;
        for (unsigned char c : in) {
            val = (val << 8) + c; bits += 8;
            while (bits >= 0) { out += tbl[(val >> bits) & 0x3F]; bits -= 6; }
        }
        if (bits > -6) out += tbl[((val << 8) >> (bits + 8)) & 0x3F];
        while (out.size() % 4) out += '=';
        return out;
    }

    Config cfg_;
};
