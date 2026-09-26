/**
 * @file CacheStrategy.cc
 * @brief 多层缓存策略实现
 */

#include "CacheStrategy.h"

#include "CacheWarmup.h"
#include "RedisCluster.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <json/json.h>
#include <random>
#include <sstream>
#include <thread>
#include <trantor/utils/Logger.h>

namespace Cache {
namespace {

std::string serializeCacheValue(const CacheEntry& entry) {
    Json::Value root;
    root["isNull"] = entry.isNull;
    root["expiresInMs"] = Json::Int64(std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.expiresAt - std::chrono::steady_clock::now()).count());

    if (std::holds_alternative<std::string>(entry.value)) {
        root["type"] = "string";
        root["value"] = std::get<std::string>(entry.value);
    } else if (std::holds_alternative<Json::Value>(entry.value)) {
        root["type"] = "json";
        root["value"] = std::get<Json::Value>(entry.value);
    } else {
        root["type"] = "null";
        root["value"] = Json::Value();
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::optional<CacheEntry> deserializeCacheValue(const std::string& rawValue) {
    if (rawValue.empty()) {
        return std::nullopt;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream input(rawValue);
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        return std::nullopt;
    }

    CacheEntry entry;
    entry.isNull = root.get("isNull", false).asBool();

    const auto expiresInMs = root.get("expiresInMs", Json::Int64(0)).asInt64();
    entry.expiresAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(expiresInMs);

    const auto type = root.get("type", "null").asString();
    if (type == "string") {
        entry.value = root["value"].asString();
    } else if (type == "json") {
        entry.value = root["value"];
    } else {
        entry.value = std::monostate{};
    }

    return entry;
}

bool matchesPattern(const std::string& value, const std::string& pattern) {
    if (pattern.empty() || pattern == "*") {
        return true;
    }

    const auto wildcardPos = pattern.find('*');
    if (wildcardPos == std::string::npos) {
        return value == pattern;
    }

    const auto prefix = pattern.substr(0, wildcardPos);
    if (value.find(prefix) != 0) {
        return false;
    }

    const auto suffix = pattern.substr(wildcardPos + 1);
    if (suffix.empty()) {
        return true;
    }

    if (value.size() < prefix.size() + suffix.size()) {
        return false;
    }

    return value.rfind(suffix) == value.size() - suffix.size();
}

} // anonymous namespace

// ── UUID 生成器（用于锁 token）───────────────────────────────────────────────
static std::string generateUuid() {
    static std::mt19937_64 rng{static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count())};
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::uppercase;
    oss << std::setw(8) << rng() << "-"
        << std::setw(4) << (rng() & 0xFFFF) << "-"
        << std::setw(4) << ((rng() & 0x0FFF) | 0x4000) << "-"  // version 4
        << std::setw(4) << ((rng() & 0x3FFF) | 0x8000) << "-"  // variant
        << std::setw(12) << rng();
    return oss.str();
}

// ── DistributedLock ────────────────────────────────────────────────────────

DistributedLock::DistributedLock(const std::string& key, int timeoutMs)
    : key_("lock:" + key)
    , token_(generateUuid())   // 全局唯一持有者标识
    , timeoutMs_(timeoutMs) {
}

DistributedLock::~DistributedLock() {
    if (acquired_) {
        release();
    }
}

bool DistributedLock::tryAcquire() {
    // Redis SET NX PX（原子加锁）：SET lock:<key> <uuid> NX PX <timeoutMs>
    std::ostringstream cmd;
    cmd << "SETNX " << key_ << " " << token_ << " " << timeoutMs_;
    std::string result = RedisCluster::instance().executeRaw(cmd.str());

    if (result == "1") {
        acquired_ = true;
        LOG_DEBUG << "[DistLock] Acquired lock: " << key_ << " token=" << token_;
        return true;
    }

    LOG_DEBUG << "[DistLock] Failed to acquire lock (already held): " << key_;
    return false;
}

void DistributedLock::release() {
    if (!acquired_) {
        return;
    }

    // Lua 等效脚本：只删除自己持有的 token，防止误删他人锁
    // EVAL "release-lock" 1 <key> <token>
    std::ostringstream cmd;
    cmd << "EVAL release-lock 1 " << key_ << " " << token_;
    std::string result = RedisCluster::instance().executeRaw(cmd.str());

    acquired_ = false;
    if (result == "1") {
        LOG_DEBUG << "[DistLock] Released lock: " << key_;
    } else {
        LOG_WARN << "[DistLock] Failed to release lock (token mismatch or expired): "
                 << key_ << " token=" << token_ << " result=" << result;
    }
}

// ── CacheStrategy ───────────────────────────────────────────────────────────

CacheStrategy& CacheStrategy::instance() {
    static CacheStrategy instance;
    return instance;
}

void CacheStrategy::initialize(const CacheConfig& config) {
    config_ = config;

    ClusterConfig clusterConfig = config_.redisCluster;
    clusterConfig.enabled = config_.enabled && clusterConfig.enabled;
    if (clusterConfig.enabled && clusterConfig.nodes.empty()) {
        clusterConfig.nodes.push_back(RedisNode{"127.0.0.1", 6379, "", 0, true, "local"});
    }
    RedisCluster::instance().init(clusterConfig);
}

void CacheStrategy::shutdown() {
    clear();
    RedisCluster::instance().shutdown();
}

void CacheStrategy::warmup() {
    CacheWarmup::instance().runAsync();
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

    const bool isNullValue = std::holds_alternative<std::monostate>(value);
    const int effectiveTtl = resolveTtl(ttlSeconds, isNullValue);

    CacheEntry entry;
    entry.value = value;
    entry.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(effectiveTtl);
    entry.isNull = isNullValue;

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
    if (auto entry = get(key)) {
        return *entry;
    }

    auto lock = acquireLock(key, config_.lockTimeoutMs);

    if (auto entry = get(key)) {
        return *entry;
    }

    CacheValue value;
    try {
        value = loader();
    } catch (...) {
        value = std::monostate{};
    }

    const bool isNullValue = std::holds_alternative<std::monostate>(value);
    const int effectiveTtl = resolveTtl(ttlSeconds, isNullValue);

    CacheEntry entry;
    entry.value = value;
    entry.isNull = isNullValue;
    entry.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(effectiveTtl);

    if (isNullValue && !config_.nullValueCaching) {
        return entry;
    }

    setToRedis(key, entry);
    setToLocal(key, entry);
    stats_.sets.fetch_add(1, std::memory_order_relaxed);
    return entry;
}

CacheEntry CacheStrategy::getWithLock(const std::string& key,
                                      const std::function<CacheValue()>& loader,
                                      int ttlSeconds) {
    return getOrSet(key, loader, ttlSeconds);
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

void CacheStrategy::warmupHotKeys(const std::vector<std::string>& keys, int ttlSeconds) {
    std::vector<WarmupTask> tasks;
    tasks.reserve(keys.size());

    for (const auto& key : keys) {
        tasks.push_back(WarmupTask{
            key,
            [key]() {
                const auto entry = CacheStrategy::instance().get(key);
                return entry ? entry->value : CacheValue{std::monostate{}};
            },
            ttlSeconds,
        });
    }

    CacheWarmup::instance().addTasks(tasks);
    CacheWarmup::instance().runAsync();
}

size_t CacheStrategy::removeByPattern(const std::string& pattern) {
    size_t localCount = 0;

    {
        std::unique_lock lock(localMutex_);
        for (auto it = localCache_.begin(); it != localCache_.end(); ) {
            if (matchesPattern(it->first, pattern)) {
                it = localCache_.erase(it);
                ++localCount;
            } else {
                ++it;
            }
        }
    }

    const auto redisCount = static_cast<size_t>(std::max<long long>(0, RedisCluster::instance().delByPattern(pattern)));
    return std::max(localCount, redisCount);
}

void CacheStrategy::clear() {
    {
        std::unique_lock lock(localMutex_);
        localCache_.clear();
    }

    const auto keys = RedisCluster::instance().scan("*", 1000000);
    if (!keys.empty()) {
        RedisCluster::instance().mdel(keys);
    }
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
    const auto rawValue = RedisCluster::instance().get(key);
    return deserializeCacheValue(rawValue);
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
    const auto serialized = serializeCacheValue(entry);
    const auto ttlSeconds = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
        entry.expiresAt - std::chrono::steady_clock::now()).count());
    return RedisCluster::instance().set(key, serialized, std::max(ttlSeconds, 1));
}

bool CacheStrategy::removeFromLocal(const std::string& key) {
    std::unique_lock lock(localMutex_);
    localCache_.erase(key);
    return true;
}

bool CacheStrategy::removeFromRedis(const std::string& key) {
    return RedisCluster::instance().del(key);
}

int CacheStrategy::randomizeTtl(int baseTtl) const {
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dis(0, config_.randomExpiryJitterSeconds);
    return baseTtl + dis(gen);
}

int CacheStrategy::resolveTtl(int ttlSeconds, bool isNullValue) const {
    int effectiveTtl = ttlSeconds > 0
        ? ttlSeconds
        : (isNullValue ? config_.nullValueTtlSeconds : config_.redisTtlSeconds);

    if (config_.enableRandomExpiryJitter && effectiveTtl > 0) {
        effectiveTtl = randomizeTtl(effectiveTtl);
    }

    return effectiveTtl > 0 ? effectiveTtl : 1;
}

} // namespace Cache
