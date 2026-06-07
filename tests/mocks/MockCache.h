/**
 * @file MockCache.h
 * @brief 缓存 Mock 对象
 */

#pragma once
#include <string>
#include <map>
#include <chrono>
#include <functional>

/**
 * @class MockCacheEntry
 * @brief Mock 缓存条目
 */
struct MockCacheEntry {
    std::string value;
    std::chrono::steady_clock::time_point expiresAt;
    bool isExpired() const {
        return std::chrono::steady_clock::now() > expiresAt;
    }
};

/**
 * @class MockCache
 * @brief Mock 缓存服务
 */
class MockCache {
public:
    static MockCache& instance() {
        static MockCache inst;
        return inst;
    }

    // 设置缓存（默认永不过期）
    void set(const std::string& key, const std::string& value) {
        cache_[key] = MockCacheEntry{value, std::chrono::steady_clock::time_point::max()};
    }

    // 设置缓存（带过期时间）
    void set(const std::string& key, const std::string& value, int ttlSeconds) {
        auto expires = std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds);
        cache_[key] = MockCacheEntry{value, expires};
    }

    // 获取缓存
    std::string get(const std::string& key) const {
        if (auto it = cache_.find(key); it != cache_.end()) {
            if (!it->second.isExpired()) {
                hitCount_++;
                return it->second.value;
            }
        }
        missCount_++;
        return "";
    }

    // 检查是否存在
    bool exists(const std::string& key) const {
        if (auto it = cache_.find(key); it != cache_.end()) {
            return !it->second.isExpired();
        }
        return false;
    }

    // 删除缓存
    bool remove(const std::string& key) {
        return cache_.erase(key) > 0;
    }

    // 按前缀删除
    size_t removeByPrefix(const std::string& prefix) {
        size_t count = 0;
        for (auto it = cache_.begin(); it != cache_.end(); ) {
            if (it->first.find(prefix) == 0) {
                it = cache_.erase(it);
                count++;
            } else {
                ++it;
            }
        }
        return count;
    }

    // 清空所有缓存
    void clear() {
        cache_.clear();
        hitCount_ = 0;
        missCount_ = 0;
    }

    // 获取统计信息
    size_t size() const {
        size_t count = 0;
        for (const auto& [key, entry] : cache_) {
            if (!entry.isExpired()) count++;
        }
        return count;
    }

    long long getHitCount() const { return hitCount_; }
    long long getMissCount() const { return missCount_; }

    double getHitRate() const {
        long long total = hitCount_ + missCount_;
        return total > 0 ? static_cast<double>(hitCount_) / total : 0.0;
    }

    // 批量操作
    void mset(const std::map<std::string, std::string>& items) {
        for (const auto& [key, value] : items) {
            set(key, value);
        }
    }

    std::map<std::string, std::string> mget(const std::vector<std::string>& keys) const {
        std::map<std::string, std::string> result;
        for (const auto& key : keys) {
            std::string val = get(key);
            if (!val.empty()) {
                result[key] = val;
            }
        }
        return result;
    }

private:
    std::map<std::string, MockCacheEntry> cache_;
    long long hitCount_ = 0;
    long long missCount_ = 0;
};
