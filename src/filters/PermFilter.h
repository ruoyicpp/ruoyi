/**
 * @file PermFilter.h
 * @brief 权限检查工具 — 基于权限字符串的访问控制
 * 
 * 功能概述：
 *   - 权限检查：检查用户是否拥有指定权限
 *   - 角色检查：检查用户是否拥有指定角色
 *   - 用户信息提取：从请求中提取登录用户信息
 *   - 管理员豁免：管理员（userId=1）拥有所有权限
 *   - 通配符支持：支持 *:*:* 通配符权限
 * 
 * 权限字符串格式：
 *   - system:user:list   - 查看用户列表
 *   - system:user:add    - 新增用户
 *   - system:user:edit   - 修改用户
 *   - system:user:remove - 删除用户
 *   - *:*:*              - 拥有所有权限
 * 
 * 使用示例：
 *   // 在 Controller 中检查权限
 *   void list(const drogon::HttpRequestPtr &req, ...) {
 *       CHECK_PERM(req, cb, "system:user:list");
 *       // 权限检查通过，继续处理
 *   }
 *   
 *   // 获取登录用户信息
 *   auto user = GET_LOGIN_USER(req);
 *   long userId = GET_USER_ID(req);
 *   std::string userName = GET_USER_NAME(req);
 * 
 * 权限检查规则：
 *   1. 如果用户 ID 为 1（管理员），直接返回 true
 *   2. 如果用户拥有 *:*:* 权限，直接返回 true
 *   3. 否则检查用户权限列表中是否包含指定权限
 */

#pragma once
#include <drogon/HttpFilter.h>
#include "../common/AjaxResult.h"
#include "../common/LoginUser.h"
#include <string>
#include <algorithm>

/**
 * @class PermissionChecker
 * @brief 权限检查工具类
 * 
 * 提供权限和角色检查的静态方法。
 * 所有方法都是静态的，无需创建实例。
 * 
 * 对应 RuoYi 中的 [AppAuthorize] 特性，提供基于权限字符串的访问控制。
 */
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
