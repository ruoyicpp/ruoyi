#pragma once
#include "AppIncludes.h"

inline void registerCorsMiddleware() {
    // ── CORS（从 config.json cors 段读取，无需重新编译）─────────────────────
    struct CorsCfg {
        std::vector<std::string> origins;
        std::string methods;
        std::string headers;
        std::string expose;
        bool credentials = false;
    };
    auto corsCfg = std::make_shared<CorsCfg>();
    std::ifstream ccf("config.json");
    if (ccf.is_open()) {
        Json::Value root; Json::CharReaderBuilder rb; std::string errs;
        if (Json::parseFromStream(rb, ccf, &root, &errs) && root.isMember("cors")) {
            auto& c = root["cors"];
            if (c.isMember("allow_origins"))
                for (auto& o : c["allow_origins"]) corsCfg->origins.push_back(o.asString());
            if (c.isMember("allow_methods")) {
                std::string m;
                for (auto& v : c["allow_methods"]) { if (!m.empty()) m+=','; m+=v.asString(); }
                corsCfg->methods = m;
            }
            if (c.isMember("allow_headers")) {
                std::string h;
                for (auto& v : c["allow_headers"]) { if (!h.empty()) h+=','; h+=v.asString(); }
                corsCfg->headers = h;
            }
            if (c.isMember("expose_headers")) {
                std::string e;
                for (auto& v : c["expose_headers"]) { if (!e.empty()) e+=','; e+=v.asString(); }
                corsCfg->expose = e;
            }
            corsCfg->credentials = c.get("allow_credentials", false).asBool();
        }
    }
    // 若 menu.api_base_url 已配置，自动将其 origin 加入 CORS 白名单
    {
        std::ifstream maf("config.json");
        if (maf.is_open()) {
            Json::Value mr; Json::CharReaderBuilder rb2; std::string err2;
            if (Json::parseFromStream(rb2, maf, &mr, &err2)
                && mr.isMember("menu") && mr["menu"].isMember("api_base_url")) {
                std::string abu = mr["menu"]["api_base_url"].asString();
                if (!abu.empty()) {
                    auto pos = abu.find("://");
                    if (pos != std::string::npos) {
                        auto rest = abu.substr(pos + 3);
                        auto slash = rest.find('/');
                        std::string origin = abu.substr(0, pos + 3)
                            + (slash != std::string::npos ? rest.substr(0, slash) : rest);
                        bool found = false;
                        for (auto& o : corsCfg->origins) if (o == origin) { found = true; break; }
                        if (!found) {
                            corsCfg->origins.push_back(origin);
                            LOG_INFO << "[CORS] auto-added origin from api_base_url: " << origin;
                        }
                    }
                }
            }
        }
    }
    if (corsCfg->origins.empty()) corsCfg->origins.push_back("*");
    if (corsCfg->methods.empty())  corsCfg->methods  = "GET,POST,PUT,DELETE,OPTIONS";
    if (corsCfg->headers.empty())  corsCfg->headers  = "*";
    LOG_INFO << "[CORS] origins=" << corsCfg->origins[0]
             << " credentials=" << corsCfg->credentials;

    auto resolveOrigin = [corsCfg](const std::string& origin) -> std::string {
        if (origin.empty()) return "";
        bool wildcard = (corsCfg->origins.size() == 1 && corsCfg->origins[0] == "*");
        if (wildcard) return "*";
        for (auto& o : corsCfg->origins)
            if (o == origin) return o;
        return "";
    };

    // Preflight (OPTIONS)
    drogon::app().registerPreRoutingAdvice(
        [corsCfg, resolveOrigin](const drogon::HttpRequestPtr &req,
                  drogon::AdviceCallback &&acb,
                  drogon::AdviceChainCallback &&accb) {
            if (req->method() != drogon::Options) { accb(); return; }
            std::string allowOrigin = resolveOrigin(req->getHeader("Origin"));
            if (allowOrigin.empty()) { accb(); return; }
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->addHeader("Access-Control-Allow-Origin",  allowOrigin);
            resp->addHeader("Access-Control-Allow-Methods", corsCfg->methods);
            resp->addHeader("Access-Control-Allow-Headers", corsCfg->headers);
            resp->addHeader("Access-Control-Max-Age",       "86400");
            if (!corsCfg->expose.empty())
                resp->addHeader("Access-Control-Expose-Headers", corsCfg->expose);
            if (corsCfg->credentials && allowOrigin != "*")
                resp->addHeader("Access-Control-Allow-Credentials", "true");
            if (allowOrigin != "*")
                resp->addHeader("Vary", "Origin");
            resp->setStatusCode(drogon::k204NoContent);
            acb(resp);
        });

    // 实际请求追加 CORS 头
    drogon::app().registerPostHandlingAdvice(
        [corsCfg, resolveOrigin](const drogon::HttpRequestPtr &req,
                                 const drogon::HttpResponsePtr &resp) {
            std::string allowOrigin = resolveOrigin(req->getHeader("Origin"));
            if (allowOrigin.empty()) return;
            resp->addHeader("Access-Control-Allow-Origin", allowOrigin);
            if (!corsCfg->expose.empty())
                resp->addHeader("Access-Control-Expose-Headers", corsCfg->expose);
            if (corsCfg->credentials && allowOrigin != "*")
                resp->addHeader("Access-Control-Allow-Credentials", "true");
            if (allowOrigin != "*")
                resp->addHeader("Vary", "Origin");
        });
}
