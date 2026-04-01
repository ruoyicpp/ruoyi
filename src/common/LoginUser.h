#pragma once
#include <string>
#include <vector>
#include <json/json.h>

// ��Ӧ RuoYi.Net LoginUser
struct LoginUser {
    long        userId      = 0;
    long        deptId      = 0;
    std::string userName;
    std::string password;   // �����л�����Ӧ
    std::string token;      // uuid������ cache key
    long        loginTime   = 0;
    long        expireTime  = 0;
    std::string ipAddr;
    std::string loginLocation;
    std::string browser;
    std::string os;
    std::string deptName;
    std::vector<std::string> permissions;  // Ȩ���ַ����б���� "system:user:list"
    std::vector<std::string> roles;        // ��ɫ key �б���� "admin"

    // ���л�Ϊ JSON�������ڴ滺�棩
    Json::Value toJson() const {
        Json::Value j;
        j["userId"]        = (Json::Int64)userId;
        j["deptId"]        = (Json::Int64)deptId;
        j["userName"]      = userName;
        j["token"]         = token;
        j["loginTime"]     = (Json::Int64)loginTime;
        j["expireTime"]    = (Json::Int64)expireTime;
        j["ipAddr"]        = ipAddr;
        j["loginLocation"] = loginLocation;
        j["browser"]       = browser;
        j["os"]            = os;
        j["deptName"]      = deptName;
        for (auto &p : permissions) j["permissions"].append(p);
        for (auto &r : roles)       j["roles"].append(r);
        return j;
    }

    static LoginUser fromJson(const Json::Value &j) {
        LoginUser u;
        u.userId        = j["userId"].asInt64();
        u.deptId        = j["deptId"].asInt64();
        u.userName      = j["userName"].asString();
        u.token         = j["token"].asString();
        u.loginTime     = j["loginTime"].asInt64();
        u.expireTime    = j["expireTime"].asInt64();
        u.ipAddr        = j["ipAddr"].asString();
        u.loginLocation = j["loginLocation"].asString();
        u.browser       = j["browser"].asString();
        u.os            = j["os"].asString();
        u.deptName      = j.get("deptName", "").asString();
        for (auto &p : j["permissions"]) u.permissions.push_back(p.asString());
        for (auto &r : j["roles"])       u.roles.push_back(r.asString());
        return u;
    }
};
