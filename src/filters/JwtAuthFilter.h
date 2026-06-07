/**
 * @file JwtAuthFilter.h
 * @brief JWT 认证中间件 — 请求认证和授权
 * 
 * 功能概述：
 *   - JWT 验证：验证请求中的 JWT token
 *   - API Key 认证：支持 API Key 作为 JWT 的备选认证方式
 *   - Token 绑定：支持 IP 和 User-Agent 绑定防止 token 盗用
 *   - Bot 检测：检测和阻止爬虫和恶意工具
 *   - 权限检查：验证用户权限和数据权限
 * 
 * 认证流程：
 *   1. Bot UA 检测（第二道防线，第一道在 nginx）
 *   2. Token 提取（Header 或 Query 参数）
 *   3. Token 验证（JWT 签名、过期时间）
 *   4. Token 绑定验证（IP、User-Agent）
 *   5. API Key 备选认证（如果 JWT 失败）
 *   6. 权限检查（基于权限字符串）
 * 
 * Token 提取位置：
 *   - Header: Authorization: Bearer <token>
 *   - Header: X-Access-Token: <token>
 *   - Query: ?token=<token>
 *   - Query: ?accessToken=<token>
 * 
 * API Key 认证：
 *   - Header: X-API-Key: <key>
 *   - Query: ?apiKey=<key>
 * 
 * Token 绑定配置（config.json）：
 *   {
 *     "security": {
 *       "token_binding": {
 *         "check_ip": false,   // 是否绑定 IP（移动端 IP 易变，默认关）
 *         "check_ua": true     // 是否绑定 User-Agent（防止 token 盗用，默认开）
 *       }
 *     }
 *   }
 * 
 * Bot 检测特征：
 *   - Python requests、Scrapy、curl、wget 等工具
 *   - 爬虫工具：zgrab、masscan、sqlmap、nikto 等
 *   - 自动化工具：httpclient、httpie 等
 * 
 * 使用示例：
 *   // 在 drogon 中注册中间件
 *   app.registerMiddleware<JwtAuthFilter>();
 *   
 *   // 在控制器中使用 JwtAuthFilter
 *   ADD_METHOD_TO(MyCtrl::myMethod, "/api/path", drogon::Get, "JwtAuthFilter");
 * 
 * 特性：
 *   - 多认证方式：支持 JWT 和 API Key
 *   - 防盗用：支持 Token 绑定（IP、User-Agent）
 *   - Bot 防护：检测和阻止爬虫和恶意工具
 *   - 缓存优化：使用 TokenCache 缓存验证结果
 *   - 灵活配置：支持通过 config.json 配置
 * 
 * 配置项（config.json）：
 *   - security.token_binding.check_ip: 是否绑定 IP（默认 false）
 *   - security.token_binding.check_ua: 是否绑定 User-Agent（默认 true）
 *   - jwt.secret: JWT 密钥
 *   - jwt.ttl: JWT 过期时间（秒）
 */

#pragma once
#include <drogon/HttpMiddleware.h>
#include <algorithm>
#include <fstream>
#include <json/json.h>
#include "../common/JwtUtils.h"
#include "../common/TokenCache.h"
#include "../common/SecurityUtils.h"
#include "../common/AjaxResult.h"
#include "../common/UaUtils.h"
#include "../common/ApiKeyService.h"
#include "../common/IpUtils.h"
#include "../system/services/TokenService.h"

/**
 * @struct AntiAbuseConfig
 * @brief 防滥用配置
 * 
 * 配置 Token 绑定和防护参数。
 * 启动时从 config.json 加载一次，之后使用单例模式。
 */
struct AntiAbuseConfig {
    bool checkIp = false;  // IP 绑定：移动端 IP 易变，默认关
    bool checkUa = true;   // UA 绑定：防止 token 被抓包盗用，默认开

    static const AntiAbuseConfig& get() {
        static AntiAbuseConfig cfg = [] {
            AntiAbuseConfig c;
            try {
                std::ifstream f("config.json");
                if (!f.is_open()) return c;
                Json::Value root; Json::CharReaderBuilder rb; std::string e;
                if (Json::parseFromStream(rb, f, &root, &e)
                    && root.isMember("security")
                    && root["security"].isMember("token_binding")) {
                    auto& tb = root["security"]["token_binding"];
                    c.checkIp = tb.get("check_ip", false).asBool();
                    c.checkUa = tb.get("check_ua", true).asBool();
                }
            } catch (...) {}
            return c;
        }();
        return cfg;
    }
};

// ── Bot UA 特征检测（nginx 绕过时的第二道防线）──────────────────────────────
inline bool isBotUserAgent(const std::string& ua) {
    if (ua.empty()) return true;
    std::string low = ua;
    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    static const char* sigs[] = {
        "python-requests", "scrapy", "curl/", "wget/", "go-http-client",
        "java/", "okhttp", "libwww-perl", "zgrab", "masscan",
        "sqlmap", "nikto", "dirbuster", "httpclient", "httpie",
        nullptr
    };
    for (int i = 0; sigs[i]; ++i)
        if (low.find(sigs[i]) != std::string::npos) return true;
    return false;
}

// JWT 认证中间件（对应 java的JwtAuthorizationFilter）
class JwtAuthFilter : public drogon::HttpMiddleware<JwtAuthFilter> {
public:
    static constexpr bool isAutoCreation = false;

    void invoke(const drogon::HttpRequestPtr &req,
                drogon::MiddlewareNextCallback &&nextCb,
                drogon::MiddlewareCallback &&mcb) override {

        // 1. Bot UA 检测（直连 18080 绕过 nginx 时的后端防线）
        std::string ua = req->getHeader("User-Agent");
        if (isBotUserAgent(ua)) {
            LOG_WARN << "[Security] Bot UA blocked: "
                     << ua.substr(0, 80) << " ip=" << req->peerAddr().toIp()
                     << " path=" << req->path();
            mcb(drogon::HttpResponse::newHttpJsonResponse(
                AjaxResult::error(403, "非法请求")));
            return;
        }

        // 2. Token 提取
        auto token = SecurityUtils::getToken(req);
        if (token.empty()) {
            // ── f16 API Key fallback：X-API-Key / ?apiKey= ──────────────
            std::string apiKey = req->getHeader("X-API-Key");
            if (apiKey.empty()) apiKey = req->getParameter("apiKey");
            if (!apiKey.empty()) {
                std::string err;
                auto u = ApiKeyService::instance().verifyKey(apiKey, &err);
                if (!u) {
                    LOG_WARN << "[ApiKey] verify failed: " << err << " path=" << req->path();
                    mcb(drogon::HttpResponse::newHttpJsonResponse(
                        AjaxResult::error(401, err.empty() ? "API Key 无效" : err)));
                    return;
                }
                // 注入虚拟登录用户；ApiKey 不做 IP/UA 绑定（设计上是无状态访问）
                req->getAttributes()->insert("userId",   u->userId);
                req->getAttributes()->insert("deptId",   u->deptId);
                req->getAttributes()->insert("userName", u->userName);
                req->getAttributes()->insert("uuid",     u->token);
                req->getAttributes()->insert("loginUser", *u);
                nextCb(std::move(mcb));
                return;
            }
            mcb(drogon::HttpResponse::newHttpJsonResponse(
                AjaxResult::error(401, "请求未携带token，无法访问系统资源")));
            return;
        }

        try {
            auto uuid    = JwtUtils::parseUuid(token);
            auto userKey = SecurityUtils::getTokenKey(uuid);
            auto userOpt = TokenCache::instance().get(userKey);

            if (!userOpt) {
                mcb(drogon::HttpResponse::newHttpJsonResponse(
                    AjaxResult::error(401, "登录状态已过期，请重新登录")));
                return;
            }

            auto &user = *userOpt;

            // 3. Token 绑定校验（防止 token 被抓包后在其他设备重放）
            const auto& anti = AntiAbuseConfig::get();
            std::string curIp    = IpUtils::getIpAddr(req); // 反向代理场景下读 X-Forwarded-For，与登录时一致
            std::string curBrowser = UaUtils::parse(ua).browser;

            if (anti.checkIp && !user.ipAddr.empty() && user.ipAddr != curIp) {
                LOG_WARN << "[Security] Token IP mismatch: stored=" << user.ipAddr
                         << " current=" << curIp << " user=" << user.userName;
                mcb(drogon::HttpResponse::newHttpJsonResponse(
                    AjaxResult::error(401, "检测到异常访问，请重新登录")));
                return;
            }
            if (anti.checkUa && !user.browser.empty() && user.browser != curBrowser) {
                LOG_WARN << "[Security] Token UA mismatch: user=" << user.userName
                         << " stored=" << user.browser << " current=" << curBrowser;
                mcb(drogon::HttpResponse::newHttpJsonResponse(
                    AjaxResult::error(401, "检测到异常访问，请重新登录")));
                return;
            }

            // 4. 自动刷新：剩余不足20分钟时延长（同时写透 sys_token，重启后不丢续期）
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            long remaining = user.expireTime - now;
            if (remaining > 0 && remaining < 20LL * 60 * 1000) {
                TokenService::instance().refreshToken(user);
            }

            // 5. 将用户信息注入请求属性，供 Controller 使用
            req->getAttributes()->insert("userId",   user.userId);
            req->getAttributes()->insert("deptId",   user.deptId);
            req->getAttributes()->insert("userName", user.userName);
            req->getAttributes()->insert("uuid",     uuid);
            req->getAttributes()->insert("loginUser", *userOpt);

            nextCb(std::move(mcb));
        } catch (const std::exception &e) {
            mcb(drogon::HttpResponse::newHttpJsonResponse(
                AjaxResult::error(401, std::string("token异常") + e.what())));
        }
    }
};
