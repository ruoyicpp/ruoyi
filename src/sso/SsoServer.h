/**
 * @file SsoServer.h
 * @brief SSO 单点登录服务端 — OAuth2.0 授权码流程 + OIDC 发现
 *
 * 功能概述：
 *   - 授权码流程：authorize 签发 code → token 换 access_token + refresh_token
 *   - 令牌格式：access_token 复用系统 JWT（HS256/RS256），接入方可本地验签
 *   - 刷新令牌：一次性票据，轮换制（用后旧票作废发新票）
 *   - 客户端管理：client_id/secret/redirect_uri 白名单，配置 + 运行时增删
 *   - 票据双模：local（RiskStore/SQLite）| redis（共享，跨实例/异构后端）
 *   - OIDC 发现：/.well-known/openid-configuration
 *
 * 接入方式（异构后端）：
 *   - 纯 JS/Vue：authorize 拿 code → 后端换 token → userinfo 拿用户信息
 *   - Java/其他后端：POST /sso/introspect 校验 token 有效性
 *   - 同域前端：直接带 Authorization: Bearer <jwt>
 *
 * 配置项（config.json → security.sso）：
 *   - enabled: 总开关（默认 false）
 *   - issuer: 签发方标识（留空自动推导：menu.api_base_url → listeners[0]）
 *   - ticket_store: "local"|"redis"（默认 local）
 *   - code_ttl: 授权码秒（默认 300）
 *   - refresh_token_ttl: 刷新令牌秒（默认 2592000）
 *   - clients: [{client_id, client_secret, name, redirect_uris[], scopes[]}]
 */

#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <shared_mutex>
#include <sstream>
#include <ctime>
#include <json/json.h>
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include "SsoTicketStore.h"
#include "../common/JwtUtils.h"
#include "../common/TokenCache.h"
#include "../common/LoginUser.h"

/**
 * @class SsoServer
 * @brief SSO 服务端单例
 */
class SsoServer {
public:
    /// 接入方（OAuth2 client）
    struct ClientInfo {
        std::string clientId;
        std::string clientSecret;   ///< 空 = 公开客户端（纯JS，不校验secret）
        std::string name;
        std::vector<std::string> redirectUris;  ///< 回调地址白名单
        std::vector<std::string> scopes;        ///< 允许 scope（默认 openid profile）
        bool enabled = true;
    };

    static SsoServer& instance() {
        static SsoServer inst;
        return inst;
    }

    /**
     * @brief 初始化 SSO 服务端
     * @param cfg config.json 的 security.sso 节点
     * @note 仅 Linux 版本启用；其他平台直接跳过
     */
    void init(const Json::Value& cfg) {
#ifndef __linux__
        (void)cfg;
        LOG_INFO << "[SSO] skipped: feature only available on Linux build";
        return;
#endif
        enabled_    = cfg.get("enabled", false).asBool();
        codeTtl_    = cfg.get("code_ttl", 300).asInt();
        refreshTtl_ = cfg.get("refresh_token_ttl", 2592000).asInt();

        // issuer 三级推导：sso.issuer 显式配置 → menu.api_base_url（公网地址约定）
        // → 首个 listener（address+port+https）。不写死 localhost。
        issuer_ = cfg.get("issuer", "").asString();
        if (issuer_.empty()) {
            auto& root = drogon::app().getCustomConfig();
            issuer_ = root["menu"].get("api_base_url", "").asString();
        }
        if (issuer_.empty()) {
            auto& root = drogon::app().getCustomConfig();
            if (root.isMember("listeners") && root["listeners"].size() > 0) {
                auto& l = root["listeners"][0];
                issuer_ = std::string(l.get("https", false).asBool() ? "https" : "http")
                        + "://" + l.get("address", "127.0.0.1").asString()
                        + ":" + std::to_string(l.get("port", 80).asInt());
            }
        }
        if (issuer_.empty()) issuer_ = "http://127.0.0.1";   // 兜底
        LOG_INFO << "[SSO] issuer: " << issuer_;

        SsoTicketStore::instance().init(
            cfg.get("ticket_store", "local").asString());

        for (auto& c : cfg["clients"]) {
            ClientInfo ci;
            ci.clientId     = c.get("client_id", "").asString();
            ci.clientSecret = c.get("client_secret", "").asString();
            ci.name         = c.get("name", ci.clientId).asString();
            for (auto& u : c["redirect_uris"]) ci.redirectUris.push_back(u.asString());
            for (auto& s : c["scopes"])        ci.scopes.push_back(s.asString());
            if (!ci.clientId.empty()) clients_[ci.clientId] = std::move(ci);
        }
        LOG_INFO << "[SSO] " << (enabled_ ? "enabled" : "disabled")
                 << " issuer=" << issuer_
                 << " clients=" << clients_.size();
    }

    bool isEnabled() const { return enabled_; }
    const std::string& issuer() const { return issuer_; }

    // ── 客户端管理 ────────────────────────────────────────────────────

    /// 校验 client_id + redirect_uri（redirect_uri 必须在白名单内）
    bool validateClient(const std::string& clientId,
                        const std::string& redirectUri) const {
        std::shared_lock<std::shared_mutex> lk(mu_);
        auto it = clients_.find(clientId);
        if (it == clients_.end() || !it->second.enabled) return false;
        if (it->second.redirectUris.empty()) return true;  // 未配置=不校验
        for (auto& u : it->second.redirectUris)
            if (u == redirectUri) return true;
        return false;
    }

    /// 校验 client_secret（公开客户端 secret 为空则跳过）
    bool validateSecret(const std::string& clientId,
                        const std::string& secret) const {
        std::shared_lock<std::shared_mutex> lk(mu_);
        auto it = clients_.find(clientId);
        if (it == clients_.end()) return false;
        if (it->second.clientSecret.empty()) return true;  // 公开客户端
        return it->second.clientSecret == secret;
    }

    /// 注册/更新客户端（运行时，重启后以 config 为准）
    void upsertClient(const ClientInfo& ci) {
        std::unique_lock<std::shared_mutex> lk(mu_);
        clients_[ci.clientId] = ci;
    }
    bool removeClient(const std::string& clientId) {
        std::unique_lock<std::shared_mutex> lk(mu_);
        return clients_.erase(clientId) > 0;
    }
    std::vector<ClientInfo> listClients() const {
        std::shared_lock<std::shared_mutex> lk(mu_);
        std::vector<ClientInfo> out;
        for (auto& [_, c] : clients_) out.push_back(c);
        return out;
    }

    // ── 授权码流程 ────────────────────────────────────────────────────

    /**
     * @brief 签发授权码（authorize 端点）
     * @param user 已登录用户
     * @param clientId 接入方
     * @param redirectUri 回调地址
     * @return 授权码（调用方拼 redirect_uri?code=xx&state=yy）
     */
    std::string issueCode(const LoginUser& user,
                          const std::string& clientId,
                          const std::string& redirectUri) {
        std::string code = drogon::utils::getUuid();
        Json::Value payload;
        payload["userId"]       = (Json::Int64)user.userId;
        payload["userName"]     = user.userName;
        payload["deptId"]       = (Json::Int64)user.deptId;
        payload["client_id"]    = clientId;
        payload["redirect_uri"] = redirectUri;
        SsoTicketStore::instance().put(
            "sso:code:" + code,
            Json::writeString(Json::StreamWriterBuilder(), payload),
            codeTtl_);
        return code;
    }

    /**
     * @brief 授权码换令牌（token 端点，grant_type=authorization_code）
     * @return 成功返回 token 响应 JSON，失败返回 nullopt
     */
    std::optional<Json::Value> exchangeCode(const std::string& code,
                                            const std::string& clientId,
                                            const std::string& redirectUri) {
        auto payloadStr = SsoTicketStore::instance().consume("sso:code:" + code);
        if (!payloadStr) return std::nullopt;

        Json::Value p; std::string err;
        Json::CharReaderBuilder rb;
        std::istringstream ss(*payloadStr);
        if (!Json::parseFromStream(rb, ss, &p, &err)) return std::nullopt;

        // 校验 code 绑定的 client/redirect_uri 与本次请求一致（防 code 盗用）
        if (p["client_id"].asString() != clientId) return std::nullopt;
        if (!redirectUri.empty() &&
            p["redirect_uri"].asString() != redirectUri) return std::nullopt;

        return issueTokenPair(p["userId"].asInt64(),
                              p["userName"].asString(),
                              p["deptId"].asInt64(),
                              clientId);
    }

    /**
     * @brief 刷新令牌轮换（grant_type=refresh_token）
     */
    std::optional<Json::Value> refreshToken(const std::string& refreshTok) {
        auto payloadStr = SsoTicketStore::instance().consume(
            "sso:refresh:" + refreshTok);
        if (!payloadStr) return std::nullopt;
        Json::Value p; std::string err;
        Json::CharReaderBuilder rb;
        std::istringstream ss(*payloadStr);
        if (!Json::parseFromStream(rb, ss, &p, &err)) return std::nullopt;
        return issueTokenPair(p["userId"].asInt64(),
                              p["userName"].asString(),
                              p["deptId"].asInt64(),
                              p["client_id"].asString());
    }

    /**
     * @brief 撤销刷新令牌（logout）
     */
    void revokeRefresh(const std::string& refreshTok) {
        SsoTicketStore::instance().remove("sso:refresh:" + refreshTok);
    }

    /// OIDC 发现文档
    Json::Value discoveryDoc() const {
        Json::Value d;
        d["issuer"]                 = issuer_;
        d["authorization_endpoint"] = issuer_ + "/sso/authorize";
        d["token_endpoint"]         = issuer_ + "/sso/token";
        d["userinfo_endpoint"]      = issuer_ + "/sso/userinfo";
        d["introspection_endpoint"] = issuer_ + "/sso/introspect";
        d["revocation_endpoint"]    = issuer_ + "/sso/revoke";
        d["response_types_supported"] = Json::Value(Json::arrayValue);
        d["response_types_supported"].append("code");
        d["grant_types_supported"] = Json::Value(Json::arrayValue);
        d["grant_types_supported"].append("authorization_code");
        d["grant_types_supported"].append("refresh_token");
        d["subject_types_supported"] = Json::Value(Json::arrayValue);
        d["subject_types_supported"].append("public");
        d["id_token_signing_alg_values_supported"] = Json::Value(Json::arrayValue);
        d["id_token_signing_alg_values_supported"].append("HS256");
        return d;
    }

private:
    SsoServer() = default;

    /// 签发 access_token(JWT) + refresh_token(票据) 对
    Json::Value issueTokenPair(long userId, const std::string& userName,
                               long deptId, const std::string& clientId) {
        // access_token 复用系统 JWT：uuid 作为 TokenCache key
        std::string uuid  = drogon::utils::getUuid();
        std::string token = JwtUtils::createToken(uuid, userId, userName, deptId);

        // 写入 TokenCache，使 TokenService::getLoginUser 对 SSO token 同样生效
        LoginUser lu;
        lu.userId   = userId;
        lu.userName = userName;
        lu.deptId   = deptId;
        lu.token    = uuid;
        lu.loginTime = std::time(nullptr) * 1000;
        TokenCache::instance().set(uuid, lu);

        // refresh_token：一次性票据，轮换制
        std::string rt = drogon::utils::getUuid() + drogon::utils::getUuid();
        Json::Value rp;
        rp["userId"]    = (Json::Int64)userId;
        rp["userName"]  = userName;
        rp["deptId"]    = (Json::Int64)deptId;
        rp["client_id"] = clientId;
        SsoTicketStore::instance().put(
            "sso:refresh:" + rt,
            Json::writeString(Json::StreamWriterBuilder(), rp),
            refreshTtl_);

        Json::Value out;
        out["access_token"]  = token;
        out["token_type"]    = "Bearer";
        out["expires_in"]    = JwtUtils::config().jwtExpireDays * 86400;
        out["refresh_token"] = rt;
        out["scope"]         = "openid profile";
        return out;
    }

    bool        enabled_    = false;
    std::string issuer_;
    int         codeTtl_    = 300;
    int         refreshTtl_ = 2592000;
    std::unordered_map<std::string, ClientInfo> clients_;
    mutable std::shared_mutex mu_;
};
