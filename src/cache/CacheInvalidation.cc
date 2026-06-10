/**
 * @file CacheInvalidation.cc
 * @brief 缓存失效策略实现
 * 
 * 实现了缓存失效的核心逻辑，包括：
 *   - 单键失效
 *   - 模式匹配失效
 *   - 级联失效
 *   - 失效规则管理
 *   - 失效事件追踪
 */

#include "CacheInvalidation.h"

#include "CacheStrategy.h"
#include "RedisCluster.h"

#include <chrono>

namespace Cache {
namespace {

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 当前时间戳
 */
int64_t currentTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/**
 * @brief 检查值是否匹配模式
 * 
 * 支持通配符 * 匹配：
 *   - "*" 匹配所有
 *   - "prefix*" 匹配以 prefix 开头的
 *   - "*suffix" 匹配以 suffix 结尾的
 *   - "prefix*suffix" 匹配以 prefix 开头且以 suffix 结尾的
 * 
 * @param value 要检查的值
 * @param pattern 模式字符串
 * @return true 如果匹配，false 否则
 */
bool matchesPattern(const std::string& value, const std::string& pattern) {
    // 空模式或 * 匹配所有
    if (pattern.empty() || pattern == "*") {
        return true;
    }

    // 查找通配符位置
    const auto wildcardPos = pattern.find('*');
    if (wildcardPos == std::string::npos) {
        // 没有通配符，精确匹配
        return value == pattern;
    }

    // 提取前缀和后缀
    const auto prefix = pattern.substr(0, wildcardPos);
    if (value.find(prefix) != 0) {
        // 前缀不匹配
        return false;
    }

    const auto suffix = pattern.substr(wildcardPos + 1);
    if (suffix.empty()) {
        // 没有后缀，只需前缀匹配
        return true;
    }

    // 检查长度是否足够
    if (value.size() < prefix.size() + suffix.size()) {
        return false;
    }

    // 检查后缀是否匹配
    return value.rfind(suffix) == value.size() - suffix.size();
}

} // namespace

/**
 * @brief 获取单例实例
 * @return CacheInvalidation 单例引用
 */
CacheInvalidation& CacheInvalidation::instance() {
    static CacheInvalidation invalidation;
    return invalidation;
}

/**
 * @brief 初始化失效管理器
 * 
 * 流程：
 *   1. 加锁保护共享数据
 *   2. 保存失效规则
 *   3. 清空事件和依赖图
 *   4. 重置计数器
 *   5. 构建依赖关系图
 * 
 * @param rules 失效规则列表
 */
void CacheInvalidation::init(const std::vector<InvalidationRule>& rules) {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 保存失效规则
    rules_ = rules;
    
    // 清空事件和依赖图
    recentEvents_.clear();
    dependencyGraph_.clear();
    
    // 重置计数器
    totalInvalidations_ = 0;
    totalPatternInvalidations_ = 0;

    // 构建依赖关系图（用于级联失效）
    for (const auto& rule : rules_) {
        if (!rule.keyPattern.empty() && !rule.relatedPattern.empty()) {
            dependencyGraph_[rule.keyPattern].push_back(rule.relatedPattern);
        }
    }
}

/**
 * @brief 关闭失效管理器
 * 
 * 清空所有数据和状态。
 */
void CacheInvalidation::shutdown() {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 清空所有数据
    rules_.clear();
    recentEvents_.clear();
    dependencyGraph_.clear();
    totalInvalidations_ = 0;
    totalPatternInvalidations_ = 0;
}

/**
 * @brief 失效指定的缓存键
 * 
 * 流程：
 *   1. 从 L1（本地）缓存删除
 *   2. 从 L2（Redis）缓存删除
 *   3. 增加失效计数
 *   4. 记录失效事件
 *   5. 维护事件历史（最多 1000 条）
 * 
 * @param key 缓存键
 * @param reason 失效原因（默认 "manual"）
 * @param source 失效来源（默认空）
 */
void CacheInvalidation::invalidate(const std::string& key,
                                   const std::string& reason,
                                   const std::string& source) {
    // 从 L1（本地）缓存删除
    CacheStrategy::instance().remove(key);
    
    // 从 L2（Redis）缓存删除
    RedisCluster::instance().del(key);
    
    // 增加失效计数（使用原子操作，无需加锁）
    totalInvalidations_.fetch_add(1, std::memory_order_relaxed);

    // 创建失效事件
    InvalidationEvent event;
    event.key = key;
    event.reason = reason;
    event.source = source;
    event.timestamp = currentTimestampMs();

    // 记录事件
    std::lock_guard<std::mutex> lock(mutex_);
    recentEvents_.push_back(event);
    
    // 维护事件历史（最多 1000 条，超出时删除前 500 条）
    if (recentEvents_.size() > 1000) {
        recentEvents_.erase(recentEvents_.begin(), recentEvents_.begin() + 500);
    }
}

/**
 * @brief 失效匹配模式的所有缓存键
 * 
 * 流程：
 *   1. 扫描匹配模式的所有键
 *   2. 删除匹配的键（L1 和 L2）
 *   3. 增加模式匹配失效计数
 *   4. 记录失效事件
 *   5. 维护事件历史
 * 
 * @param pattern 键模式（支持通配符 *）
 * @param reason 失效原因（默认 "pattern_match"）
 * @param source 失效来源（默认空）
 */
void CacheInvalidation::invalidatePattern(const std::string& pattern,
                                          const std::string& reason,
                                          const std::string& source) {
    // 扫描匹配模式的所有键
    const auto keys = RedisCluster::instance().scan(pattern);
    
    // 删除匹配的键
    for (const auto& key : keys) {
        // 从 L1（本地）缓存删除
        CacheStrategy::instance().remove(key);
        // 从 L2（Redis）缓存删除
        RedisCluster::instance().del(key);
    }

    // 增加模式匹配失效计数
    totalPatternInvalidations_.fetch_add(1, std::memory_order_relaxed);

    // 创建失效事件
    InvalidationEvent event;
    event.key = pattern;
    event.reason = reason;
    event.source = source;
    event.timestamp = currentTimestampMs();

    // 记录事件
    std::lock_guard<std::mutex> lock(mutex_);
    recentEvents_.push_back(event);
    
    // 维护事件历史
    if (recentEvents_.size() > 1000) {
        recentEvents_.erase(recentEvents_.begin(), recentEvents_.begin() + 500);
    }
}

/**
 * @brief 失效相关的缓存键（级联失效）
 * 
 * 根据依赖关系图，失效与指定键相关的所有缓存键。
 * 
 * 流程：
 *   1. 遍历依赖关系图
 *   2. 查找与指定键匹配的模式
 *   3. 收集所有相关的失效模式
 *   4. 对每个相关模式执行失效操作
 * 
 * @param key 主缓存键
 */
void CacheInvalidation::invalidateRelated(const std::string& key) {
    // 收集相关的失效模式
    std::vector<std::string> relatedPatterns;

    {
        // 加锁保护依赖关系图
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 遍历依赖关系图，查找与指定键匹配的模式
        for (const auto& [pattern, relations] : dependencyGraph_) {
            if (matchesPattern(key, pattern)) {
                // 收集所有相关的失效模式
                relatedPatterns.insert(relatedPatterns.end(), relations.begin(), relations.end());
            }
        }
    }

    // 对每个相关模式执行失效操作
    for (const auto& pattern : relatedPatterns) {
        invalidatePattern(pattern, "related_invalidation", key);
    }
}

/**
 * @brief 注册新的失效规则
 * 
 * 添加一个新的失效规则到规则列表，并更新依赖关系图。
 * 
 * @param rule 失效规则
 */
void CacheInvalidation::registerRule(const InvalidationRule& rule) {
    // 加锁保护规则列表和依赖关系图
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 添加规则到列表
    rules_.push_back(rule);
    
    // 更新依赖关系图
    if (!rule.keyPattern.empty() && !rule.relatedPattern.empty()) {
        dependencyGraph_[rule.keyPattern].push_back(rule.relatedPattern);
    }
}

/**
 * @brief 获取最近的失效事件
 * 
 * 返回最近发生的失效事件，按时间顺序排列。
 * 
 * @param count 返回的事件数量（默认 100）
 * @return 失效事件列表（最多 count 条，按时间顺序）
 */
std::vector<InvalidationEvent> CacheInvalidation::getRecentEvents(int count) const {
    // 加锁保护事件列表
    std::lock_guard<std::mutex> lock(mutex_);

    // 如果数量无效或事件列表为空，返回空列表
    if (count <= 0 || recentEvents_.empty()) {
        return {};
    }

    // 计算起始位置
    const auto size = static_cast<int>(recentEvents_.size());
    const auto start = count >= size ? 0 : size - count;
    
    // 返回最近的 count 条事件
    return std::vector<InvalidationEvent>(recentEvents_.begin() + start, recentEvents_.end());
}

/**
 * @brief 清空失效事件历史
 * 
 * 删除所有记录的失效事件。
 */
void CacheInvalidation::clearEvents() {
    // 加锁保护事件列表
    std::lock_guard<std::mutex> lock(mutex_);
    // 清空事件列表
    recentEvents_.clear();
}

/**
 * @brief 获取当前时间字符串
 * 
 * 返回当前时间戳的字符串表示。
 * 
 * @return 时间戳字符串（毫秒）
 */
std::string CacheInvalidation::nowString() {
    return std::to_string(currentTimestampMs());
}

} // namespace Cache
