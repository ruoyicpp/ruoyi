#pragma once
#include <drogon/HttpFilter.h>
#include "../common/AjaxResult.h"
#include "../common/LoginUser.h"
#include <string>
#include <algorithm>

// 权限检查辅助类：检查指定权限字符串
// 用法：在路由注册时指定 PermFilter<"system:user:list">
// 因为 C++ 模板不支持字符串字面量继承方式，每种权限对应一个类
// 实际使用宏在 Controller 内手动校验权限，见下方
class PermissionChecker {
public:
    // 检查当前请求的 LoginUser 是否拥有指定权限
    static bool hasPermission(const drogon::HttpRequestPtr &req, const std::string &perm) {
        try {
            auto &attrs = req->getAttributes();
            auto userOpt = attrs->get<LoginUser>("loginUser");
            // 管理员拥有所有权限
            if (userOpt.userId == 1L) return true;
            // 有 *:*:* 则拥有所有权限
            auto &perms = userOpt.permissions;
            if (std::find(perms.begin(), perms.end(), "*:*:*") != perms.end()) return true;
            return std::find(perms.begin(), perms.end(), perm) != perms.end();
        } catch (...) {
            return false;
        }
    }

    static bool hasRole(const drogon::HttpRequestPtr &req, const std::string &role) {
        try {
            auto &attrs = req->getAttributes();
            auto userOpt = attrs->get<LoginUser>("loginUser");
            if (userOpt.userId == 1L) return true;
            auto &roles = userOpt.roles;
            return std::find(roles.begin(), roles.end(), role) != roles.end();
        } catch (...) {
            return false;
        }
    }

    // 异常安全地获取当前登录用户
    static LoginUser getLoginUser(const drogon::HttpRequestPtr &req) {
        return req->getAttributes()->get<LoginUser>("loginUser");
    }

    static long getUserId(const drogon::HttpRequestPtr &req) {
        try { return req->getAttributes()->get<long>("userId"); }
        catch (...) { return 0; }
    }

    static long getDeptId(const drogon::HttpRequestPtr &req) {
        try { return req->getAttributes()->get<long>("deptId"); }
        catch (...) { return 0; }
    }

    static std::string getUserName(const drogon::HttpRequestPtr &req) {
        try { return req->getAttributes()->get<std::string>("userName"); }
        catch (...) { return ""; }
    }
};

// 权限宏（在 Controller 处理函数头部使用）
#define CHECK_PERM(req, cb, perm) \
    if (!PermissionChecker::hasPermission(req, perm)) { \
        RESP_401(cb); return; \
    }

#define GET_LOGIN_USER(req) PermissionChecker::getLoginUser(req)
#define GET_USER_ID(req)    PermissionChecker::getUserId(req)
#define GET_DEPT_ID(req)    PermissionChecker::getDeptId(req)
#define GET_USER_NAME(req)  PermissionChecker::getUserName(req)
