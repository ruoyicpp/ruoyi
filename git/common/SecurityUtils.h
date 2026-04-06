#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <optional>
#include "Constants.h"

// 加密工具类
struct LoginUser;

// 对应 RuoYi.Net SecurityUtils
class SecurityUtils {
public:
    // 从 HTTP 请求头中获取 token 字符串
    static std::string getToken(const drogon::HttpRequestPtr &req) {
        std::string token = req->getHeader("Authorization");
        if (token.empty()) token = req->getHeader("token");
        if (token.empty()) token = req->getParameter("token");
        if (!token.empty() && token.rfind(Constants::TOKEN_PREFIX, 0) == 0)
            token = token.substr(Constants::TOKEN_PREFIX.size());
        return token;
    }

    // 获取当前登录 userId
    static bool isAdmin(long userId) { return userId == Constants::ADMIN_USER_ID; }
    // 获取当前登录角色
    static bool isAdminRole(long roleId) { return roleId == Constants::ADMIN_ROLE_ID; }

    // PBKDF2-SHA256 加密密码
    static std::string encryptPassword(const std::string &password);
    // PBKDF2-SHA256 验证
    static bool matchesPassword(const std::string &raw, const std::string &encoded);

    // 生成 token cache key
    static std::string getTokenKey(const std::string &uuid) {
        return Constants::LOGIN_TOKEN_KEY + uuid;
    }
};
