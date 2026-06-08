#include "CacheInvalidation.h"

#include "CacheStrategy.h"
#include "RedisCluster.h"

#include <chrono>

namespace Cache {
namespace {

int64_t currentTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
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

} // namespace

CacheInvalidation& CacheInvalidation::instance() {
    static CacheInvalidation invalidation;
    return invalidation;
}

void CacheInvalidation::init(const std::vector<InvalidationRule>& rules) {
    std::lock_guard<std::mutex> lock(mutex_);
    rules_ = rules;
    recentEvents_.clear();
    dependencyGraph_.clear();
    totalInvalidations_ = 0;
    totalPatternInvalidations_ = 0;

    for (const auto& rule : rules_) {
        if (!rule.keyPattern.empty() && !rule.relatedPattern.empty()) {
            dependencyGraph_[rule.keyPattern].push_back(rule.relatedPattern);
        }
    }
}

void CacheInvalidation::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    rules_.clear();
    recentEvents_.clear();
    dependencyGraph_.clear();
    totalInvalidations_ = 0;
    totalPatternInvalidations_ = 0;
}

void CacheInvalidation::invalidate(const std::string& key,
                                   const std::string& reason,
                                   const std::string& source) {
    CacheStrategy::instance().remove(key);
    RedisCluster::instance().del(key);
    totalInvalidations_.fetch_add(1, std::memory_order_relaxed);

    InvalidationEvent event;
    event.key = key;
    event.reason = reason;
    event.source = source;
    event.timestamp = currentTimestampMs();

    std::lock_guard<std::mutex> lock(mutex_);
    recentEvents_.push_back(event);
    if (recentEvents_.size() > 1000) {
        recentEvents_.erase(recentEvents_.begin(), recentEvents_.begin() + 500);
    }
}

void CacheInvalidation::invalidatePattern(const std::string& pattern,
                                          const std::string& reason,
                                          const std::string& source) {
    const auto keys = RedisCluster::instance().scan(pattern);
    for (const auto& key : keys) {
        CacheStrategy::instance().remove(key);
        RedisCluster::instance().del(key);
    }

    totalPatternInvalidations_.fetch_add(1, std::memory_order_relaxed);

    InvalidationEvent event;
    event.key = pattern;
    event.reason = reason;
    event.source = source;
    event.timestamp = currentTimestampMs();

    std::lock_guard<std::mutex> lock(mutex_);
    recentEvents_.push_back(event);
    if (recentEvents_.size() > 1000) {
        recentEvents_.erase(recentEvents_.begin(), recentEvents_.begin() + 500);
    }
}

void CacheInvalidation::invalidateRelated(const std::string& key) {
    std::vector<std::string> relatedPatterns;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [pattern, relations] : dependencyGraph_) {
            if (matchesPattern(key, pattern)) {
                relatedPatterns.insert(relatedPatterns.end(), relations.begin(), relations.end());
            }
        }
    }

    for (const auto& pattern : relatedPatterns) {
        invalidatePattern(pattern, "related_invalidation", key);
    }
}

void CacheInvalidation::registerRule(const InvalidationRule& rule) {
    std::lock_guard<std::mutex> lock(mutex_);
    rules_.push_back(rule);
    if (!rule.keyPattern.empty() && !rule.relatedPattern.empty()) {
        dependencyGraph_[rule.keyPattern].push_back(rule.relatedPattern);
    }
}

std::vector<InvalidationEvent> CacheInvalidation::getRecentEvents(int count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (count <= 0 || recentEvents_.empty()) {
        return {};
    }

    const auto size = static_cast<int>(recentEvents_.size());
    const auto start = count >= size ? 0 : size - count;
    return std::vector<InvalidationEvent>(recentEvents_.begin() + start, recentEvents_.end());
}

void CacheInvalidation::clearEvents() {
    std::lock_guard<std::mutex> lock(mutex_);
    recentEvents_.clear();
}

std::string CacheInvalidation::nowString() {
    return std::to_string(currentTimestampMs());
}

} // namespace Cache
