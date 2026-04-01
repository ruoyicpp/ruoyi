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

// ── 防滥用配置（启动时从 config.json 加载一次）──────────────────────────────
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

// JWT 认证中间件
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
            std::string curIp    = req->peerAddr().toIp();
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

            // 4. 自动刷新：剩余不足20分钟时延长
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            long remaining = user.expireTime - now;
            if (remaining > 0 && remaining < 20LL * 60 * 1000) {
                auto &cfg = JwtUtils::config();
                user.expireTime = now + (long long)cfg.expireMinutes * 60 * 1000;
                TokenCache::instance().update(userKey, user);
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
