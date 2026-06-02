/**
 * @file TokenService.h
 * @brief JWT 令牌服务 — 处理用户认证、令牌生成、刷新和验证
 * 
 * 功能概述：
 *   - 令牌生成：创建 JWT 令牌，包含用户信息和设备信息
 *   - 令牌刷新：延长令牌有效期
 *   - 令牌验证：从 HTTP 请求中提取和验证令牌
 *   - 会话管理：支持多终端登录、踢掉旧会话等策略
 *   - 地址解析：获取登录 IP 地址和地理位置
 *   - 设备识别：解析浏览器、操作系统等 User-Agent 信息
 * 
 * 核心特性：
 *   - JWT 令牌格式：包含 uuid、userId、userName、deptId
 *   - 双层存储：内存缓存（TokenCache）+ 数据库持久化（sys_token）
 *   - 异步地址解析：外网 IP 地址异步查询，不阻塞登录流程
 *   - 多终端策略：可配置是否在新登录时踢掉同用户的其他会话
 * 
 * 配置项（config.json）：
 *   - jwt.secret: JWT 签名密钥
 *   - jwt.issuer: JWT 签发者
 *   - jwt.audience: JWT 受众
 *   - jwt.expire_minutes: 令牌过期时间（分钟）
 *   - security.kick_previous_session: 新登录时是否踢掉旧会话（默认 false）
 */

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

/**
 * @struct MultiSessionPolicy
 * @brief 多终端会话策略
 * 
 * 控制新用户登录时是否踢掉同 userId 的其他在线会话。
 * 配置通过 config.json 的 security.kick_previous_session 控制（默认 false）。
 * 
 * 缓存策略：
 *   - 0 = 未初始化
 *   - 1 = true（踢掉旧会话）
 *   - 2 = false（允许多终端登录）
 */
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

/**
 * @class TokenService
 * @brief JWT 令牌管理服务单例
 * 
 * 对应 RuoYi.Net 中的 TokenService，提供令牌的生成、验证、刷新等功能。
 * 采用单例模式，全局唯一实例。
 * 
 * 主要职责：
 *   - 生成 JWT 令牌（包含用户信息和设备信息）
 *   - 刷新令牌有效期
 *   - 验证令牌有效性
 *   - 管理在线会话（支持多终端或单终端策略）
 *   - 解析 IP 地址和地理位置
 *   - 识别客户端设备信息
 */
class TokenService {
public:
    static TokenService &instance() {
        static TokenService inst;
        return inst;
    }

    /**
     * @brief 创建新的 JWT 令牌
     * 
     * 为用户生成 JWT 令牌，包含以下步骤：
     *   1. 如果启用了多终端策略，踢掉同用户的其他在线会话
     *   2. 生成唯一的 UUID 作为令牌标识
     *   3. 解析请求中的 IP 地址和 User-Agent 信息
     *   4. 获取 IP 地址的地理位置（内网 IP 直接返回，外网 IP 异步查询）
     *   5. 将用户信息存储到缓存和数据库
     *   6. 返回 JWT 字符串
     * 
     * @param user 登录用户对象（会被修改，添加 token、ipAddr、browser 等字段）
     * @param req HTTP 请求对象（用于提取 IP、User-Agent 等信息）
     * 
     * @return JWT 令牌字符串
     * 
     * @note 
     *   - 外网 IP 的地理位置查询是异步的，不会阻塞返回
     *   - 令牌过期时间由 config.json 的 jwt.expire_minutes 控制
     *   - 多终端策略由 config.json 的 security.kick_previous_session 控制
     */
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
