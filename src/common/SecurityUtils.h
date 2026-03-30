#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <optional>
#include "Constants.h"

// ǰ������
struct LoginUser;

// ��Ӧ RuoYi.Net SecurityUtils
class SecurityUtils {
public:
    // ������ͷ��ȡ token �ַ���
    static std::string getToken(const drogon::HttpRequestPtr &req) {
        std::string token = req->getHeader("Authorization");
        if (token.empty()) token = req->getHeader("token");
        if (token.empty()) token = req->getParameter("token");
        if (!token.empty() && token.rfind(Constants::TOKEN_PREFIX, 0) == 0)
            token = token.substr(Constants::TOKEN_PREFIX.size());
        return token;
    }

    // �Ƿ����Ա userId
    static bool isAdmin(long userId) { return userId == Constants::ADMIN_USER_ID; }
    // �Ƿ����Ա��ɫ
    static bool isAdminRole(long roleId) { return roleId == Constants::ADMIN_ROLE_ID; }

    // BCrypt ����
    static std::string encryptPassword(const std::string &password);
    // BCrypt ��֤
    static bool matchesPassword(const std::string &raw, const std::string &encoded);

    // ���� key
    static std::string getTokenKey(const std::string &uuid) {
        return Constants::LOGIN_TOKEN_KEY + uuid;
    }
};
