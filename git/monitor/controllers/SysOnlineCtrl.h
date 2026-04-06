#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../common/TokenCache.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../common/OperLogUtils.h"
#include "../../system/services/TokenService.h"
#include "WsNotifyCtrl.h"

// 在线用户 /monitor/online
class SysOnlineCtrl : public drogon::HttpController<SysOnlineCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysOnlineCtrl::list,       "/monitor/online/list",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysOnlineCtrl::forceLogout,"/monitor/online/{tokenId}",   drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    // 在线用户列表：遍历 TokenCache 枚举所有当前在线用户
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

    // 强制退出（踢人）
    void forceLogout(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &tokenId) {
        CHECK_PERM(req, cb, "monitor:online:forceLogout");
        auto key    = SecurityUtils::getTokenKey(tokenId);
        // 先取 userId，用于 WS 推送（取完再移除）
        auto cached = TokenCache::instance().get(key);
        long userId = cached ? cached->userId : -1;
        // 移除缓存（JWT 失效）并同步删除持久化记录
        TokenService::instance().delLoginUser(tokenId);
        // 推送 WS 踢人通知（若用户已建立 WS 连接）
        if (userId > 0) WsNotifyCtrl::sendKick(userId);
        LOG_OPER_PARAM(req, "在线用户", BusinessType::FORCE,
                       "强制退出: " + (cached ? cached->userName : tokenId));
        RESP_MSG(cb, "操作成功");
    }
};
