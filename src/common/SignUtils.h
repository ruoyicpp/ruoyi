/**
 * @file SignUtils.h
 * @brief 接口签名工具 — 请求签名验证
 * 
 * 功能概述：
 *   - 签名生成：生成请求签名
 *   - 签名验证：验证请求签名的有效性
 *   - 时间戳验证：防止过期请求重放
 *   - Nonce 验证：防止请求重放攻击
 *   - App 管理：管理多个应用的密钥
 * 
 * 签名算法：
 *   签名内容 = appId + "\n" + timestamp + "\n" + nonce + "\n" + path + "\n" + md5(body)
 *   签名值 = HMAC-SHA256(secret, 签名内容)
 * 
 * 请求头要求：
 *   - X-App-Id: 应用标识（必需）
 *   - X-Timestamp: Unix 时间戳（秒，必需）
 *   - X-Nonce: 随机字符串，每次唯一（必需）
 *   - X-Sign: 签名值（必需）
 * 
 * 使用示例：
 *   // 配置应用列表
 *   std::vector<SignUtils::AppInfo> apps = {
 *       {"app1", "secret1", true},
 *       {"app2", "secret2", true}
 *   };
 *   SignUtils::instance().configure(apps, 300);  // 时间戳容差 300 秒
 *   
 *   // 验证请求签名
 *   std::string errMsg;
 *   if (!SignUtils::instance().verify(req, errMsg)) {
 *       LOG_WARN << "签名验证失败: " << errMsg;
 *       return;
 *   }
 * 
 * 安全特性：
 *   - 时间戳验证：防止过期请求重放（默认容差 300 秒）
 *   - Nonce 防重放：记录已使用的 nonce，防止重复使用
 *   - HMAC-SHA256：使用强加密算法生成签名
 *   - 多应用支持：支持多个应用的密钥管理
 * 
 * 防护机制：
 *   1. 时间戳验证：请求时间戳与服务器时间偏差超过容差值时拒绝
 *   2. Nonce 防重放：同一 nonce 在容差时间内只能使用一次
 *   3. 签名验证：验证 HMAC-SHA256 签名的正确性
 *   4. 应用验证：验证应用 ID 是否存在且启用
 * 
 * 配置项（config.json）：
 *   {
 *     "sign": {
 *       "enabled": true,
 *       "timestamp_tolerance": 300,
 *       "apps": [
 *         {"appId": "app1", "secret": "secret1", "enabled": true},
 *         {"appId": "app2", "secret": "secret2", "enabled": true}
 *       ]
 *     }
 *   }
 */

#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <drogon/HttpRequest.h>
#include <trantor/utils/Logger.h>

/**
 * @class SignUtils
 * @brief 接口签名工具单例
 * 
 * 提供请求签名生成和验证功能。
 * 支持多应用管理和防重放攻击。
 */
class SignUtils {
public:
    struct AppInfo {
        std::string appId;
        std::string secret;
        bool        enabled = true;
    };

    static SignUtils& instance() {
        static SignUtils inst;
        return inst;
    }

    // 从 config 加载 App 列表
    void configure(const std::vector<AppInfo>& apps, int timestampTolerance = 300) {
        std::lock_guard<std::mutex> lk(mu_);
        apps_.clear();
        for (auto& a : apps) apps_[a.appId] = a;
        tolerance_ = timestampTolerance;
    }

    // 注册单个 App
    void registerApp(const std::string& appId, const std::string& secret) {
        std::lock_guard<std::mutex> lk(mu_);
        apps_[appId] = {appId, secret, true};
    }

    // 验证请求签名，返回 false 时 errMsg 包含原因
    bool verify(const drogon::HttpRequestPtr& req, std::string& errMsg) {
        std::string appId     = req->getHeader("X-App-Id");
        std::string timestamp = req->getHeader("X-Timestamp");
        std::string nonce     = req->getHeader("X-Nonce");
        std::string sign      = req->getHeader("X-Sign");

        if (appId.empty() || timestamp.empty() || nonce.empty() || sign.empty()) {
            errMsg = "缺少签名头 (X-App-Id / X-Timestamp / X-Nonce / X-Sign)";
            return false;
        }

        // ── 时间戳校验（防过期重放）─────────────────────────────────────
        long long ts = 0;
        try { ts = std::stoll(timestamp); } catch (...) {
            errMsg = "X-Timestamp 格式无效"; return false;
        }
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (std::abs(now - ts) > tolerance_) {
            errMsg = "请求已过期（时间戳偏差超过" + std::to_string(tolerance_) + "秒）";
            return false;
        }

        // ── Nonce 防重放（已使用的 nonce 在 tolerance*2 内拒绝）─────────
        std::string nonceKey = appId + ":" + nonce;
        {
            std::lock_guard<std::mutex> lk(mu_);
            cleanExpiredNonces();
            if (usedNonces_.count(nonceKey)) {
                errMsg = "Nonce 已被使用（重放攻击）"; return false;
            }
        }

        // ── 查找 AppSecret ─────────────────────────────────────────────
        std::string secret;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = apps_.find(appId);
            if (it == apps_.end() || !it->second.enabled) {
                errMsg = "App-Id 不存在或已禁用"; return false;
            }
            secret = it->second.secret;
        }

        // ── 计算期望签名 ───────────────────────────────────────────────
        std::string body     = std::string(req->body());
        std::string bodySha  = sha256Hex(body);
        std::string path     = std::string(req->path());
        std::string query    = std::string(req->query());
        std::string message  = appId + "\n" + timestamp + "\n" + nonce + "\n" + path + "\n" + query + "\n" + bodySha;
        std::string expected = hmacSha256Hex(secret, message);

        if (!constTimeEqual(sign, expected)) {
            LOG_WARN << "[SignUtils] Sign mismatch for appId=" << appId
                     << " path=" << path;
            errMsg = "签名验证失败"; return false;
        }

        // ── 记录 Nonce ─────────────────────────────────────────────────
        {
            std::lock_guard<std::mutex> lk(mu_);
            usedNonces_[nonceKey] = now + tolerance_ * 2;
        }
        return true;
    }

    // 生成签名（用于客户端测试/文档示例）
    static std::string generateSign(const std::string& secret,
                                    const std::string& appId,
                                    const std::string& timestamp,
                                    const std::string& nonce,
                                    const std::string& path,
                                    const std::string& query = "",
                                    const std::string& body = "") {
        std::string bodySha = sha256Hex(body);
        std::string message = appId + "\n" + timestamp + "\n" + nonce + "\n" + path + "\n" + query + "\n" + bodySha;
        return hmacSha256Hex(secret, message);
    }

    bool hasApps() const { return !apps_.empty(); }

private:
    std::unordered_map<std::string, AppInfo>        apps_;
    std::unordered_map<std::string, long long>      usedNonces_; // nonceKey → expireTime
    std::mutex mu_;
    int tolerance_ = 300; // 秒

    SignUtils() = default;

    // 清除过期 nonce（调用时已持有 mu_）
    void cleanExpiredNonces() {
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        for (auto it = usedNonces_.begin(); it != usedNonces_.end(); ) {
            if (it->second < now) it = usedNonces_.erase(it);
            else ++it;
        }
    }

    // HMAC-SHA256 → hex 字符串
    static std::string hmacSha256Hex(const std::string& key, const std::string& data) {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int  digestLen = 0;
        HMAC(EVP_sha256(),
             key.data(),  (int)key.size(),
             (const unsigned char*)data.data(), (int)data.size(),
             digest, &digestLen);
        return toHex(digest, digestLen);
    }

    // SHA-256 → hex 字符串（body hash，比 MD5 更安全）
    static std::string sha256Hex(const std::string& data) {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int  digestLen = 0;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, data.data(), data.size());
        EVP_DigestFinal_ex(ctx, digest, &digestLen);
        EVP_MD_CTX_free(ctx);
        return toHex(digest, digestLen);
    }

    static std::string toHex(const unsigned char* buf, unsigned int len) {
        std::ostringstream oss;
        for (unsigned int i = 0; i < len; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i];
        return oss.str();
    }

    // 常数时间字符串比较（防时序攻击）
    static bool constTimeEqual(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        unsigned char diff = 0;
        for (size_t i = 0; i < a.size(); ++i)
            diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
        return diff == 0;
    }
};
