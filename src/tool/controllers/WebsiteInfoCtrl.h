#pragma once
#include <drogon/HttpController.h>
#include <drogon/HttpClient.h>
#include <json/json.h>
#include <sstream>
#include "../../common/AjaxResult.h"
#include "../../system/services/SysConfigService.h"

class WebsiteInfoCtrl : public drogon::HttpController<WebsiteInfoCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(WebsiteInfoCtrl::query, "/tool/websiteInfo", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    // GET /tool/websiteInfo?url=https://www.bilibili.com/
    void query(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        std::string targetUrl = req->getParameter("url");
        if (targetUrl.empty()) {
            RESP_ERR(cb, "url 参数不能为空");
            return;
        }

        // API 地址从 sys_config(sys.websiteinfo.api) 读，fallback 到内置地址
        std::string apiBase = SysConfigService::instance().selectConfigByKey("sys.websiteinfo.api");
        if (apiBase.empty()) apiBase = "https://api.pearktrue.cn";
        // 分离 host 和 path（如 https://host/path/to/api）
        std::string apiSchemeHost = apiBase, apiPath = "/api/website/info/";
        for (auto prefix : {"https://", "http://"}) {
            if (apiBase.rfind(prefix, 0) == 0) {
                auto rest = apiBase.substr(strlen(prefix));
                auto slash = rest.find('/');
                if (slash != std::string::npos) {
                    apiSchemeHost = std::string(prefix) + rest.substr(0, slash);
                    apiPath = rest.substr(slash);
                    if (apiPath.back() != '/') apiPath += '/';
                } else {
                    apiSchemeHost = apiBase;
                }
                break;
            }
        }
        auto client = drogon::HttpClient::newHttpClient(apiSchemeHost);
        auto apiReq = drogon::HttpRequest::newHttpRequest();
        apiReq->setPath(apiPath);
        apiReq->setMethod(drogon::Get);
        apiReq->setParameter("url", targetUrl);

        client->sendRequest(apiReq,
            [cb = std::move(cb), client](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
                if (result != drogon::ReqResult::Ok || !resp) {
                    RESP_ERR(cb, "外部 API 请求失败，请检查网络");
                    return;
                }
                // 直接透传外部 API 的 JSON 响应
                std::string body(resp->body());
                Json::Value root;
                Json::CharReaderBuilder rb;
                std::string errs;
                std::istringstream iss(body);
                if (!Json::parseFromStream(rb, iss, &root, &errs)) {
                    RESP_ERR(cb, "解析外部 API 响应失败");
                    return;
                }
                // 外部 API 返回非 200 时透传错误
                int extCode = root.get("code", 200).asInt();
                if (extCode != 200) {
                    std::string extMsg = root.get("msg", "外部 API 返回错误").asString();
                    RESP_ERR(cb, extMsg);
                    return;
                }
                // 统一包装为 RuoYi 标准格式
                Json::Value out;
                out["code"] = 200;
                out["msg"]  = "查询成功";
                out["data"] = root;
                auto r = drogon::HttpResponse::newHttpJsonResponse(out);
                cb(r);
            }, 8.0);
    }
};
