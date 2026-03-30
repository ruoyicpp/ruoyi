#pragma once
#include <drogon/HttpFilter.h>
#include "../common/AjaxResult.h"
#include "../common/LoginUser.h"
#include <string>
#include <algorithm>

// Ȩ�޹��������ࣺ����ָ������Ȩ���ַ���
// ��Ӧ RuoYi.Net [AppAuthorize("system:user:list")]
// �÷�����·��ע��ʱָ�� PermFilter<"system:user:list">
// ���� C++ ģ�岻֧���ַ��������ü̳з�ʽ��ÿ��Ȩ�޵�����һ������
// ʵ��ʹ�ぁ� Controller ���ֶ����Ȩ�ޣ�����
class PermissionChecker {
public:
    // ��鵱ǰ����� LoginUser �Ƿ�ӵ��ָ��Ȩ��
    static bool hasPermission(const drogon::HttpRequestPtr &req, const std::string &perm) {
        try {
            auto &attrs = req->getAttributes();
            auto userOpt = attrs->get<LoginUser>("loginUser");
            // ����Աӵ������Ȩ��
            if (userOpt.userId == 1L) return true;
            // ��� *:*:* ����Ȩ��
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

    // ���������Ի�ȡ��ǰ��¼�û�
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

// Ȩ�޼��꣨�� Controller ������ͷʹ�ã�
#define CHECK_PERM(req, cb, perm) \
    if (!PermissionChecker::hasPermission(req, perm)) { \
        RESP_401(cb); return; \
    }

#define GET_LOGIN_USER(req) PermissionChecker::getLoginUser(req)
#define GET_USER_ID(req)    PermissionChecker::getUserId(req)
#define GET_DEPT_ID(req)    PermissionChecker::getDeptId(req)
#define GET_USER_NAME(req)  PermissionChecker::getUserName(req)
