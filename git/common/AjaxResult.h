#pragma once

#include <drogon/drogon.h>

#include <json/json.h>

#include <string>



// PostgreSQL TIMESTAMP 转换为 "yyyy-MM-dd HH:mm:ss" 格式，空值返回空串
inline std::string fmtTs(const std::string &ts) {
    if (ts.empty()) return "";
    return ts.size() > 19 ? ts.substr(0, 19) : ts;
}

// 对应 RuoYi.Net AjaxResult，统一响应体

class AjaxResult {

public:

    static constexpr int CODE_SUCCESS = 200;

    static constexpr int CODE_ERROR   = 500;



    static Json::Value success() {

        Json::Value r;

        r["code"] = CODE_SUCCESS;

        r["msg"]  = "操作成功";

        return r;

    }

    static Json::Value success(const std::string &msg) {

        Json::Value r;

        r["code"] = CODE_SUCCESS;

        r["msg"]  = msg;

        return r;

    }

    static Json::Value success(const Json::Value &data) {

        Json::Value r;

        r["code"] = CODE_SUCCESS;

        r["msg"]  = "操作成功";

        r["data"] = data;

        return r;

    }

    static Json::Value success(const std::string &msg, const Json::Value &data) {

        Json::Value r;

        r["code"] = CODE_SUCCESS;

        r["msg"]  = msg;

        r["data"] = data;

        return r;

    }

    static Json::Value error(const std::string &msg) {

        Json::Value r;

        r["code"] = CODE_ERROR;

        r["msg"]  = msg;

        return r;

    }

    static Json::Value error(int code, const std::string &msg) {

        Json::Value r;

        r["code"] = code;

        r["msg"]  = msg;

        return r;

    }

    // 带额外字段的成功响应（用于 login/getInfo 等）

    static Json::Value successMap() {

        Json::Value r;

        r["code"] = CODE_SUCCESS;

        r["msg"]  = "操作成功";

        return r;

    }

};



// 快捷宏：发送 JSON 响应

#define RESP_OK(cb, data)    (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::success(data)))

#define RESP_MSG(cb, msg)    (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::success(std::string(msg))))

#define RESP_ERR(cb, msg)    (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::error(std::string(msg))))

#define RESP_401(cb)         (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::error(401,"授权失败")))

#define RESP_JSON(cb, json)  (cb)(drogon::HttpResponse::newHttpJsonResponse(json))

