/**
 * @file CertManagerRoutes.h
 * @brief SSL/TLS 证书管理路由
 * 
 * 功能概述：
 *   - 证书管理 Web UI：/certmanager 路由，提供证书管理界面
 *   - 证书管理 API：/certmanager/api/* 路由，提供证书管理 REST API
 *   - 旧版兼容 API：/api/ssl/* 路由，支持旧版前端调用
 *   - ACME 集成：支持 Let's Encrypt 自动化证书申请和续期
 *   - DNS 验证：支持多种 DNS 提供商进行域名验证
 * 
 * 路由列表：
 *   - GET /certmanager - 证书管理 Web UI
 *   - GET /certmanager/api/info - 获取证书管理器版本信息
 *   - GET /certmanager/api/certificates - 列出所有证书
 *   - GET /certmanager/api/accounts - 列出所有 ACME 账户
 *   - POST /certmanager/api/accounts - 注册新 ACME 账户
 *   - GET /certmanager/api/dns-providers - 列出支持的 DNS 提供商
 *   - POST /certmanager/api/certificates/obtain - 申请新证书
 *   - POST /certmanager/api/certificates/renew - 续期证书
 *   - POST /certmanager/api/certificates/revoke - 撤销证书
 *   - GET /certmanager/api/certificates/{id}/download/{type} - 下载证书文件
 * 
 * 认证方式：
 *   - 令牌认证：支持 JWT 令牌（Header、Cookie、Query 参数）
 *   - localhost 白名单：开发环境可直接访问
 *   - 管理员认证：/api/ssl/* 接口要求管理员权限
 * 
 * @see CertManagerAcme - ACME 证书管理器
 * @see CertManagerDriver - 证书管理驱动
 */

#pragma once
#include "AppIncludes.h"
#include "services/CertManagerDriver.h"

/**
 * @brief 注册 SSL/TLS 证书管理路由
 * 
 * 在应用启动时调用此函数，注册证书管理相关的所有路由。
 * 包括 Web UI、REST API 和旧版兼容接口。
 * 
 * @note 此函数应在 main() 中调用
 */
inline void registerCertManagerRoutes() {
    // ── 公共鉴权 lambda ──────────────────────────────────────────────
    /**
     * @brief 证书管理器公共认证函数
     * 
     * 支持多种令牌来源和 localhost 白名单。
     * 用于 /certmanager 和 /certmanager/api/* 接口。
     * 
     * 认证流程：
     *   1. 从 Authorization Header 提取令牌
     *   2. 如果不存在，从 Cookie 中的 Admin-Token 提取
     *   3. 如果不存在，从 Query 参数中的 token 提取
     *   4. 验证 JWT 令牌有效性
     *   5. 如果令牌无效，检查 localhost 白名单
     * 
     * @param req HTTP 请求对象
     * @return true 如果认证通过，false 否则
     */
    auto cmAuth = [](const drogon::HttpRequestPtr& req) -> bool {
        auto token = SecurityUtils::getToken(req);
        if (token.empty()) {
            const std::string& ck = req->getHeader("cookie");
            const std::string key = "Admin-Token=";
            auto pos = ck.find(key);
            if (pos != std::string::npos) {
                pos += key.size(); auto end = ck.find(';', pos);
                token = ck.substr(pos, end == std::string::npos ? end : end - pos);
            }
        }
        if (token.empty()) token = req->getParameter("token");
        if (!token.empty()) {
            try {
                auto uuid    = JwtUtils::parseUuid(token);
                auto userKey = SecurityUtils::getTokenKey(uuid);
                if (TokenCache::instance().get(userKey)) return true;
            } catch (...) {}
        }
        // localhost 白名单
        const auto& peer = req->getPeerAddr().toIp();
        return (peer == "127.0.0.1" || peer == "::1" || peer == "0.0.0.0");
    };
    
    /**
     * @brief 生成 401 未授权响应（JSON 格式）
     */
    auto cm401 = []() {
        Json::Value e; e["error"] = "Unauthorized";
        return drogon::HttpResponse::newHttpJsonResponse(e);
    };

    // ── 管理员级鉴权（/api/ssl/* 接口要求）────────────────────────────
    /**
     * @brief SSL 管理员认证函数
     * 
     * 用于 /api/ssl/* 接口，要求用户是管理员。
     * 比 cmAuth 更严格，不允许 localhost 白名单。
     * 
     * @param req HTTP 请求对象
     * @return true 如果用户是管理员，false 否则
     */
    auto sslAuth = [](const drogon::HttpRequestPtr& req) -> bool {
        auto user = TokenService::instance().getLoginUser(req);
        return user.has_value() && SecurityUtils::isAdmin(user->userId);
    };
    
    /**
     * @brief 生成 401 未授权响应（RuoYi 格式）
     */
    auto ssl401 = []() {
        Json::Value e; e["code"] = 401; e["msg"] = "未授权";
        return drogon::HttpResponse::newHttpJsonResponse(e);
    };

    // ── certmanager Web UI ──────────────────────────────────────────────
    /**
     * @brief 证书管理器 Web UI 路由
     * 
     * GET /certmanager - 返回证书管理器 Web 界面
     * 
     * 流程：
     *   1. 验证用户认证（令牌或 localhost）
     *   2. 如果认证失败，返回 401 错误页面
     *   3. 加载 certmanager-web/index.html 文件
     *   4. 如果文件不存在，返回 404 错误
     *   5. 返回 HTML 页面
     */
    drogon::app().registerHandler("/certmanager",
        [cmAuth](const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) {
                auto r = drogon::HttpResponse::newHttpResponse();
                r->setStatusCode(drogon::k401Unauthorized);
                r->setContentTypeCode(drogon::CT_TEXT_HTML);
                r->setBody("<html><body style='font-family:sans-serif;text-align:center;padding:60px'>"
                           "<h2>&#128274; 请先登录后携带 token 访问</h2>"
                           "<p>示例：/certmanager?token=eyJhbG...</p></body></html>");
                cb(r); return;
            }
            std::ifstream f("certmanager-web/index.html", std::ios::binary);
            if (!f.is_open()) {
                auto r = drogon::HttpResponse::newHttpResponse();
                r->setStatusCode(drogon::k404NotFound);
                r->setBody("certmanager-web/index.html not found in working directory");
                cb(r); return;
            }
            std::string html((std::istreambuf_iterator<char>(f)), {});
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(html);
            cb(resp);
        }, {drogon::Get});

    // ── certmanager Web UI API (/certmanager/api/*) ─────────────────
    drogon::app().registerHandler("/certmanager/api/info",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            Json::Value j; j["version"] = CertManagerAcme::instance().version();
            cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }, {drogon::Get});

    drogon::app().registerHandler("/certmanager/api/certificates",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().listCertsJson()));
        }, {drogon::Get});

    drogon::app().registerHandler("/certmanager/api/accounts",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().listAccountsJson()));
        }, {drogon::Get});

    drogon::app().registerHandler("/certmanager/api/accounts",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            auto body = req->getJsonObject();
            std::string email   = body ? (*body).get("email",   "").asString() : "";
            std::string keyType = body ? (*body).get("keyType", "EC256").asString() : "EC256";
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().registerAccountJson(email, keyType)));
        }, {drogon::Post});

    drogon::app().registerHandler("/certmanager/api/dns-providers",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().listDNSProvidersJson()));
        }, {drogon::Get});

    drogon::app().registerHandler("/certmanager/api/credentials",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            cb(drogon::HttpResponse::newHttpJsonResponse(Json::Value(Json::arrayValue)));
        }, {drogon::Get});

    drogon::app().registerHandler("/certmanager/api/certificates/obtain",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            auto body = req->getJsonObject();
            if (!body) { Json::Value e; e["error"] = "need JSON body";
                cb(drogon::HttpResponse::newHttpJsonResponse(e)); return; }
            std::string email    = (*body).get("email",       "").asString();
            std::string keyType  = (*body).get("keyType",     "EC256").asString();
            std::string provider = (*body).get("dnsProvider", "").asString();
            int bundle = (*body).get("bundle", 1).asInt();
            std::string domainList;
            for (auto& d : (*body)["domains"])
                domainList += (domainList.empty() ? "" : ",") + d.asString();
            std::string envJson = "{}";
            if (body->isMember("envVars")) {
                Json::StreamWriterBuilder wb; wb["indentation"] = "";
                envJson = Json::writeString(wb, (*body)["envVars"]);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().obtainCertJson(
                    email, domainList, provider, envJson, keyType, bundle)));
        }, {drogon::Post});

    drogon::app().registerHandler("/certmanager/api/certificates/renew",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            auto body = req->getJsonObject();
            std::string certId = body ? (*body).get("certId", "").asString() : "";
            int bundle = body ? (*body).get("bundle", 1).asInt() : 1;
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().renewCertJson(certId, bundle)));
        }, {drogon::Post});

    drogon::app().registerHandler("/certmanager/api/certificates/revoke",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            auto body = req->getJsonObject();
            std::string certId = body ? (*body).get("certId", "").asString() : "";
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().revokeCertJson(certId)));
        }, {drogon::Post});

    drogon::app().registerHandler("/certmanager/api/certificates/{id}/download/{type}",
        [cmAuth, cm401](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                        const std::string& certId, const std::string& fileType) {
            if (!cmAuth(req)) { cb(cm401()); return; }
            std::string content = CertManagerAcme::instance().readCertFileContent(certId, fileType);
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody(content);
            resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
            resp->addHeader("Content-Disposition",
                "attachment; filename=\"" + certId + "." + fileType + ".pem\"");
            cb(resp);
        }, {drogon::Get});

    // ── certmanager REST API（/api/ssl/*，供旧版前端调用）──────────────
    drogon::app().registerHandler("/api/ssl/version",
        [sslAuth, ssl401](const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!sslAuth(req)) { cb(ssl401()); return; }
            Json::Value j; j["version"] = CertManagerAcme::instance().version();
            cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }, {drogon::Get});

    drogon::app().registerHandler("/api/ssl/providers",
        [sslAuth, ssl401](const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!sslAuth(req)) { cb(ssl401()); return; }
            Json::Value j; j["data"] = CertManagerAcme::instance().listDNSProvidersJson();
            cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }, {drogon::Get});

    drogon::app().registerHandler("/api/ssl/certs",
        [sslAuth, ssl401](const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!sslAuth(req)) { cb(ssl401()); return; }
            Json::Value j; j["data"] = CertManagerAcme::instance().listCertsJson();
            cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }, {drogon::Get});

    drogon::app().registerHandler("/api/ssl/cert/{id}",
        [sslAuth, ssl401](const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                          const std::string& id) {
            if (!sslAuth(req)) { cb(ssl401()); return; }
            Json::Value j; j["data"] = CertManagerAcme::instance().getCertInfoJson(id);
            cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }, {drogon::Get});

    drogon::app().registerHandler("/api/ssl/cert/{id}",
        [sslAuth, ssl401](const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                          const std::string& id) {
            if (!sslAuth(req)) { cb(ssl401()); return; }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().revokeCertJson(id)));
        }, {drogon::Delete});

    drogon::app().registerHandler("/api/ssl/renew/{id}",
        [sslAuth, ssl401](const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                          const std::string& id) {
            if (!sslAuth(req)) { cb(ssl401()); return; }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().renewCertJson(id, 0)));
        }, {drogon::Post});

    drogon::app().registerHandler("/api/ssl/obtain",
        [sslAuth, ssl401](const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!sslAuth(req)) { cb(ssl401()); return; }
            auto body = req->getJsonObject();
            if (!body) {
                Json::Value e; e["code"] = 400; e["msg"] = "需要 JSON body";
                cb(drogon::HttpResponse::newHttpJsonResponse(e)); return;
            }
            std::string email    = (*body).get("email",    "").asString();
            std::string provider = (*body).get("provider", "").asString();
            std::string keyType  = (*body).get("key_type", "EC256").asString();
            std::string domainList;
            for (auto& d : (*body)["domains"])
                domainList += (domainList.empty() ? "" : ",") + d.asString();
            std::string envJson = "{}";
            if (body->isMember("env_vars")) {
                Json::StreamWriterBuilder wb; wb["indentation"] = "";
                envJson = Json::writeString(wb, (*body)["env_vars"]);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().obtainCertJson(
                    email, domainList, provider, envJson, keyType, 0)));
        }, {drogon::Post});
}
