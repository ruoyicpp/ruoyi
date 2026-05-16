#pragma once
#include "AppIncludes.h"
#include "common/FrontendState.h"

inline void registerSecurityMiddleware() {
    // ── IP 限流 (DDoS 防御) ─────────────────────────────────────────────────
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

    // ── Bot UA 拦截（绕过 nginx 直连后端时的第二道防线）─────────────────────
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
