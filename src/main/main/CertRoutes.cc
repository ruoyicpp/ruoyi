/**
 * @file CertRoutes.cc
 * @brief certmanager Web UI + REST API 路由（原 main.cc 后半部分）
 *
 * 包含：
 *   - GET  /certmanager                                  certmanager Web UI（JWT 认证）
 *   - GET  /certmanager/api/info|certificates|accounts|dns-providers|credentials
 *   - POST /certmanager/api/accounts|certificates/obtain|renew|revoke
 *   - GET  /certmanager/api/certificates/{id}/download/{type}
 *   - GET  /api/ssl/version|providers|certs|cert/{id}    certmanager REST API（管理员 JWT）
 *   - POST /api/ssl/renew/{id}|obtain                    申请/续期证书
 *   - DELETE /api/ssl/cert/{id}                          吊销证书
 */

#include "AppBootstrap.h"

namespace boot {

void registerCertRoutes(AppContext& /*ctx*/) {
    // ── certmanager Web UI（原版前端 + DLL API，单端口，无需 19443）────────
    // 访问：GET /certmanager  → 读取 certmanager-web/index.html（JWT 认证）
    // API：/certmanager/api/* → 直接调 DLL 函数

    // UI 页面
    drogon::app().registerHandler("/certmanager",
        [](const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                auto r = drogon::HttpResponse::newHttpResponse();
                r->setStatusCode(drogon::k401Unauthorized);
                r->setContentTypeCode(drogon::CT_TEXT_HTML);
                r->setBody(R"HTML(<!DOCTYPE html><html><head><meta charset="UTF-8"></head><body><script>
(function(){
  var t='';
  try{var u=new URL(window.location.href);t=u.searchParams.get('token')||'';}catch(e){}
  if(!t&&window.parent!==window){try{t=window.parent.sessionStorage.getItem('Admin-Token')||'';}catch(e){}}
  if(!t){try{t=sessionStorage.getItem('Admin-Token')||'';}catch(e){}}
  if(t){var u=new URL(window.location.href);u.searchParams.set('token',t);window.location.replace(u.toString());}
  else{document.body.innerHTML='<div style="text-align:center;padding:60px;font-family:sans-serif"><h2>&#128274; 请先登录后携带 token 访问</h2><p>示例：/certmanager?token=eyJhbG...</p></div>';}
})();
</script></body></html>)HTML");
                cb(r); return;
            }
            // 从配置读取 certmanager web 根目录
            std::string webRoot = "./certmanager-web";
            try {
                auto& cfg = drogon::app().getCustomConfig();
                if (cfg.isMember("acme") && cfg["acme"].isMember("web_root")) {
                    webRoot = cfg["acme"]["web_root"].asString();
                }
            } catch (...) {}

            std::string indexPath = webRoot + "/index.html";
            std::ifstream f(indexPath, std::ios::binary);
            if (!f.is_open()) {
                auto r = drogon::HttpResponse::newHttpResponse();
                r->setStatusCode(drogon::k404NotFound);
                r->setBody("certmanager-web/index.html not found at: " + indexPath);
                cb(r); return;
            }

            // 检查文件大小（防止内存耗尽）
            f.seekg(0, std::ios::end);
            auto fileSize = f.tellg();
            constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10MB
            if (fileSize > MAX_FILE_SIZE) {
                auto r = drogon::HttpResponse::newHttpResponse();
                r->setStatusCode(drogon::k413RequestEntityTooLarge);
                r->setBody("File too large (max 10MB)");
                cb(r); return;
            }
            f.seekg(0, std::ios::beg);

            std::string html((std::istreambuf_iterator<char>(f)), {});
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(html);
            cb(resp);
        }, {drogon::Get});

    // API: GET /certmanager/api/info
    drogon::app().registerHandler("/certmanager/api/info",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
            Json::Value j; j["version"] = CertManagerAcme::instance().version();
            cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }, {drogon::Get});

    // API: GET /certmanager/api/certificates
    drogon::app().registerHandler("/certmanager/api/certificates",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().listCertsJson()));
        }, {drogon::Get});

    // API: GET /certmanager/api/accounts
    drogon::app().registerHandler("/certmanager/api/accounts",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().listAccountsJson()));
        }, {drogon::Get});

    // API: POST /certmanager/api/accounts
    drogon::app().registerHandler("/certmanager/api/accounts",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
            auto body = req->getJsonObject();
            std::string email   = body ? (*body).get("email",   "").asString() : "";
            std::string keyType = body ? (*body).get("keyType", "EC256").asString() : "EC256";
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().registerAccountJson(email, keyType)));
        }, {drogon::Post});

    // API: GET /certmanager/api/dns-providers
    drogon::app().registerHandler("/certmanager/api/dns-providers",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().listDNSProvidersJson()));
        }, {drogon::Get});

    // API: GET /certmanager/api/credentials（DLL无此功能，返回空数组）
    drogon::app().registerHandler("/certmanager/api/credentials",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(Json::Value(Json::arrayValue)));
        }, {drogon::Get});

    // API: POST /certmanager/api/certificates/obtain
    drogon::app().registerHandler("/certmanager/api/certificates/obtain",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
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

    // API: POST /certmanager/api/certificates/renew
    drogon::app().registerHandler("/certmanager/api/certificates/renew",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
            auto body = req->getJsonObject();
            std::string certId = body ? (*body).get("certId", "").asString() : "";
            int bundle = body ? (*body).get("bundle", 1).asInt() : 1;
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().renewCertJson(certId, bundle)));
        }, {drogon::Post});

    // API: POST /certmanager/api/certificates/revoke
    drogon::app().registerHandler("/certmanager/api/certificates/revoke",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
            auto body = req->getJsonObject();
            std::string certId = body ? (*body).get("certId", "").asString() : "";
            cb(drogon::HttpResponse::newHttpJsonResponse(
                CertManagerAcme::instance().revokeCertJson(certId)));
        }, {drogon::Post});

    // API: GET /certmanager/api/certificates/{id}/download/{type}
    drogon::app().registerHandler("/certmanager/api/certificates/{id}/download/{type}",
        [](const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                        const std::string& certId, const std::string& fileType) {
            if (!AuthHelper::verifyAdminToken(req)) {
                cb(AuthHelper::make401JsonResponse());
                return;
            }
            std::string content = CertManagerAcme::instance().readCertFileContent(certId, fileType);
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody(content);
            resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
            resp->addHeader("Content-Disposition",
                "attachment; filename=\"" + certId + "." + fileType + ".pem\"");
            cb(resp);
        }, {drogon::Get});

    // ── certmanager REST API（直接封装 DLL，无需开 web_ui_addr 端口）─────
    // GET  /api/ssl/version         → DLL 版本
    // GET  /api/ssl/providers       → 支持的 DNS 提供商列表
    // GET  /api/ssl/certs           → 已申请证书列表
    // GET  /api/ssl/cert/{id}       → 单个证书详情
    // POST /api/ssl/obtain          → 申请新证书 body:{email,domains,provider,env_vars,key_type}
    // POST /api/ssl/renew/{id}      → 续期证书
    // DELETE /api/ssl/cert/{id}     → 吊销证书
    auto sslAuth = [](const drogon::HttpRequestPtr& req) -> bool {
        // 完整验证 JWT 并要求管理员角色
        auto user = TokenService::instance().getLoginUser(req);
        return user.has_value() && SecurityUtils::isAdmin(user->userId);
    };
    auto ssl401 = []() {
        Json::Value e; e["code"] = 401; e["msg"] = "未授权";
        return drogon::HttpResponse::newHttpJsonResponse(e);
    };

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
            // domains: ["a.com","b.com"] → "a.com,b.com"
            std::string domainList;
            for (auto& d : (*body)["domains"])
                domainList += (domainList.empty() ? "" : ",") + d.asString();
            // env_vars: {key:val} → JSON 字符串
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

} // namespace boot
