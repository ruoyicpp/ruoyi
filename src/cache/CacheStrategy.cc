/**
 * @file CacheStrategy.cc
 * @brief 多层缓存策略实现
 */

#include "CacheStrategy.h"

#include <random>
#include <thread>
#include <chrono>
#include <cstdlib>

namespace Cache {

// ── DistributedLock ────────────────────────────────────────────────────────

DistributedLock::DistributedLock(const std::string& key, int timeoutMs)
    : key_("lock:" + key), timeoutMs_(timeoutMs) {
}

DistributedLock::~DistributedLock() {
    if (acquired_) {
        release();
    }
}

bool DistributedLock::tryAcquire() {
    // TODO: 使用 Redis SET NX PX 实现分布式锁
    acquired_ = true;
    return true;
}

void DistributedLock::release() {
    if (acquired_) {
        // TODO: 使用 Lua 脚本释放锁 (只释放自己的 token)
        acquired_ = false;
    }
}

// ── CacheStrategy ───────────────────────────────────────────────────────────

CacheStrategy& CacheStrategy::instance() {
    static CacheStrategy instance;
    return instance;
}

void CacheStrategy::initialize(const CacheConfig& config) {
    config_ = config;
    // TODO: 连接 Redis
}

void CacheStrategy::shutdown() {
    clear();
}

std::optional<CacheEntry> CacheStrategy::get(const std::string& key) {
    // L1: 本地内存
    if (auto entry = getFromLocal(key)) {
        if (!entry->isExpired()) {
            stats_.hits.fetch_add(1, std::memory_order_relaxed);
            return entry;
        }
        removeFromLocal(key);
    }

    // L2: Redis
    if (auto entry = getFromRedis(key)) {
        if (!entry->isExpired()) {
            stats_.hits.fetch_add(1, std::memory_order_relaxed);
            // 回填本地缓存
            setToLocal(key, *entry);
            return entry;
        }
        removeFromRedis(key);
    }

    stats_.misses.fetch_add(1, std::memory_order_relaxed);
    return std::nullopt;
}

bool CacheStrategy::set(const std::string& key, const CacheValue& value, int ttlSeconds) {
    if (!config_.enabled) {
        return false;
    }

    int effectiveTtl = ttlSeconds > 0 ? ttlSeconds : config_.redisTtlSeconds;
    if (config_.enableRandomExpiryJitter) {
        effectiveTtl = randomizeTtl(effectiveTtl);
    }

    CacheEntry entry;
    entry.value = value;
    entry.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(effectiveTtl);
    entry.isNull = std::holds_alternative<std::monostate>(value);

    bool redisOk = setToRedis(key, entry);
    bool localOk = setToLocal(key, entry);

    stats_.sets.fetch_add(1, std::memory_order_relaxed);
    return redisOk && localOk;
}

bool CacheStrategy::remove(const std::string& key) {
    bool redisOk = removeFromRedis(key);
    bool localOk = removeFromLocal(key);
    return redisOk || localOk;
}

bool CacheStrategy::exists(const std::string& key) {
    if (auto entry = getFromLocal(key)) {
        return !entry->isExpired();
    }
    if (auto entry = getFromRedis(key)) {
        return !entry->isExpired();
    }
    return false;
}

CacheEntry CacheStrategy::getOrSet(const std::string& key,
                                   const std::function<CacheValue()>& loader,
                                   int ttlSeconds) {
    // 先查缓存
    if (auto entry = get(key)) {
        return *entry;
    }

    // 尝试获取分布式锁防止击穿
    auto lock = acquireLock(key, config_.lockTimeoutMs);

    // Double-check
    if (auto entry = get(key)) {
        return *entry;
    }

    // 加载数据
    CacheValue value;
    try {
        value = loader();
    } catch (...) {
        value = std::monostate{};
    }

    // 空值缓存防穿透
    if (std::holds_alternative<std::monostate>(value) && !config_.nullValueCaching) {
        CacheEntry nullEntry;
        nullEntry.isNull = true;
        nullEntry.expiresAt = std::chrono::steady_clock::now() +
                              std::chrono::seconds(config_.nullValueTtlSeconds);
        return nullEntry;
    }

    set(key, value, ttlSeconds);

    CacheEntry entry;
    entry.value = value;
    entry.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds);
    return entry;
}

std::unique_ptr<DistributedLock> CacheStrategy::acquireLock(const std::string& key, int timeoutMs) {
    auto lock = std::make_unique<DistributedLock>(key, timeoutMs);
    if (lock->tryAcquire()) {
        return lock;
    }
    return nullptr;
}

std::map<std::string, std::optional<CacheEntry>> CacheStrategy::mget(const std::vector<std::string>& keys) {
    std::map<std::string, std::optional<CacheEntry>> results;

    for (const auto& key : keys) {
        results[key] = get(key);
    }

    return results;
}

void CacheStrategy::mset(const std::map<std::string, CacheValue>& items, int ttlSeconds) {
    for (const auto& [key, value] : items) {
        set(key, value, ttlSeconds);
    }
}

void CacheStrategy::mremove(const std::vector<std::string>& keys) {
    for (const auto& key : keys) {
        remove(key);
    }
}

size_t CacheStrategy::removeByPattern(const std::string& pattern) {
    size_t count = 0;

    // 清理本地缓存
    {
        std::unique_lock lock(localMutex_);
        for (auto it = localCache_.begin(); it != localCache_.end(); ) {
            // 简单前缀匹配
            if (it->first.find(pattern) == 0) {
                it = localCache_.erase(it), ++count;
            } else {
                ++it;
            }
        }
    }

    // TODO: Redis SCAN + DEL

    return count;
}

void CacheStrategy::clear() {
    {
        std::unique_lock lock(localMutex_);
        localCache_.clear();
    }
    // TODO: Redis FLUSHDB
}

std::optional<CacheEntry> CacheStrategy::getFromLocal(const std::string& key) {
    std::shared_lock lock(localMutex_);
    auto it = localCache_.find(key);
    if (it != localCache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<CacheEntry> CacheStrategy::getFromRedis(const std::string& key) {
    // TODO: 实际实现 Redis GET
    (void)key;
    return std::nullopt;
}

bool CacheStrategy::setToLocal(const std::string& key, const CacheEntry& entry) {
    std::unique_lock lock(localMutex_);

    // LRU: 如果超过最大条目，删除最旧的
    if (static_cast<int>(localCache_.size()) >= config_.maxLocalEntries &&
        localCache_.find(key) == localCache_.end()) {
        if (!localCache_.empty()) {
            localCache_.erase(localCache_.begin());
            stats_.evictions.fetch_add(1, std::memory_order_relaxed);
        }
    }

    localCache_[key] = entry;
    return true;
}

bool CacheStrategy::setToRedis(const std::string& key, const CacheEntry& entry) {
    // TODO: 实际实现 Redis SETEX
    (void)key;
    (void)entry;
    return true;
}

bool CacheStrategy::removeFromLocal(const std::string& key) {
    std::unique_lock lock(localMutex_);
    localCache_.erase(key);
    return true;
}

bool CacheStrategy::removeFromRedis(const std::string& key) {
    // TODO: 实际实现 Redis DEL
    (void)key;
    return true;
}

int CacheStrategy::randomizeTtl(int baseTtl) const {
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dis(0, config_.randomExpiryJitterSeconds);
    return baseTtl + dis(gen);
}

} // namespace Cache
