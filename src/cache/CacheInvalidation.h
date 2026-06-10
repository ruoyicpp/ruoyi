/**
 * @file CacheInvalidation.h
 * @brief 缓存失效策略 — 管理缓存键的失效和相关键的级联失效
 * 
 * 功能概述：
 *   - 单键失效：删除指定的缓存键
 *   - 模式匹配失效：删除匹配模式的所有缓存键
 *   - 级联失效：删除相关的缓存键（依赖关系）
 *   - 延迟失效：延迟一段时间后失效（避免数据不一致）
 *   - 失效事件追踪：记录所有失效操作
 * 
 * 失效策略：
 *   - Passive（被动）：只在查询时检查过期
 *   - Active（主动）：立即删除缓存
 *   - Delayed（延迟）：延迟一段时间后删除
 *   - WriteBehind（写回）：异步删除，不阻塞主线程
 * 
 * 使用场景：
 *   - 数据更新时失效相关缓存
 *   - 配置变更时失效配置缓存
 *   - 删除数据时失效相关缓存
 *   - 批量操作时级联失效
 * 
 * @see Cache::InvalidationStrategy - 失效策略枚举
 * @see Cache::InvalidationRule - 失效规则
 * @see Cache::InvalidationEvent - 失效事件
 */

#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

/**
 * @namespace Cache
 * @brief 缓存管理命名空间
 */
namespace Cache {

/**
 * @enum InvalidationStrategy
 * @brief 缓存失效策略
 */
enum class InvalidationStrategy {
    Passive,       ///< 被动失效：只在查询时检查过期
    Active,        ///< 主动失效：立即删除缓存
    Delayed,       ///< 延迟失效：延迟一段时间后删除
    WriteBehind    ///< 写回失效：异步删除，不阻塞主线程
};

/**
 * @struct InvalidationRule
 * @brief 缓存失效规则
 * 
 * 定义缓存失效的规则，包括键模式、相关键模式、失效策略等。
 */
struct InvalidationRule {
    std::string keyPattern;                        ///< 主键模式（支持通配符 *）
    std::string relatedPattern;                    ///< 相关键模式（级联失效）
    InvalidationStrategy strategy = InvalidationStrategy::Active;  ///< 失效策略
    int delayMs = 0;                               ///< 延迟时间（毫秒）
    int ttlSeconds = 0;                            ///< 缓存 TTL（秒）
};

/**
 * @struct InvalidationEvent
 * @brief 缓存失效事件
 * 
 * 记录一次缓存失效操作的详细信息。
 */
struct InvalidationEvent {
    std::string key;                               ///< 被失效的缓存键
    std::string reason;                            ///< 失效原因（如 "manual"、"pattern_match"）
    std::string source;                            ///< 失效来源（如 "UserService.update"）
    int64_t timestamp = 0;                         ///< 失效时间戳
};

/**
 * @class CacheInvalidation
 * @brief 缓存失效管理器
 * 
 * 单例模式，管理缓存失效的执行。支持：
 *   - 单键失效
 *   - 模式匹配失效
 *   - 级联失效
 *   - 多种失效策略
 *   - 失效事件追踪
 */
class CacheInvalidation {
public:
    /**
     * @brief 获取单例实例
     * @return CacheInvalidation 单例引用
     */
    static CacheInvalidation& instance();

    /**
     * @brief 初始化失效管理器
     * @param rules 失效规则列表
     */
    void init(const std::vector<InvalidationRule>& rules);

    /**
     * @brief 关闭失效管理器
     */
    void shutdown();

    /**
     * @brief 失效指定的缓存键
     * @param key 缓存键
     * @param reason 失效原因（默认 "manual"）
     * @param source 失效来源（默认空）
     */
    void invalidate(const std::string& key,
                    const std::string& reason = "manual",
                    const std::string& source = "");

    /**
     * @brief 失效匹配模式的所有缓存键
     * @param pattern 键模式（支持通配符 *）
     * @param reason 失效原因（默认 "pattern_match"）
     * @param source 失效来源（默认空）
     */
    void invalidatePattern(const std::string& pattern,
                          const std::string& reason = "pattern_match",
                          const std::string& source = "");

    /**
     * @brief 失效相关的缓存键（级联失效）
     * 
     * 根据失效规则，失效与指定键相关的所有缓存键。
     * 
     * @param key 主缓存键
     */
    void invalidateRelated(const std::string& key);

    /**
     * @brief 注册新的失效规则
     * @param rule 失效规则
     */
    void registerRule(const InvalidationRule& rule);

    /**
     * @brief 获取最近的失效事件
     * @param count 返回的事件数量（默认 100）
     * @return 失效事件列表
     */
    std::vector<InvalidationEvent> getRecentEvents(int count = 100) const;

    /**
     * @brief 清空失效事件历史
     */
    void clearEvents();

    /**
     * @brief 获取总失效次数
     * @return 失效次数
     */
    int64_t totalInvalidations() const { return totalInvalidations_.load(); }

    /**
     * @brief 获取总模式匹配失效次数
     * @return 模式匹配失效次数
     */
    int64_t totalPatternInvalidations() const { return totalPatternInvalidations_.load(); }

private:
    CacheInvalidation() = default;
    ~CacheInvalidation() = default;
    CacheInvalidation(const CacheInvalidation&) = delete;
    CacheInvalidation& operator=(const CacheInvalidation&) = delete;

    /**
     * @brief 获取当前时间字符串
     * @return 时间字符串
     */
    std::string nowString();

    mutable std::mutex mutex_;                     ///< 互斥锁
    std::vector<InvalidationRule> rules_;          ///< 失效规则列表
    std::vector<InvalidationEvent> recentEvents_;  ///< 最近的失效事件
    std::atomic<int64_t> totalInvalidations_{0};   ///< 总失效次数
    std::atomic<int64_t> totalPatternInvalidations_{0};  ///< 总模式匹配失效次数
    std::map<std::string, std::vector<std::string>> dependencyGraph_;  ///< 依赖关系图
};

} // namespace Cache
