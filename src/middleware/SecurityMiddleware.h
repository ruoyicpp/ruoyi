/**
 * @file SecurityMiddleware.h
 * @brief 安全中间件 — 提供 IP 限流、Bot 检测、错误处理等安全防护
 * 
 * 功能概述：
 *   - IP 限流：防止 DDoS 攻击，基于 IP 地址的限流
 *   - Bot 检测：检测和拦截爬虫和自动化工具
 *   - 错误处理：统一的 JSON 格式错误响应
 *   - 安全头：添加安全相关的 HTTP 响应头
 *   - 日志记录：记录安全事件和异常请求
 * 
 * 工作流程：
 *   1. 预路由阶段：检查 IP 限流和 Bot UA
 *   2. 路由处理：正常的路由处理
 *   3. 错误处理：统一的错误响应格式
 *   4. 响应阶段：添加安全响应头
 * 
 * 配置示例（config.json）：
 *   {
 *     "security": {
 *       "rate_limit": {
 *         "enabled": true,
 *         "requests_per_minute": 100,
 *         "burst_size": 20
 *       },
 *       "bot_detection": {
 *         "enabled": true,
 *         "block_list": ["curl", "wget", "python-requests"]
 *       }
 *     }
 *   }
 * 
 * @see RateLimiter - IP 限流器
 * @see IpUtils - IP 地址工具
 * @see MetricsCollector - 性能指标收集器
 */

#pragma once
#include "AppIncludes.h"
#include "common/FrontendState.h"

/**
 * @brief 注册安全中间件
 * 
 * 在应用启动时调用此函数，注册所有安全相关的中间件。
 * 包括 IP 限流、Bot 检测、错误处理等。
 */
inline void registerSecurityMiddleware() {
    /**
     * @brief IP 限流中间件 - 防止 DDoS 攻击
     * 
     * 工作流程：
     *   1. 提取客户端 IP 地址
     *   2. 检查 IP 是否超过限流阈值
     *   3. 如果超过限流，返回 429 Too Many Requests
     *   4. 记录限流事件到指标收集器
     */
    drogon::app().registerPreRoutingAdvice(
        [](const drogon::HttpRequestPtr &req,
           drogon::AdviceCallback &&acb,
           drogon::AdviceChainCallback &&accb) {
            if (feHosted) {
                std::string p = req->path();
                bool isApi = !feApiPrefix.empty() && feApiPrefix != "/" &&
                             p.rfind(feApiPrefix, 0) == 0;
                auto slash = p.find_last_of('/');
                auto seg   = (slash == std::string::npos) ? p : p.substr(slash + 1);
                bool hasExt = seg.find('.') != std::string::npos;
                if (!isApi && hasExt) { accb(); return; }
            }
            std::string ip = IpUtils::getIpAddr(req);
            if (!RateLimiter::instance().allow(ip)) {
                MetricsCollector::instance().onRateLimited();
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode((drogon::HttpStatusCode)429);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"code\":429,\"msg\":\"请求过于频繁，请稍后重试\"}");
                resp->addHeader("Retry-After", "300");
                acb(resp);
                return;
            }
            accb();
        });

    /**
     * @brief Bot UA 检测中间件 - 拦截爬虫和自动化工具
     * 
     * 这是第二道防线，第一道防线在 Nginx 层。
     * 用于绕过 Nginx 直连后端时的防护。
     * 
     * 检测的 Bot 特征：
     *   - Python requests、Scrapy、curl、wget 等工具
     *   - 爬虫工具：zgrab、masscan、sqlmap、nikto 等
     *   - 自动化工具：httpclient、httpie 等
     */
    drogon::app().registerPreRoutingAdvice(
        [](const drogon::HttpRequestPtr &req,
           drogon::AdviceCallback &&acb,
           drogon::AdviceChainCallback &&accb) {
            if (req->method() == drogon::Options) { accb(); return; }
            std::string ua = req->getHeader("User-Agent");
            if (isBotUserAgent(ua)) {
                LOG_WARN << "[Security] Bot UA blocked (global): "
                         << ua.substr(0, 80) << " ip=" << IpUtils::getIpAddr(req)
                         << " path=" << req->path();
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"code\":403,\"msg\":\"非法请求\"}");
                acb(resp);
                return;
            }
            accb();
        });

    // ── 自定义默认错误响应：404/405/500 等也走 AjaxResult JSON 格式 ──
    drogon::app().setCustomErrorHandler(
        [](drogon::HttpStatusCode code,
           const drogon::HttpRequestPtr &req) -> drogon::HttpResponsePtr {
            bool isSpaCode = (code == drogon::k404NotFound ||
                              code == drogon::k405MethodNotAllowed);
            if (isSpaCode && feHosted && feSpaMode) {
                std::string p = req->path();
                bool isApi = !feApiPrefix.empty() && feApiPrefix != "/" &&
                             p.rfind(feApiPrefix, 0) == 0;
                auto slash = p.find_last_of('/');
                auto seg   = (slash == std::string::npos) ? p : p.substr(slash + 1);
                bool hasExt = seg.find('.') != std::string::npos;
                if (!isApi && !hasExt) {
                    if (!feEmbedded && !feIndexPath.empty()) {
                        return drogon::HttpResponse::newFileResponse(feIndexPath);
                    }
                }
            }
            std::string msg;
            int bodyCode = (int)code;
            switch (code) {
                case drogon::k400BadRequest:            msg = "请求参数错误";   break;
                case drogon::k401Unauthorized:          msg = "认证失败";       break;
                case drogon::k403Forbidden:             msg = "无权限访问";     break;
                case drogon::k404NotFound:              msg = "资源不存在";     break;
                case drogon::k405MethodNotAllowed:      msg = "方法不允许";     break;
                case drogon::k413RequestEntityTooLarge: msg = "请求体过大";     break;
                case drogon::k500InternalServerError:   msg = "服务器内部错误"; break;
                case drogon::k502BadGateway:            msg = "网关错误";       break;
                case drogon::k503ServiceUnavailable:    msg = "服务不可用";     break;
                default:                                msg = "请求失败";       break;
            }
            Json::Value j;
            j["code"] = bodyCode;
            j["msg"]  = msg;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(j);
            resp->setStatusCode(code);
            return resp;
        });

    // ── 安全响应头（XSS/点击劫持/内容嗅探防御）────────────────────────────
    drogon::app().registerPostHandlingAdvice(
        [](const drogon::HttpRequestPtr& req,
           const drogon::HttpResponsePtr& resp) {
            resp->addHeader("X-Content-Type-Options",  "nosniff");
            resp->addHeader("X-XSS-Protection",        "1; mode=block");
            resp->addHeader("Referrer-Policy",         "strict-origin-when-cross-origin");
            const std::string& p = req->path();
            bool isAiPage = (p == "/ai" || p == "/ai/" ||
                             (p.size() > 4 && p.compare(0, 4, "/ai/") == 0));
            bool isCertmgrPage = (p == "/certmanager" ||
                                  p.rfind("/certmanager/", 0) == 0);
            if (!isAiPage && !isCertmgrPage) {
                resp->addHeader("X-Frame-Options", "SAMEORIGIN");
                resp->addHeader("Content-Security-Policy",
                    "default-src 'self'; script-src 'self' 'unsafe-inline'; "
                    "style-src 'self' 'unsafe-inline'; img-src 'self' data:");
            } else if (isCertmgrPage) {
                resp->addHeader("X-Frame-Options", "SAMEORIGIN");
                resp->addHeader("Content-Security-Policy",
                    "default-src 'self'; "
                    "script-src 'self' 'unsafe-inline' 'unsafe-eval' "
                        "https://cdn.tailwindcss.com https://cdn.jsdelivr.net; "
                    "style-src 'self' 'unsafe-inline' https://cdn.tailwindcss.com "
                        "https://cdn.jsdelivr.net https://fonts.googleapis.com; "
                    "font-src 'self' https://fonts.gstatic.com; "
                    "img-src 'self' data:;");
            }
        });

    // ── XSS 过滤（POST/PUT 请求 JSON body 净化）──────────────────────────
    drogon::app().registerPreHandlingAdvice(
        [](const drogon::HttpRequestPtr& req,
           drogon::AdviceCallback&&,
           drogon::AdviceChainCallback&& accb) {
            if (req->method() == drogon::Post || req->method() == drogon::Put) {
                auto body = req->getJsonObject();
                if (body) {
                    auto& bv = *body;
                    for (auto& key : bv.getMemberNames()) {
                        if (bv[key].isString()) {
                            const std::string& val = bv[key].asString();
                            if (XssUtils::hasSqlSignature(val)) {
                                LOG_WARN << "[SQLi] suspicious input key=" << key
                                         << " ip=" << req->peerAddr().toIp()
                                         << " path=" << req->path();
                            }
                        }
                    }
                }
            }
            accb();
        });

    // ── 接口验证（公开接口 + server-to-server）────────────────────────────
    drogon::app().registerPreHandlingAdvice(
        [](const drogon::HttpRequestPtr& req,
           drogon::AdviceCallback&& acb,
           drogon::AdviceChainCallback&& accb) {
            auto& sv = SignUtils::instance();
            const std::string& path = req->path();

            if (path.size() >= 4 && path.compare(0, 4, "/ws/") == 0
                && path != "/ws/ticket") {
                if (req->getParameter("token").empty()
                    && req->getParameter("ticket").empty()) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k401Unauthorized);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody(R"({"code":401,"msg":"缺少 token/ticket 参数"})");
                    acb(resp); return;
                }
                accb(); return;
            }

            if (path == "/challenge" || path == "/resetPassword" || path == "/forgotPassword"
                || path == "/health" || path == "/version" || path == "/ssl-config") {
                accb(); return;
            }
            if (path.rfind("/api/ssl/", 0) == 0) { accb(); return; }
            if (path == "/certmanager" || path.rfind("/certmanager/", 0) == 0) { accb(); return; }
            if (path == "/ai/page" || path == "/ai/chat"
                || path == "/ai/health" || path == "/ai/generate") {
                accb(); return;
            }

            std::string appId          = req->getHeader("X-App-Id");
            std::string challengeToken = req->getHeader("X-Challenge-Token");
            bool isPublicRoute = (path == "/login" || path == "/captchaImage"
                               || path == "/register" || path == "/sendRegCode");
            bool hasSignHeader    = !appId.empty();
            bool hasChallengeToken = !challengeToken.empty();

            if (isPublicRoute) {
                if (hasChallengeToken) {
                    auto cacheKey = "challenge:" + challengeToken;
                    auto cached   = MemCache::instance().getString(cacheKey);
                    if (!cached) {
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k403Forbidden);
                        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                        resp->setBody("{\"code\":403,\"msg\":\"挑战令牌无效或已过期\"}");
                        acb(resp); return;
                    }
                    MemCache::instance().remove(cacheKey);
                    accb(); return;
                }
                if (sv.hasApps() && hasSignHeader) {
                    std::string errMsg;
                    if (!sv.verify(req, errMsg)) {
                        LOG_WARN << "[Sign] 验签失败: " << errMsg << " path=" << path
                                 << " ip=" << IpUtils::getIpAddr(req);
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k403Forbidden);
                        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                        resp->setBody("{\"code\":403,\"msg\":\"" + errMsg + "\"}");
                        acb(resp); return;
                    }
                    accb(); return;
                }
                if (!sv.hasApps()) { accb(); return; }
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"code\":403,\"msg\":\"缺少访问凭证 (X-Challenge-Token 或 X-App-Id)\"}");
                acb(resp); return;
            }

            if (!hasSignHeader || !sv.hasApps()) { accb(); return; }
            std::string errMsg;
            if (!sv.verify(req, errMsg)) {
                LOG_WARN << "[Sign] 验签失败: " << errMsg << " path=" << path
                         << " ip=" << IpUtils::getIpAddr(req);
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"code\":403,\"msg\":\"" + errMsg + "\"}");
                acb(resp); return;
            }
            accb();
        });

    // ── 定期清理限流器过期记录（每2分钟）────────────────────────────────
    drogon::app().getLoop()->runEvery(120.0, [] {
        RateLimiter::instance().cleanup();
    });
}
