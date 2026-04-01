#pragma once
#include <string>
#include <chrono>
#include <drogon/drogon.h>
#include "../../common/LoginUser.h"
#include "../../common/TokenCache.h"
#include "../../common/JwtUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../common/IpUtils.h"
#include "../../common/UaUtils.h"
#include "../../services/DatabaseService.h"

// 对应 RuoYi.Net TokenService
class TokenService {
public:
    static TokenService &instance() {
        static TokenService inst;
        return inst;
    }

    // 创建 token，生成 uuid 后存入缓存，返回 JWT 字符串
    std::string createToken(LoginUser &user, const drogon::HttpRequestPtr &req) {
        auto uuid = drogon::utils::getUuid();
        user.token = uuid;

        // 解析 UA 信息
        user.ipAddr = IpUtils::getIpAddr(req);
        auto ua = UaUtils::parse(req->getHeader("User-Agent"));
        user.browser = ua.browser;
        user.os      = ua.os;
        user.loginLocation = IpUtils::getIpLocation(user.ipAddr); // 内网IP/XX XX 兜底

        refreshToken(user);

        // 外网 IP：异步获取精确地址后更新缓存和 sys_token
        if (!IpUtils::isIntranetIp(user.ipAddr)) {
            std::string tokenKey = SecurityUtils::getTokenKey(uuid);
            std::string ipCopy   = user.ipAddr;
            IpUtils::getIpLocationAsync(ipCopy, [tokenKey, ipCopy](std::string loc) {
                auto cached = TokenCache::instance().get(tokenKey);
                if (cached) {
                    cached->loginLocation = loc;
                    TokenCache::instance().update(tokenKey, *cached);
                    try {
                        Json::FastWriter fw;
                        std::string val = fw.write(cached->toJson());
                        DatabaseService::instance().execParams(
                            "UPDATE sys_token SET token_value=$1 WHERE token_key=$2",
                            {val, tokenKey});
                    } catch (...) {}
                }
            });
        }

        return JwtUtils::createToken(uuid, user.userId, user.userName, user.deptId);
    }

    // 刷新 token 过期时间，同时写透 sys_token（SQLite 持久化）
    void refreshToken(LoginUser &user) {
        auto &cfg = JwtUtils::config();
        auto now = nowMs();
        user.loginTime  = now;
        user.expireTime = now + (long long)cfg.expireMinutes * 60 * 1000;
        auto key = SecurityUtils::getTokenKey(user.token);
        TokenCache::instance().set(key, user, cfg.expireMinutes);
        // 写透到 sys_token（仅 SQLite，用于重启恢复）
        try {
            Json::FastWriter fw;
            std::string val = fw.write(user.toJson());
            DatabaseService::instance().execParams(
                "INSERT INTO sys_token(token_key,token_value,expire_time,create_time) "
                "VALUES($1,$2,$3,NOW()) "
                "ON CONFLICT(token_key) DO UPDATE SET token_value=$2,expire_time=$3",
                {key, val, std::to_string(user.expireTime)});
        } catch (...) {}
    }

    // 删除 token（登出），同时清理 sys_token
    void delLoginUser(const std::string &uuid) {
        if (uuid.empty()) return;
        auto key = SecurityUtils::getTokenKey(uuid);
        TokenCache::instance().remove(key);
        try {
            DatabaseService::instance().execParams(
                "DELETE FROM sys_token WHERE token_key=$1", {key});
        } catch (...) {}
    }

    // 删除 token 对应缓存（登出）
    void setLoginUser(const LoginUser &user) {
        if (user.token.empty()) return;
        TokenCache::instance().update(SecurityUtils::getTokenKey(user.token), user);
    }

    // 更新缓存中的登录用户
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
