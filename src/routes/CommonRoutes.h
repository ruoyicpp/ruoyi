#pragma once
#include "AppIncludes.h"

inline void registerCommonRoutes() {
    // 静态文件服务：/profile/{dir}/{file} → uploads/{dir}/{file}
    auto serveUpload = [](const drogon::HttpRequestPtr &,
                          std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                          const std::string &dir, const std::string &file) {
        std::string filePath = "uploads/" + dir + "/" + file;
        if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath)) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            cb(resp);
            return;
        }
        cb(drogon::HttpResponse::newFileResponse(filePath));
    };
    drogon::app().registerHandler("/profile/{dir}/{file}", serveUpload, {drogon::Get});

    // ── iconfont 字体文件路由 ──────────────────────────────────────
    drogon::app().registerHandler("/iconfont-sys.woff2",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            static std::string fontData;
            static std::once_flag once;
            std::call_once(once, []() {
                std::ifstream f("iconfont-sys.woff2", std::ios::binary);
                if (f) fontData = std::string(std::istreambuf_iterator<char>(f), {});
            });
            auto resp = drogon::HttpResponse::newHttpResponse();
            if (fontData.empty()) {
                resp->setStatusCode(drogon::k404NotFound);
            } else {
                resp->setContentTypeString("font/woff2");
                resp->addHeader("Cache-Control", "public,max-age=86400");
                resp->setBody(fontData);
            }
            cb(resp);
        }, {drogon::Get});

    // ── /health 健康检查 ──────────────────────────────────────────────
    drogon::app().registerHandler("/health",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto& db = DatabaseService::instance();
            bool dbOk = db.isConnected() || db.isUsingSqlite();
            Json::Value j;
            j["status"] = dbOk ? "UP" : "DEGRADED";
            j["db"]     = db.backendInfo();
            j["cache"]  = MemCache::backendInfo();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(j);
            resp->setStatusCode(dbOk ? drogon::k200OK : drogon::k503ServiceUnavailable);
            cb(resp);
        }, {drogon::Get});

    // ── /version 版本信息 ──────────────────────────────────────────────
    drogon::app().registerHandler("/version",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            Json::Value j;
            j["app"]     = "ruoyi-cpp";
            j["version"] = "1.0.0";
            cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }, {drogon::Get});
}
