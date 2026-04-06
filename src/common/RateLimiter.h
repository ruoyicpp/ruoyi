#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <chrono>
#include <vector>
#include <trantor/utils/Logger.h>

// IP 滑动窗口限流 + 自动封禁
// 算法：固定时间窗口内超过 maxRequests 次则封禁 banSeconds 秒
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

    void configure(const Config& cfg) {
        std::lock_guard<std::mutex> lk(mu_);
        cfg_ = cfg;
        for (auto& ip : cfg.whitelist) whitelist_.insert(ip);
    }

    // 检查是否允许本次请求，返回 false 表示应拒绝
    bool allow(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!cfg_.enabled) return true;
        if (whitelist_.count(ip)) return true;
        auto now = std::chrono::steady_clock::now();
        auto& info = ips_[ip];

        // ── 封禁检查 ──────────────────────────────────────────────────────
        if (info.bannedUntil > now) return false;

        // ── 清除窗口外的时间戳 ────────────────────────────────────────────
        auto cutoff = now - std::chrono::seconds(cfg_.windowSeconds);
        while (!info.timestamps.empty() && info.timestamps.front() < cutoff)
            info.timestamps.pop_front();

        // ── 检查是否超出限制 ──────────────────────────────────────────────
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

    // 手动解封
    void unban(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
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
    std::unordered_set<std::string>          whitelist_;
    std::mutex  mu_;
    Config      cfg_;

    RateLimiter() {
        // 默认白名单
        whitelist_.insert("127.0.0.1");
        whitelist_.insert("::1");
        whitelist_.insert("0.0.0.0");
    }
};
