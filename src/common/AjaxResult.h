#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>

// PostgreSQL TIMESTAMP �� "yyyy-MM-dd HH:mm:ss"����ֵ���ؿմ�
inline std::string fmtTs(const std::string &ts) {
    if (ts.empty()) return "";
    return ts.size() > 19 ? ts.substr(0, 19) : ts;
}

// ��Ӧ RuoYi.Net AjaxResult��ͳһ��Ӧ��
class AjaxResult {
public:
    static constexpr int CODE_SUCCESS = 200;
    static constexpr int CODE_ERROR   = 500;

    static Json::Value success() {
        Json::Value r;
        r["code"] = CODE_SUCCESS;
        r["msg"]  = "�����ɹ�";
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
        r["msg"]  = "�����ɹ�";
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
    // �������ֶεĳɹ���Ӧ������ login/getInfo �ȣ�
    static Json::Value successMap() {
        Json::Value r;
        r["code"] = CODE_SUCCESS;
        r["msg"]  = "�����ɹ�";
        return r;
    }
};

// ��ݺ꣺���� JSON ��Ӧ
#define RESP_OK(cb, data)    (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::success(data)))
#define RESP_MSG(cb, msg)    (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::success(std::string(msg))))
#define RESP_ERR(cb, msg)    (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::error(std::string(msg))))
#define RESP_401(cb)         (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::error(401,"��Ȩʧ��")))
#define RESP_JSON(cb, json)  (cb)(drogon::HttpResponse::newHttpJsonResponse(json))
