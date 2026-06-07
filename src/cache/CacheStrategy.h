/**
 * @file CacheStrategy.h
 * @brief 多层缓存策略接口 - L1: Redis, L2: 本地内存
 */

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <variant>
#include <vector>
#include <json/json.h>

namespace Cache {

// ── 缓存值类型 ──────────────────────────────────────────────────────────────

using CacheValue = std::variant<std::monostate, std::string, Json::Value>;

struct CacheEntry {
    CacheValue value;
    std::chrono::steady_clock::time_point expiresAt;
    bool isNull = false; // 空值缓存标记（防穿透）

    bool isExpired() const {
        return std::chrono::steady_clock::now() >= expiresAt;
    }

    bool isNullValue() const { return isNull; }
};

// ── 缓存策略 ───────────────────────────────────────────────────────────────

struct CacheStats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> sets{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> errors{0};

    double hitRate() const {
        uint64_t total = hits.load() + misses.load();
        if (total == 0) {
            return 0.0;
        }
        return static_cast<double>(hits.load()) / static_cast<double>(total) * 100.0;
    }

    void reset() {
        hits = 0;
        misses = 0;
        sets = 0;
        evictions = 0;
        errors = 0;
    }
};

struct CacheConfig {
    bool enabled = true;
    int localTtlSeconds = 60;
    int redisTtlSeconds = 3600;
    int maxLocalEntries = 10000;
    bool nullValueCaching = true;
    int nullValueTtlSeconds = 60;
    int lockTimeoutMs = 5000;
    bool enableRandomExpiryJitter = true;
    int randomExpiryJitterSeconds = 60;
};

// ── 分布式锁 ───────────────────────────────────────────────────────────────

class DistributedLock {
public:
    DistributedLock(const std::string& key, int timeoutMs);
    ~DistributedLock();

    bool tryAcquire();
    void release();
    bool isAcquired() const { return acquired_; }

private:
    std::string key_;
    std::string token_;
    int timeoutMs_;
    bool acquired_ = false;
};

// ── 缓存策略接口 ───────────────────────────────────────────────────────────

class CacheStrategy {
public:
    static CacheStrategy& instance();

    void initialize(const CacheConfig& config);
    void shutdown();

    // 基础操作
    std::optional<CacheEntry> get(const std::string& key);
    bool set(const std::string& key, const CacheValue& value, int ttlSeconds = 0);
    bool remove(const std::string& key);
    bool exists(const std::string& key);

    // 防穿透: getOrSet
    CacheEntry getOrSet(const std::string& key,
                        const std::function<CacheValue()>& loader,
                        int ttlSeconds = 0);

    // 分布式锁 (Redis)
    std::unique_ptr<DistributedLock> acquireLock(const std::string& key, int timeoutMs = 5000);

    // 批量操作
    std::map<std::string, std::optional<CacheEntry>> mget(const std::vector<std::string>& keys);
    void mset(const std::map<std::string, CacheValue>& items, int ttlSeconds = 0);
    void mremove(const std::vector<std::string>& keys);

    // 模式删除
    size_t removeByPattern(const std::string& pattern);

    // 统计
    const CacheStats& stats() const { return stats_; }
    CacheStats& stats() { return stats_; }

    // 清空
    void clear();

private:
    CacheStrategy() = default;

    std::optional<CacheEntry> getFromLocal(const std::string& key);
    std::optional<CacheEntry> getFromRedis(const std::string& key);
    bool setToLocal(const std::string& key, const CacheEntry& entry);
    bool setToRedis(const std::string& key, const CacheEntry& entry);
    bool removeFromLocal(const std::string& key);
    bool removeFromRedis(const std::string& key);

    int randomizeTtl(int baseTtl) const;

    mutable std::shared_mutex localMutex_;
    std::map<std::string, CacheEntry> localCache_;
    CacheConfig config_;
    CacheStats stats_;
};

} // namespace Cache
