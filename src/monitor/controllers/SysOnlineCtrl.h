#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../common/TokenCache.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"

// �����û� /monitor/online
class SysOnlineCtrl : public drogon::HttpController<SysOnlineCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysOnlineCtrl::list,       "/monitor/online/list",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysOnlineCtrl::forceLogout,"/monitor/online/{tokenId}",   drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    // �����û��б���� TokenCache ö�����������û���
    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:online:list");
        auto ipaddr   = req->getParameter("ipaddr");
        auto userName = req->getParameter("userName");
        auto allUsers = TokenCache::instance().getAll();
        Json::Value arr(Json::arrayValue);
        for (auto &u : allUsers) {
            if (!ipaddr.empty() && u.ipAddr.find(ipaddr) == std::string::npos) continue;
            if (!userName.empty() && u.userName.find(userName) == std::string::npos) continue;
            Json::Value j;
            j["tokenId"]       = u.token;
            j["userName"]      = u.userName;
            j["ipaddr"]        = u.ipAddr;
            j["loginLocation"] = u.loginLocation;
            j["browser"]       = u.browser;
            j["os"]            = u.os;
            j["loginTime"]     = (Json::Int64)u.loginTime;
            j["deptName"]      = u.deptName;
            arr.append(j);
        }
        PageResult pr; pr.total = (long)arr.size(); pr.rows = arr;
        RESP_JSON(cb, pr.toJson());
    }

    // ǿ���˳�
    void forceLogout(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &tokenId) {
        CHECK_PERM(req, cb, "monitor:online:forceLogout");
        TokenCache::instance().remove(SecurityUtils::getTokenKey(tokenId));
        RESP_MSG(cb, "�����ɹ�");
    }
};
