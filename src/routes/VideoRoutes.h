#pragma once
#include "AppIncludes.h"

inline void registerVideoRoutes() {
    // ── 随机视频开关（无需登录，前端用于决定是否显示菜单）──────────────
    // GET /api/video/enabled → {"enabled":true}
    drogon::app().registerHandler("/api/video/enabled",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto& db = DatabaseService::instance();
            auto res = db.queryParams(
                "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1",
                {"sys.video.enabled"});
            bool enabled = true;
            if (res.ok() && res.rows() > 0) {
                std::string val = res.str(0, 0);
                enabled = !(val == "false" || val == "0");
            }
            Json::Value j;
            j["enabled"] = enabled;
            auto r = drogon::HttpResponse::newHttpJsonResponse(j);
            r->addHeader("Access-Control-Allow-Origin", "*");
            cb(r);
        }, {drogon::Get});

    // ── 随机视频接口 ───────────────────────────────────────────────────
    // GET /api/video/random  → {"url":"https://...mp4"}
    drogon::app().registerHandler("/api/video/random",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto client = drogon::HttpClient::newHttpClient("http://api.yujn.cn");
            auto extReq = drogon::HttpRequest::newHttpRequest();
            extReq->setPath("/api/zzxjj.php");
            extReq->setParameter("type", "video");
            extReq->setMethod(drogon::Get);
            client->sendRequest(extReq,
                [cb](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
                    Json::Value j;
                    if (result == drogon::ReqResult::Ok) {
                        std::string url = resp->getHeader("location");
                        if (url.empty()) url = std::string(resp->body());
                        j["url"] = url;
                        j["ok"]  = true;
                    } else {
                        j["ok"]  = false;
                        j["url"] = "";
                    }
                    auto r = drogon::HttpResponse::newHttpJsonResponse(j);
                    r->addHeader("Access-Control-Allow-Origin", "*");
                    cb(r);
                });
        }, {drogon::Get});

    // GET /api/video/player  → 读取 web/video-player.html
    drogon::app().registerHandler("/api/video/player",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            static std::string html;
            { std::ifstream _htmlf("web/video-player.html", std::ios::binary);
              if (_htmlf) html = std::string(std::istreambuf_iterator<char>(_htmlf), {}); }
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(html);
            cb(resp);
        }, {drogon::Get});
}
