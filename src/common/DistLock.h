/**
 * @file DistLock.h
 * @brief 分布式锁 — Redis SET NX EX + Lua 原子释放/续期 + 本地降级
 *
 * 功能概述：
 *   - 加锁：SET key token NX EX ttl（原子，跨实例互斥）
 *   - 解锁：Lua 脚本 compare-and-del（只删自己的锁，防误删）
 *   - 续期：Lua 脚本 compare-and-expire（长任务防超时）
 *   - 防死锁：ttl 兜底，持锁进程崩溃后自动释放
 *   - 降级：Redis 不可用时退化为进程内互斥锁（单机仍可用）
 *   - RAII：DistLockGuard 构造加锁、析构解锁
 *
 * 使用示例：
 *   {
 *       DistLockGuard g("job:report:daily", 300);
 *       if (!g.locked()) { /* 别的实例在跑 *\/ return; }
 *       // ... 临界区 ...
 *   }   // 析构自动解锁
 *
 *   // 长任务手动续期
 *   auto token = DistLock::tryLock("task:sync", 60);
 *   if (token) { DistLock::renew("task:sync", *token, 60); ... }
 */

#pragma once
#include <string>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>
#include <hiredis/hiredis.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
#include "TokenCache.h"   // RedisConn

/**
 * @class DistLock
 * @brief 分布式锁（静态方法集）
 */
class DistLock {
public:
    /**
     * @brief 尝试加锁（非阻塞）
     * @param key 锁名（自动加 redis keyPrefix）
     * @param ttlSec 锁超时秒数（防死锁兜底）
     * @return 成功返回锁 token（解锁/续期凭据），失败返回 nullopt
     */
    static std::optional<std::string> tryLock(const std::string& key, int ttlSec) {
        auto& rc = RedisConn::instance();
        if (rc.available()) {
            std::string token = drogon::utils::getUuid();
            std::string rk = rc.prefixKey("lock:" + key);
            auto* r = rc.command("SET %s %s NX EX %d",
                                 rk.c_str(), token.c_str(), ttlSec);
            if (!r) { rc.markBad(); return localLock(key, ttlSec); }
            bool ok = (r->type == REDIS_REPLY_STATUS &&
                       std::string(r->str) == "OK");
            freeReplyObject(r);
            if (ok) return token;
            return std::nullopt;   // 已被其他实例持有
        }
        return localLock(key, ttlSec);
    }

    /**
     * @brief 解锁（Lua 原子：值匹配才删，防误删他人锁）
     */
    static void unlock(const std::string& key, const std::string& token) {
        auto& rc = RedisConn::instance();
        if (rc.available()) {
            std::string rk = rc.prefixKey("lock:" + key);
            static const char* lua =
                "if redis.call('get',KEYS[1])==ARGV[1] then "
                "return redis.call('del',KEYS[1]) else return 0 end";
            auto* r = rc.command("EVAL %s 1 %s %s", lua, rk.c_str(), token.c_str());
            if (r) freeReplyObject(r); else rc.markBad();
            return;
        }
        localUnlock(key, token);
    }

    /**
     * @brief 续期（Lua 原子：值匹配才续，防续他人锁）
     * @return true=续期成功（仍持有锁）
     */
    static bool renew(const std::string& key, const std::string& token, int ttlSec) {
        auto& rc = RedisConn::instance();
        if (rc.available()) {
            std::string rk = rc.prefixKey("lock:" + key);
            static const char* lua =
                "if redis.call('get',KEYS[1])==ARGV[1] then "
                "return redis.call('expire',KEYS[1],ARGV[2]) else return 0 end";
            auto* r = rc.command("EVAL %s 1 %s %s %d",
                                 lua, rk.c_str(), token.c_str(), ttlSec);
            if (!r) { rc.markBad(); return false; }
            bool ok = (r->type == REDIS_REPLY_INTEGER && r->integer == 1);
            freeReplyObject(r);
            return ok;
        }
        return localRenew(key, token, ttlSec);
    }

    /// 阻塞式加锁（带超时和重试间隔）
    static std::optional<std::string> lock(const std::string& key, int ttlSec,
                                           int waitMs = 5000, int retryMs = 50) {
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(waitMs);
        while (std::chrono::steady_clock::now() < deadline) {
            auto t = tryLock(key, ttlSec);
            if (t) return t;
            std::this_thread::sleep_for(std::chrono::milliseconds(retryMs));
        }
        return std::nullopt;
    }

private:
    // ── 本地降级（Redis 不可用时进程内互斥）──────────────────────────
    struct LocalLock { std::string token; std::chrono::steady_clock::time_point expireAt; };
    static std::unordered_map<std::string, LocalLock>& localMap() {
        static std::unordered_map<std::string, LocalLock> m; return m;
    }
    static std::mutex& localMu() { static std::mutex m; return m; }

    static std::optional<std::string> localLock(const std::string& key, int ttlSec) {
        std::lock_guard<std::mutex> lk(localMu());
        auto now = std::chrono::steady_clock::now();
        auto it = localMap().find(key);
        if (it != localMap().end() && it->second.expireAt > now)
            return std::nullopt;   // 本地已被持有且未过期
        std::string token = drogon::utils::getUuid();
        localMap()[key] = {token, now + std::chrono::seconds(ttlSec)};
        return token;
    }

    static void localUnlock(const std::string& key, const std::string& token) {
        std::lock_guard<std::mutex> lk(localMu());
        auto it = localMap().find(key);
        if (it != localMap().end() && it->second.token == token)
            localMap().erase(it);
    }

    static bool localRenew(const std::string& key, const std::string& token, int ttlSec) {
        std::lock_guard<std::mutex> lk(localMu());
        auto it = localMap().find(key);
        if (it == localMap().end() || it->second.token != token) return false;
        it->second.expireAt = std::chrono::steady_clock::now() +
                              std::chrono::seconds(ttlSec);
        return true;
    }
};

/**
 * @class DistLockGuard
 * @brief RAII 分布式锁守卫：构造加锁、析构解锁
 */
class DistLockGuard {
public:
    DistLockGuard(const std::string& key, int ttlSec, int waitMs = 0) {
        if (waitMs > 0) token_ = DistLock::lock(key, ttlSec, waitMs);
        else            token_ = DistLock::tryLock(key, ttlSec);
        key_ = key;
    }
    ~DistLockGuard() {
        if (token_) DistLock::unlock(key_, *token_);
    }
    bool locked() const { return token_.has_value(); }
    bool renew(int ttlSec) {
        return token_ && DistLock::renew(key_, *token_, ttlSec);
    }
    const std::string& token() const { return *token_; }

    DistLockGuard(const DistLockGuard&) = delete;
    DistLockGuard& operator=(const DistLockGuard&) = delete;

private:
    std::string key_;
    std::optional<std::string> token_;
};
