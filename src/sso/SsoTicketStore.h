/**
 * @file SsoTicketStore.h
 * @brief SSO 票据存储 — 授权码/刷新令牌的存取（一次性消费）
 *
 * 功能概述：
 *   - 双模式存储：local（RiskStore/SQLite，单机）| redis（共享 Redis，多实例/异构后端）
 *   - 一次性票据：consume() 读取即销毁（授权码防重放）
 *   - 自动降级：redis 模式连接失败时降级 local，并记日志
 *   - TTL 支持：授权码默认 300s，刷新令牌默认 30d
 *
 * 票据类型（key 前缀区分）：
 *   - sso:code:{code}     — OAuth2 授权码（一次性，短 TTL）
 *   - sso:refresh:{token} — 刷新令牌（一次性，轮换制）
 *
 * Redis 要求：6.2+（GETDEL 原子消费）；低版本自动退化为 GET+DEL
 *
 * 配置项（config.json → security.sso）：
 *   - ticket_store: "local" | "redis"（默认 local）
 *   - code_ttl: 授权码有效期秒（默认 300）
 *   - refresh_token_ttl: 刷新令牌有效期秒（默认 2592000 = 30天）
 */

#pragma once
#include <string>
#include <optional>
#include <trantor/utils/Logger.h>
#include "../waf/RiskStore.h"
#include "../common/TokenCache.h"   // RedisConn

/**
 * @class SsoTicketStore
 * @brief SSO 票据存储单例
 *
 * 对上层暴露 put/consume/get/remove 四个原语，
 * 底层根据配置路由到 Redis 或本地 RiskStore。
 */
class SsoTicketStore {
public:
    enum class Mode { Local, Redis };

    static SsoTicketStore& instance() {
        static SsoTicketStore inst;
        return inst;
    }

    /**
     * @brief 初始化票据存储
     * @param mode "local" | "redis"
     */
    void init(const std::string& mode) {
#ifndef __linux__
        (void)mode;
        LOG_INFO << "[SSO] ticket store skipped: Linux-only feature";
        return;
#endif
        if (mode == "redis") {
            if (RedisConn::instance().enabledByConfig()) {
                mode_ = Mode::Redis;
                LOG_INFO << "[SSO] ticket store: redis (shared)";
            } else {
                mode_ = Mode::Local;
                LOG_WARN << "[SSO] ticket_store=redis 但 redis 未启用，"
                            "降级为 local（单机模式）";
            }
        } else {
            mode_ = Mode::Local;
            LOG_INFO << "[SSO] ticket store: local (RiskStore/SQLite)";
        }
    }

    /**
     * @brief 写入票据
     * @param key 完整 key（含前缀）
     * @param value 载荷（JSON 字符串）
     * @param ttlSec 有效期秒
     */
    bool put(const std::string& key, const std::string& value, int ttlSec) {
        if (mode_ == Mode::Redis) {
            auto* r = RedisConn::instance().command(
                "SETEX %b %d %b", key.data(), key.size(),
                ttlSec, value.data(), value.size());
            bool ok = r && r->type == REDIS_REPLY_STATUS;
            if (r) freeReplyObject(r);
            if (ok) return true;
            // Redis 失败降级写本地
            LOG_WARN << "[SSO] redis SETEX failed, fallback to local";
        }
        // local：RiskStore 的 ticket 即 key（issueTicket 返回随机串），
        // 这里需要自定义 key，直接走底层 kv：用 ticket 表 key=我们的 key
        return RiskStore::instance().putTicket(key, value, ttlSec);
    }

    /**
     * @brief 消费票据（读取即销毁，一次性）
     * @return 载荷，不存在/过期返回 nullopt
     */
    std::optional<std::string> consume(const std::string& key) {
        if (mode_ == Mode::Redis) {
            auto* r = RedisConn::instance().command(
                "GETDEL %b", key.data(), key.size());
            if (r) {
                std::optional<std::string> out;
                if (r->type == REDIS_REPLY_STRING && r->str)
                    out = std::string(r->str, r->len);
                freeReplyObject(r);
                return out;
            }
            // GETDEL 不可用（Redis <6.2）→ GET + DEL 退化
            r = RedisConn::instance().command("GET %b", key.data(), key.size());
            if (r) {
                std::optional<std::string> out;
                if (r->type == REDIS_REPLY_STRING && r->str)
                    out = std::string(r->str, r->len);
                freeReplyObject(r);
                RedisConn::instance().command("DEL %b", key.data(), key.size());
                return out;
            }
            return std::nullopt;
        }
        // consumeTicketEx 返回载荷（consumeTicket 只返回 bool，SSO 需要载荷）
        return RiskStore::instance().consumeTicketEx(key);
    }

    /// 非销毁读取（调试用）
    std::optional<std::string> get(const std::string& key) {
        if (mode_ == Mode::Redis) {
            auto* r = RedisConn::instance().command(
                "GET %b", key.data(), key.size());
            if (!r) return std::nullopt;
            std::optional<std::string> out;
            if (r->type == REDIS_REPLY_STRING && r->str)
                out = std::string(r->str, r->len);
            freeReplyObject(r);
            return out;
        }
        return RiskStore::instance().getTicket(key);
    }

    /// 主动删除（撤销刷新令牌等）
    void remove(const std::string& key) {
        if (mode_ == Mode::Redis) {
            auto* r = RedisConn::instance().command(
                "DEL %b", key.data(), key.size());
            if (r) freeReplyObject(r);
            return;
        }
        RiskStore::instance().removeTicket(key);
    }

    Mode mode() const { return mode_; }

private:
    SsoTicketStore() = default;
    Mode mode_ = Mode::Local;
};
