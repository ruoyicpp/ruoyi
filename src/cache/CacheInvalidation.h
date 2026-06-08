#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Cache {

enum class InvalidationStrategy {
    Passive,
    Active,
    Delayed,
    WriteBehind
};

struct InvalidationRule {
    std::string keyPattern;
    std::string relatedPattern;
    InvalidationStrategy strategy = InvalidationStrategy::Active;
    int delayMs = 0;
    int ttlSeconds = 0;
};

struct InvalidationEvent {
    std::string key;
    std::string reason;
    std::string source;
    int64_t timestamp = 0;
};

class CacheInvalidation {
public:
    static CacheInvalidation& instance();

    void init(const std::vector<InvalidationRule>& rules);
    void shutdown();

    void invalidate(const std::string& key,
                    const std::string& reason = "manual",
                    const std::string& source = "");

    void invalidatePattern(const std::string& pattern,
                          const std::string& reason = "pattern_match",
                          const std::string& source = "");

    void invalidateRelated(const std::string& key);

    void registerRule(const InvalidationRule& rule);

    std::vector<InvalidationEvent> getRecentEvents(int count = 100) const;
    void clearEvents();

    int64_t totalInvalidations() const { return totalInvalidations_.load(); }
    int64_t totalPatternInvalidations() const { return totalPatternInvalidations_.load(); }

private:
    CacheInvalidation() = default;
    ~CacheInvalidation() = default;
    CacheInvalidation(const CacheInvalidation&) = delete;
    CacheInvalidation& operator=(const CacheInvalidation&) = delete;

    std::string nowString();

    mutable std::mutex mutex_;
    std::vector<InvalidationRule> rules_;
    std::vector<InvalidationEvent> recentEvents_;
    std::atomic<int64_t> totalInvalidations_{0};
    std::atomic<int64_t> totalPatternInvalidations_{0};
    std::map<std::string, std::vector<std::string>> dependencyGraph_;
};

} // namespace Cache
