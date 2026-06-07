/**
 * @file RateLimiter.h
 * @brief IP 限流器 — 防止暴力破解和 DDoS 攻击
 * 
 * 功能概述：
 *   - 滑动窗口限流：基于时间窗口的请求计数
 *   - 自动封禁：超过限制自动封禁 IP
 *   - Redis 支持：支持 Redis 后端实现跨进程共享
 *   - 自动降级：Redis 不可用时自动降级到内存实现
 *   - IP 白名单：支持白名单 IP 永不限流
 * 
 * 限流算法：
 *   - 固定时间窗口：统计时间窗口内的请求数
 *   - 超限封禁：超过 maxRequests 则封禁 banSeconds 秒
 *   - 滑动计数：使用内存队列或 Redis INCR 实现
 * 
 * 使用示例：
 *   // 检查是否允许请求
 *   if (!RateLimiter::instance().allow(ip)) {
 *       return error("请求过于频繁，请稍后再试");
 *   }
 *   
 *   // 针对特定操作的限流
 *   if (!RateLimiter::instance().allowKey("login:ip:" + ip, 30, 60)) {
 *       return error("登录尝试过于频繁");
 *   }
 * 
 * 配置项（config.json）：
 *   - ratelimit.enabled: 是否启用限流（默认 true）
 *   - ratelimit.maxRequests: 时间窗口内最大请求数（默认 200）
 *   - ratelimit.windowSeconds: 时间窗口大小（秒，默认 60）
 *   - ratelimit.banSeconds: 封禁时长（秒，默认 300）
 *   - ratelimit.whitelist: IP 白名单列表
 */

#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <functional>
#include <mutex>
#include <chrono>
#include <vector>
#include <trantor/utils/Logger.h>

/**
 * @class RateLimiter
 * @brief IP 限流器单例
 * 
 * 支持两种后端：
 *   1. 内存实现（默认）：单进程内一致，适合单机部署
 *   2. Redis 实现（可选）：跨进程共享，适合集群部署
 * 
 * 限流策略：
 *   - 全局限流：allow(ip) - 限制单个 IP 的全局请求速率
 *   - 操作限流：allowKey(key, max, window) - 限制特定操作的请求速率
 *   - IP 白名单：whitelist 中的 IP 永不限流
 * 
 * 算法：固定时间窗口内超过 maxRequests 次则封禁 banSeconds 秒
 */
class RateLimiter {
public:
    static RateLimiter& instance() {
        static RateLimiter inst;
        return inst;
    }

    struct Config {
        bool        enabled        = true;
        int         maxRequests    = 200;   // 窗口内最大请求数
        int         windowSeconds  = 60;    // 滑动窗口大小（秒）
        int         banSeconds     = 300;   // 触发封禁后的冷却时间（秒）
        std::vector<std::string> whitelist; // 白名单 IP 永不限流
    };

    // Redis 后端接口（由外部注入，避免头文件依赖 hiredis）
    // - incrAndExpire(key, windowSec): 返回累计计数（首次调用同时设 EXPIRE）；<0 表示后端不可用
    // - setBan(key, banSec): 记录封禁状态
    // - isBanned(key): 是否处于封禁
    // - delKey(key): 删除 key（解封/重置计数用）
    struct RedisBackend {
        std::function<long(const std::string&, int)> incrAndExpire;
        std::function<bool(const std::string&, int)> setBan;
        std::function<bool(const std::string&)>      isBanned;
        std::function<void(const std::string&)>      delKey;
    };

    void setRedisBackend(RedisBackend be) {
        std::lock_guard<std::mutex> lk(mu_);
        backend_ = std::move(be);
        useRedis_ = (backend_.incrAndExpire && backend_.setBan
                  && backend_.isBanned && backend_.delKey);
        if (useRedis_)
            LOG_INFO << "[RateLimit] Redis backend attached (cross-process counter)";
    }

    // 由 config.json 完全控制 whitelist（clear 后用配置值替换；空表示禁用本地豁免）
    // 注意：调用前 whitelist_ 已含默认 ::1（构造函数）；configure 会重置
    void configure(const Config& cfg) {
        std::lock_guard<std::mutex> lk(mu_);
        cfg_ = cfg;
        whitelist_.clear();
        for (auto& ip : cfg.whitelist) whitelist_.insert(ip);
    }

    // 检查是否允许本次请求，返回 false 表示应拒绝
    bool allow(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!cfg_.enabled) return true;
        if (whitelist_.count(ip)) return true;

        // === Redis 路径：INCR + EXPIRE 原子计数 ===
        if (useRedis_) {
            const std::string banKey = "ratelimit:ban:" + ip;
            try {
                if (backend_.isBanned(banKey)) return false;
                const std::string cntKey = "ratelimit:cnt:" + ip;
                long n = backend_.incrAndExpire(cntKey, cfg_.windowSeconds);
                if (n < 0) {
                    // Redis 不可用 → 降级内存路径
                } else if (n > cfg_.maxRequests) {
                    backend_.setBan(banKey, cfg_.banSeconds);
                    backend_.delKey(cntKey);
                    LOG_WARN << "[RateLimit][redis] IP banned: " << ip
                             << " cnt=" << n << " ban=" << cfg_.banSeconds << "s";
                    return false;
                } else {
                    return true;
                }
            } catch (...) {
                // 走下面内存路径
            }
        }

        // === 内存路径（默认 / Redis 降级） ===
        auto now = std::chrono::steady_clock::now();
        auto& info = ips_[ip];

        if (info.bannedUntil > now) return false;

        auto cutoff = now - std::chrono::seconds(cfg_.windowSeconds);
        while (!info.timestamps.empty() && info.timestamps.front() < cutoff)
            info.timestamps.pop_front();

        if ((int)info.timestamps.size() >= cfg_.maxRequests) {
            info.bannedUntil = now + std::chrono::seconds(cfg_.banSeconds);
            info.timestamps.clear();
            info.banCount++;
            LOG_WARN << "[RateLimit] IP banned: " << ip
                     << " (ban #" << info.banCount << ", "
                     << cfg_.banSeconds << "s)";
            return false;
        }

        info.timestamps.push_back(now);
        return true;
    }

    // 当前窗口内请求数
    int requestCount(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = ips_.find(ip);
        if (it == ips_.end()) return 0;
        return (int)it->second.timestamps.size();
    }

    // ── 通用 key 限流（按用户名/路径/任意维度）─────────────────────────────
    // 不会触发 ban，仅返回是否允许，调用方自行决定如何处理（如返回 429）
    // 例如：allowKey("login:" + ip + ":" + username, 5, 60)
    bool allowKey(const std::string& key, int maxReq, int windowSec) {
        if (maxReq <= 0 || windowSec <= 0) return true;
        std::lock_guard<std::mutex> lk(mu_);

        if (useRedis_) {
            try {
                long n = backend_.incrAndExpire("ratelimit:key:" + key, windowSec);
                if (n >= 0) return n <= maxReq;
            } catch (...) {}
        }

        auto now = std::chrono::steady_clock::now();
        auto& dq = keyTimestamps_[key];
        auto cutoff = now - std::chrono::seconds(windowSec);
        while (!dq.empty() && dq.front() < cutoff) dq.pop_front();
        if ((int)dq.size() >= maxReq) return false;
        dq.push_back(now);
        return true;
    }

    // 重置某个 key（成功登录后清除该用户名的失败计数等）
    void resetKey(const std::string& key) {
        std::lock_guard<std::mutex> lk(mu_);
        if (useRedis_) {
            try { backend_.delKey("ratelimit:key:" + key); } catch (...) {}
        }
        keyTimestamps_.erase(key);
    }

    // 手动解封
    void unban(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        if (useRedis_) {
            try {
                backend_.delKey("ratelimit:ban:" + ip);
                backend_.delKey("ratelimit:cnt:" + ip);
            } catch (...) {}
        }
        if (ips_.count(ip))
            ips_[ip].bannedUntil = std::chrono::steady_clock::time_point{};
        LOG_INFO << "[RateLimit] IP manually unbanned: " << ip;
    }

    // 定期清理过期 IP 记录（建议每分钟调用一次）
    void cleanup() {
        std::lock_guard<std::mutex> lk(mu_);
        auto now = std::chrono::steady_clock::now();
        for (auto it = ips_.begin(); it != ips_.end(); ) {
            auto& info = it->second;
            bool banExpired = info.bannedUntil <= now;
            bool windowEmpty = info.timestamps.empty() ||
                info.timestamps.back() < now - std::chrono::seconds(cfg_.windowSeconds);
            if (banExpired && windowEmpty)
                it = ips_.erase(it);
            else
                ++it;
        }
        // 通用 key 限流：超过 1 小时未访问的 key 清理掉，避免内存泄漏
        auto keyCutoff = now - std::chrono::hours(1);
        for (auto it = keyTimestamps_.begin(); it != keyTimestamps_.end(); ) {
            if (it->second.empty() || it->second.back() < keyCutoff)
                it = keyTimestamps_.erase(it);
            else
                ++it;
        }
    }

    struct BanEntry {
        std::string ip;
        long  remainSecs;
        int   banCount;
    };
    // 获取当前所有被封禁的 IP
    std::vector<BanEntry> bannedList() {
        std::lock_guard<std::mutex> lk(mu_);
        auto now = std::chrono::steady_clock::now();
        std::vector<BanEntry> result;
        for (auto& [ip, info] : ips_) {
            if (info.bannedUntil > now) {
                long remain = (long)std::chrono::duration_cast<std::chrono::seconds>(
                    info.bannedUntil - now).count();
                result.push_back({ip, remain, info.banCount});
            }
        }
        return result;
    }

    bool isEnabled() const { return cfg_.enabled; }

private:
    struct IpInfo {
        std::deque<std::chrono::steady_clock::time_point> timestamps;
        std::chrono::steady_clock::time_point bannedUntil{};
        int banCount = 0;
    };

    std::unordered_map<std::string, IpInfo>  ips_;
    std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> keyTimestamps_;
    std::unordered_set<std::string>          whitelist_;
    std::mutex  mu_;
    Config      cfg_;

    // Redis 后端（可选）
    bool         useRedis_ = false;
    RedisBackend backend_;

    RateLimiter() {
        // 默认 IPv6 loopback（兜底，避免开发环境本机自检被锁）。
        // 注意：configure() 会用 config.json 的 whitelist 完全替换本集合。
        // 0.0.0.0 是绑定地址，作为客户端来源 IP 永不会出现，不放入默认集。
        whitelist_.insert("::1");
    }
};
