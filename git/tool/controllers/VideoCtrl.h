#pragma once
#include <drogon/HttpController.h>
#include <drogon/HttpClient.h>
#include <sstream>
#include <algorithm>
#include "../../common/AjaxResult.h"
#include "../../services/DatabaseService.h"

// GET /api/video/random
// 从 sys_config[sys.video.api] 配置的外部 API 获取随机视频 URL
class VideoCtrl : public drogon::HttpController<VideoCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(VideoCtrl::random, "/api/video/random", drogon::Get);
    METHOD_LIST_END

    void random(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& db = DatabaseService::instance();

        // 检查 sys.video.enabled（不存在则默认 true）
        auto cfgRes = db.queryParams(
            "SELECT config_value FROM sys_config WHERE config_key='sys.video.enabled' LIMIT 1",
            {});
        if (cfgRes.ok() && cfgRes.rows() > 0) {
            std::string val = cfgRes.str(0, 0);
            if (val == "false" || val == "0") {
                Json::Value r; r["ok"] = false; r["msg"] = "视频功能已关闭";
                cb(drogon::HttpResponse::newHttpJsonResponse(r));
                return;
            }
        }

        // 读取 API 地址
        auto apiRes = db.queryParams(
            "SELECT config_value FROM sys_config WHERE config_key='sys.video.api' LIMIT 1",
            {});
        std::string apiUrl = (apiRes.ok() && apiRes.rows() > 0 && !apiRes.str(0,0).empty())
            ? apiRes.str(0, 0)
            : "https://api.pearktrue.cn/api/video/";

        // 解析 scheme + host + path
        bool useHttps = true;
        std::string rawUrl = apiUrl;
        if (rawUrl.substr(0, 8) == "https://")      { useHttps = true;  rawUrl = rawUrl.substr(8); }
        else if (rawUrl.substr(0, 7) == "http://")   { useHttps = false; rawUrl = rawUrl.substr(7); }

        std::string host, path, query;
        auto slashPos = rawUrl.find('/');
        if (slashPos != std::string::npos) {
            host = rawUrl.substr(0, slashPos);
            path = rawUrl.substr(slashPos);
        } else {
            host = rawUrl; path = "/";
        }
        // 分离 query string（外部 API 可能带参数）
        auto qPos = path.find('?');
        if (qPos != std::string::npos) {
            query = path.substr(qPos + 1);
            path  = path.substr(0, qPos);
        }

        std::string scheme = useHttps ? "https://" : "http://";
        auto client = drogon::HttpClient::newHttpClient(scheme + host);
        auto apiReq = drogon::HttpRequest::newHttpRequest();
        apiReq->setPath(path);
        apiReq->setMethod(drogon::Get);
        if (!query.empty()) {
            // 将 query string 各参数设置到请求
            std::istringstream qs(query);
            std::string token;
            while (std::getline(qs, token, '&')) {
                auto eq = token.find('=');
                if (eq != std::string::npos)
                    apiReq->setParameter(token.substr(0, eq), token.substr(eq + 1));
            }
        }

        client->sendRequest(apiReq,
            [cb = std::move(cb), client](drogon::ReqResult result,
                                         const drogon::HttpResponsePtr& resp) {
                Json::Value r;
                if (result != drogon::ReqResult::Ok || !resp) {
                    r["ok"] = false; r["msg"] = "获取视频失败，请稍后重试";
                    cb(drogon::HttpResponse::newHttpJsonResponse(r));
                    return;
                }
                std::string body(resp->body());
                // 去除首尾空白
                auto trim = [](std::string s) {
                    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                        [](unsigned char c){ return !std::isspace(c); }));
                    s.erase(std::find_if(s.rbegin(), s.rend(),
                        [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
                    return s;
                };
                body = trim(body);

                // 尝试解析 JSON（兼容多种外部 API 响应格式）
                Json::Value root;
                Json::CharReaderBuilder rb;
                std::string errs;
                std::istringstream iss(body);
                std::string url;
                if (Json::parseFromStream(rb, iss, &root, &errs)) {
                    // 按优先级依次尝试字段名
                    for (const auto& key : {"url","src","data","link","video","mp4","path"}) {
                        if (root.isMember(key)) {
                            auto v = root[key];
                            if (v.isString() && !v.asString().empty()) {
                                url = v.asString(); break;
                            }
                        }
                    }
                    // 嵌套 data 对象
                    if (url.empty() && root.isMember("data") && root["data"].isObject()) {
                        for (const auto& key : {"url","src","link","mp4"}) {
                            if (root["data"].isMember(key) && root["data"][key].isString())
                                { url = root["data"][key].asString(); break; }
                        }
                    }
                } else if (body.size() > 4 && body.substr(0, 4) == "http") {
                    // 纯文本 URL 响应
                    url = body;
                }

                if (!url.empty()) {
                    r["ok"]  = true;
                    r["url"] = url;
                } else {
                    r["ok"]  = false;
                    r["msg"] = "无法解析视频地址";
                }
                cb(drogon::HttpResponse::newHttpJsonResponse(r));
            }, 10.0);
    }
};
