#pragma once
#include "AppIncludes.h"

inline void registerSslRoutes() {
    // ── SSL/HTTPS 配置管理页（无需前端，浏览器直接访问）─────────────────
    drogon::app().registerHandler("/ssl-config",
        [](const drogon::HttpRequestPtr& req,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto token = SecurityUtils::getToken(req);
            if (token.empty()) {
                const std::string& cookieHdr = req->getHeader("cookie");
                const std::string key = "Admin-Token=";
                auto pos = cookieHdr.find(key);
                if (pos != std::string::npos) {
                    pos += key.size();
                    auto end = cookieHdr.find(';', pos);
                    token = cookieHdr.substr(pos, end == std::string::npos ? end : end - pos);
                }
            }
            if (token.empty()) token = req->getParameter("token");
            bool ok = false;
            if (!token.empty()) {
                try {
                    auto uuid    = JwtUtils::parseUuid(token);
                    auto userKey = SecurityUtils::getTokenKey(uuid);
                    ok = (bool)TokenCache::instance().get(userKey);
                } catch (...) {}
            }
            if (!ok) {
                const auto& peer = req->getPeerAddr().toIp();
                ok = (peer == "127.0.0.1" || peer == "::1" || peer == "0.0.0.0");
            }
            if (!ok) {
                auto r = drogon::HttpResponse::newHttpResponse();
                r->setStatusCode(drogon::k401Unauthorized);
                r->setContentTypeCode(drogon::CT_TEXT_HTML);
                r->setBody("<html><body style='font-family:sans-serif;text-align:center;padding:60px'>"
                           "<h2>&#128274; 请先登录后携带 token 访问</h2>"
                           "<p>示例：/ssl-config?token=eyJhbG...</p></body></html>");
                cb(r); return;
            }
            std::string html;
            { std::ifstream _htmlf("web/ssl-config.html", std::ios::binary);
              if (_htmlf) html = std::string(std::istreambuf_iterator<char>(_htmlf), {}); }
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(html);
            cb(resp);
        }, {drogon::Get});
}
