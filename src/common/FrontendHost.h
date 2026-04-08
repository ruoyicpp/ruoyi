#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <fstream>
#include <sstream>
#include <filesystem>

// Drogon 内置前端托管
// 将 Vue dist 目录直接由后端托管，支持 SPA history 模式回退
// config.json: { "frontend": { "enabled": true, "dist_path": "./web", "api_prefix": "/prod-api", "cache_seconds": 3600 } }
class FrontendHost {
public:
    static FrontendHost &instance() { static FrontendHost f; return f; }

    void init(const Json::Value &cfg) {
        if (!cfg.get("enabled", false).asBool()) return;
        distPath_   = cfg.get("dist_path", "./web").asString();
        apiPrefix_  = cfg.get("api_prefix", "/prod-api").asString();
        cacheSeconds_ = cfg.get("cache_seconds", 3600).asInt();
        enabled_    = true;
        registerRoutes();
        LOG_INFO << "[FrontendHost] serving " << distPath_ << " at /";
    }

    bool enabled() const { return enabled_; }

private:
    std::string distPath_  = "./web";
    std::string apiPrefix_ = "/prod-api";
    int cacheSeconds_      = 3600;
    bool enabled_          = false;

    // MIME 类型
    static std::string mime(const std::string &ext) {
        if (ext == ".html") return "text/html; charset=utf-8";
        if (ext == ".js")   return "application/javascript";
        if (ext == ".css")  return "text/css";
        if (ext == ".png")  return "image/png";
        if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".svg")  return "image/svg+xml";
        if (ext == ".ico")  return "image/x-icon";
        if (ext == ".json") return "application/json";
        if (ext == ".woff2")return "font/woff2";
        if (ext == ".woff") return "font/woff";
        return "application/octet-stream";
    }

    void registerRoutes() {
        namespace fs = std::filesystem;
        std::string dist = distPath_;
        std::string prefix = apiPrefix_;
        int cache = cacheSeconds_;

        drogon::app().registerHandlerViaRegex(
            "/(.*)",
            [dist, prefix, cache](const drogon::HttpRequestPtr &req,
                                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
                std::string path = req->path();

                // API 请求直接跳过（由后端处理）
                if (!path.empty() && path.rfind(prefix, 0) == 0) {
                    auto r = drogon::HttpResponse::newHttpResponse();
                    r->setStatusCode(drogon::k404NotFound);
                    cb(r); return;
                }

                // 去掉 prod-api 前缀（Nginx 代理模式兼容）
                if (path.rfind(prefix, 0) == 0)
                    path = path.substr(prefix.size());
                if (path.empty()) path = "/";

                std::string fullPath = dist + path;

                // SPA 回退：找不到文件就返回 index.html
                auto tryServe = [&](const std::string &fp) -> bool {
                    if (!fs::exists(fp) || fs::is_directory(fp)) return false;
                    std::ifstream f(fp, std::ios::binary);
                    if (!f) return false;
                    std::ostringstream ss; ss << f.rdbuf();
                    std::string body = ss.str();
                    std::string ext = fs::path(fp).extension().string();
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setBody(body);
                    resp->setContentTypeString(mime(ext));
                    if (cache > 0 && ext != ".html")
                        resp->addHeader("Cache-Control", "max-age=" + std::to_string(cache));
                    cb(resp); return true;
                };

                if (!tryServe(fullPath) &&
                    !tryServe(fullPath + "/index.html") &&
                    !tryServe(dist + "/index.html")) {
                    auto r = drogon::HttpResponse::newHttpResponse();
                    r->setStatusCode(drogon::k404NotFound);
                    cb(r);
                }
            },
            {drogon::Get}
        );
    }
};
