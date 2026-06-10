/**
 * @file CacheStrategy.h
 * @brief 多层缓存策略接口 — L1: 本地内存, L2: Redis 集群
 * 
 * 功能概述：
 *   - 多层缓存架构：本地内存 + Redis 分布式缓存
 *   - 缓存穿透防护：空值缓存
 *   - 缓存雪崩防护：随机过期时间抖动
 *   - 缓存击穿防护：分布式锁
 *   - 性能监控：命中率、响应时间等指标
 * 
 * 缓存层级：
 *   - L1（本地内存）：快速访问，TTL 短（默认 60 秒）
 *   - L2（Redis）：分布式共享，TTL 长（默认 3600 秒）
 *   - 本地最多 10000 条记录，超出时自动驱逐
 * 
 * 安全防护：
 *   - 空值缓存：防止缓存穿透（查询不存在的数据）
 *   - 过期时间抖动：防止缓存雪崩（大量键同时过期）
 *   - 分布式锁：防止缓存击穿（热点数据并发查询）
 * 
 * @see Cache::CacheEntry - 缓存条目结构
 * @see Cache::CacheStats - 缓存统计信息
 * @see Cache::CacheConfig - 缓存配置
 * @see Cache::RedisCluster - Redis 集群管理
 */

/**
 * @file CacheStrategy.h
 * @brief 缓存策略管理 — 实现多种缓存淘汰和更新策略
 * 
 * 功能概述：
 *   - 缓存淘汰策略：LRU、LFU、TTL 等多种淘汰算法
 *   - 缓存更新策略：Cache-Aside、Write-Through、Write-Behind
 *   - 缓存预热：应用启动时预加载热数据
 *   - 缓存失效：支持单键失效、模式匹配失效、全量清空
 *   - 性能监控：缓存命中率、平均响应时间、内存占用
 * 
 * 核心特性：
 *   - 多策略支持：可根据业务需求选择不同的缓存策略
 *   - 自适应淘汰：根据访问模式动态调整淘汰策略
 *   - 热点识别：自动识别热点数据并提高缓存优先级
 *   - 分层缓存：支持本地缓存 + Redis 缓存的分层架构
 * 
 * 配置项（config.json）：
 *   - cache.strategy: "lru" | "lfu" | "ttl"（默认 "lru"）
 *   - cache.max_size: 最大缓存条目数（默认 10000）
 *   - cache.ttl_seconds: 默认过期时间（秒，默认 3600）
 *   - cache.update_mode: "aside" | "through" | "behind"（默认 "aside"）
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
#include "RedisCluster.h"

/**
 * @namespace Cache
 * @brief 缓存管理命名空间
 */
namespace Cache {

// ── 缓存值类型 ──────────────────────────────────────────────────────────────

/**
 * @typedef CacheValue
 * @brief 缓存值类型 — 支持字符串和 JSON 两种类型
 */
using CacheValue = std::variant<std::monostate, std::string, Json::Value>;

/**
 * @struct CacheEntry
 * @brief 缓存条目结构
 * 
 * 表示缓存中的一条记录，包含值、过期时间和空值标记。
 */
struct CacheEntry {
    CacheValue value;                                      ///< 缓存值（字符串或 JSON）
    std::chrono::steady_clock::time_point expiresAt;      ///< 过期时间
    bool isNull = false;                                   ///< 空值缓存标记（防穿透）

    /**
     * @brief 检查缓存是否已过期
     * @return true 如果已过期，false 如果仍有效
     */
    bool isExpired() const {
        return std::chrono::steady_clock::now() >= expiresAt;
    }

    /**
     * @brief 检查是否为空值缓存
     * @return true 如果是空值缓存，false 否则
     */
    bool isNullValue() const { return isNull; }
};

// ── 缓存统计信息 ───────────────────────────────────────────────────────────

/**
 * @struct CacheStats
 * @brief 缓存统计信息
 * 
 * 记录缓存系统的性能指标，包括命中率、操作次数等。
 * 所有字段都是原子操作，支持多线程并发访问。
 */
struct CacheStats {
    std::atomic<uint64_t> hits{0};         ///< 缓存命中次数
    std::atomic<uint64_t> misses{0};       ///< 缓存未命中次数
    std::atomic<uint64_t> sets{0};         ///< 缓存设置次数
    std::atomic<uint64_t> evictions{0};    ///< 缓存驱逐次数（LRU）
    std::atomic<uint64_t> errors{0};       ///< 缓存操作错误次数

    /**
     * @brief 计算缓存命中率
     * @return 命中率百分比（0-100）
     */
    double hitRate() const {
        uint64_t total = hits.load() + misses.load();
        if (total == 0) {
            return 0.0;
        }
        return static_cast<double>(hits.load()) / static_cast<double>(total) * 100.0;
    }

    /**
     * @brief 重置所有统计数据
     */
    void reset() {
        hits = 0;
        misses = 0;
        sets = 0;
        evictions = 0;
        errors = 0;
    }
};

// ── 缓存配置 ───────────────────────────────────────────────────────────────

/**
 * @struct CacheConfig
 * @brief 缓存配置结构
 * 
 * 配置缓存系统的各项参数，包括 TTL、大小限制、防护策略等。
 */
struct CacheConfig {
    bool enabled = true;                           ///< 是否启用缓存
    int localTtlSeconds = 60;                      ///< L1（本地）缓存 TTL（秒）
    int redisTtlSeconds = 3600;                    ///< L2（Redis）缓存 TTL（秒）
    int maxLocalEntries = 10000;                   ///< L1 缓存最大条目数
    bool nullValueCaching = true;                  ///< 是否缓存空值（防穿透）
    int nullValueTtlSeconds = 60;                  ///< 空值缓存 TTL（秒）
    int lockTimeoutMs = 5000;                      ///< 分布式锁超时（毫秒）
    bool enableRandomExpiryJitter = true;          ///< 是否启用过期时间抖动（防雪崩）
    int randomExpiryJitterSeconds = 60;            ///< 过期时间抖动范围（秒）
    ClusterConfig redisCluster;                    ///< Redis 集群配置
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
    void init(const CacheConfig& config) { initialize(config); }
    void shutdown();
    void warmup();

    // 基础操作
    std::optional<CacheEntry> get(const std::string& key);
    bool set(const std::string& key, const CacheValue& value, int ttlSeconds = 0);
    bool remove(const std::string& key);
    bool exists(const std::string& key);

    // 防穿透: getOrSet
    CacheEntry getOrSet(const std::string& key,
                        const std::function<CacheValue()>& loader,
                        int ttlSeconds = 0);
    CacheEntry getWithLock(const std::string& key,
                           const std::function<CacheValue()>& loader,
                           int ttlSeconds = 0);

    // 分布式锁 (Redis)
    std::unique_ptr<DistributedLock> acquireLock(const std::string& key, int timeoutMs = 5000);

    // 批量操作
    std::map<std::string, std::optional<CacheEntry>> mget(const std::vector<std::string>& keys);
    void mset(const std::map<std::string, CacheValue>& items, int ttlSeconds = 0);
    void mremove(const std::vector<std::string>& keys);
    void warmupHotKeys(const std::vector<std::string>& keys, int ttlSeconds = 0);

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
    int resolveTtl(int ttlSeconds, bool isNullValue) const;

    mutable std::shared_mutex localMutex_;
    std::map<std::string, CacheEntry> localCache_;
    CacheConfig config_;
    CacheStats stats_;
};

} // namespace Cache
