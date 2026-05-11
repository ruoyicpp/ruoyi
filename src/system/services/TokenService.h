#pragma once
#include <string>
#include <chrono>
#include <fstream>
#include <drogon/drogon.h>
#include "../../common/LoginUser.h"
#include "../../common/TokenCache.h"
#include "../../common/JwtUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../common/IpUtils.h"
#include "../../common/UaUtils.h"
#include "../../common/ApiKeyService.h"
#include "../../services/DatabaseService.h"

// 多终端策略：是否在新登录时踢掉同 userId 的其它在线会话
// 通过 config.json 的 security.kick_previous_session 控制（默认 false）
// 0=未初始化，1=true，2=false
struct MultiSessionPolicy {
    static bool kickPrevious() {
        static int cached = [] {
            int v = 2;
            try {
                std::ifstream f("config.json");
                if (!f.is_open()) return v;
                Json::Value root; Json::CharReaderBuilder rb; std::string e;
                if (Json::parseFromStream(rb, f, &root, &e)
                    && root.isMember("security")
                    && root["security"].isMember("kick_previous_session"))
                    v = root["security"]["kick_previous_session"].asBool() ? 1 : 2;
            } catch (...) {}
            return v;
        }();
        return cached == 1;
    }
};

// 对应 RuoYi.Net TokenService
class TokenService {
public:
    static TokenService &instance() {
        static TokenService inst;
        return inst;
    }

    // 创建 token，生成 uuid 后存入缓存，返回 JWT 字符串
    std::string createToken(LoginUser &user, const drogon::HttpRequestPtr &req) {
        // 多终端策略：踢掉同 userId 的其它在线会话（若已开启）
        if (MultiSessionPolicy::kickPrevious() && user.userId > 0) {
            try {
                auto all = TokenCache::instance().getAll();
                for (auto &u : all) {
                    if (u.userId == user.userId && !u.token.empty())
                        delLoginUser(u.token);
                }
            } catch (...) {}
        }

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
        // 写透到 sys_token（含 user_id 字段以便按用户索引查询）
        try {
            Json::FastWriter fw;
            std::string val = fw.write(user.toJson());
            DatabaseService::instance().execParams(
                "INSERT INTO sys_token(token_key,token_value,expire_time,user_id,create_time) "
                "VALUES($1,$2,$3,$4,NOW()) "
                "ON CONFLICT(token_key) DO UPDATE SET token_value=$2,expire_time=$3,user_id=$4",
                {key, val, std::to_string(user.expireTime), std::to_string(user.userId)});
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
    // 鉴权优先级：JWT (Authorization: Bearer ...) > X-API-Key header > ?apiKey= query
    std::optional<LoginUser> getLoginUser(const drogon::HttpRequestPtr &req) {
        auto token = SecurityUtils::getToken(req);
        if (!token.empty()) {
            try {
                auto uuid = JwtUtils::parseUuid(token);
                return TokenCache::instance().get(SecurityUtils::getTokenKey(uuid));
            } catch (...) {}
        }
        // ── f16 API Key 鉴权 fallback ─────────────────────────────────────
        std::string apiKey = req->getHeader("X-API-Key");
        if (apiKey.empty()) apiKey = req->getParameter("apiKey");
        if (!apiKey.empty()) {
            return ApiKeyService::instance().verifyKey(apiKey);
        }
        return std::nullopt;
    }

private:
    static long long nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};
