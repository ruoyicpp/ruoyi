#pragma once
#include <string>
#include <chrono>
#include <drogon/drogon.h>
#include "../../common/LoginUser.h"
#include "../../common/TokenCache.h"
#include "../../common/JwtUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../common/IpUtils.h"

// ��Ӧ RuoYi.Net TokenService
class TokenService {
public:
    static TokenService &instance() {
        static TokenService inst;
        return inst;
    }

    // ���� token�����뻺�棬���� JWT �ַ���
    std::string createToken(LoginUser &user, const drogon::HttpRequestPtr &req) {
        auto uuid = drogon::utils::getUuid();
        user.token = uuid;

        // ���� UA ��Ϣ
        user.ipAddr = IpUtils::getIpAddr(req);
        user.browser = req->getHeader("User-Agent").substr(0, 50);
        user.os = "Unknown";

        refreshToken(user);

        return JwtUtils::createToken(uuid, user.userId, user.userName, user.deptId);
    }

    // ˢ�� token ����ʱ��
    void refreshToken(LoginUser &user) {
        auto &cfg = JwtUtils::config();
        auto now = nowMs();
        user.loginTime  = now;
        user.expireTime = now + (long long)cfg.expireMinutes * 60 * 1000;
        auto key = SecurityUtils::getTokenKey(user.token);
        TokenCache::instance().set(key, user, cfg.expireMinutes);
    }

    // ɾ�� token ���棨�˳���¼��
    void delLoginUser(const std::string &uuid) {
        if (uuid.empty()) return;
        TokenCache::instance().remove(SecurityUtils::getTokenKey(uuid));
    }

    // ���»����е��û���Ϣ���޸�Ȩ�޺���ã�
    void setLoginUser(const LoginUser &user) {
        if (user.token.empty()) return;
        TokenCache::instance().update(SecurityUtils::getTokenKey(user.token), user);
    }

    // �������л�ȡ LoginUser
    std::optional<LoginUser> getLoginUser(const drogon::HttpRequestPtr &req) {
        auto token = SecurityUtils::getToken(req);
        if (token.empty()) return std::nullopt;
        try {
            auto uuid = JwtUtils::parseUuid(token);
            return TokenCache::instance().get(SecurityUtils::getTokenKey(uuid));
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    static long long nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};
