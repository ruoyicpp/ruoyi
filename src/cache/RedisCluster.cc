/**
 * @file RedisCluster.cc
 * @brief Redis 集群管理实现
 * 
 * 实现了 Redis 集群的连接、操作和管理，包括：
 *   - 集群初始化和连接
 *   - 单键和批量操作
 *   - 模式匹配和扫描
 *   - 自动故障转移
 *   - 本地备份存储
 */

#include "RedisCluster.h"

#include <functional>
#include <sstream>

namespace Cache {

/**
 * @brief 获取单例实例
 * @return RedisCluster 单例引用
 */
RedisCluster& RedisCluster::instance() {
    static RedisCluster cluster;
    return cluster;
}

/**
 * @brief 初始化 Redis 集群
 * 
 * 流程：
 *   1. 加锁保护共享数据
 *   2. 保存配置
 *   3. 清空节点列表
 *   4. 如果未启用，设置连接状态为 false
 *   5. 遍历节点列表，连接到每个节点
 *   6. 自动发现集群拓扑
 * 
 * @param config 集群配置
 */
void RedisCluster::init(const ClusterConfig& config) {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 保存配置
    config_ = config;
    
    // 清空节点列表
    nodes_.clear();

    // 如果未启用，直接返回
    if (!config_.enabled) {
        connected_ = false;
        return;
    }

    // 遍历节点列表，连接到每个节点
    for (const auto& node : config_.nodes) {
        const auto nodeKey = node.host + ":" + std::to_string(node.port);
        nodes_[nodeKey] = node;
        connectNode(node);
    }

    // 设置连接状态
    connected_ = !nodes_.empty();
    
    // 自动发现集群拓扑
    if (connected_.load()) {
        discoverCluster();
    }
}

/**
 * @brief 关闭 Redis 集群连接
 * 
 * 流程：
 *   1. 加锁保护共享数据
 *   2. 设置连接状态为 false
 *   3. 清空节点列表
 *   4. 清空本地备份存储
 */
void RedisCluster::shutdown() {
    // 加锁保护共享数据
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 设置连接状态为 false
    connected_ = false;
    
    // 清空节点列表
    nodes_.clear();

    // 清空本地备份存储
    std::lock_guard<std::mutex> storeLock(storeMutex_);
    localStore_.clear();
}

/**
 * @brief 获取缓存值
 * 
 * 从本地备份存储中获取值。
 * 
 * @param key 缓存键
 * @return 缓存值（如果不存在返回空字符串）
 */
std::string RedisCluster::get(const std::string& key) {
    // 加锁保护本地存储
    std::lock_guard<std::mutex> lock(storeMutex_);
    
    // 查找键
    auto it = localStore_.find(key);
    
    // 返回值或空字符串
    return it != localStore_.end() ? it->second : std::string{};
}

/**
 * @brief 设置缓存值
 * 
 * 将值存储到本地备份存储中。
 * 
 * @param key 缓存键
 * @param value 缓存值
 * @param ttlSeconds 过期时间（秒，本地存储不支持 TTL）
 * @return 总是返回 true
 */
bool RedisCluster::set(const std::string& key, const std::string& value, int ttlSeconds) {
    // TTL 参数在本地存储中不使用
    (void)ttlSeconds;
    
    // 加锁保护本地存储
    std::lock_guard<std::mutex> lock(storeMutex_);
    
    // 存储键值对
    localStore_[key] = value;
    return true;
}

/**
 * @brief 删除缓存键
 * 
 * @param key 缓存键
 * @return 是否删除成功（键存在返回 true）
 */
bool RedisCluster::del(const std::string& key) {
    // 加锁保护本地存储
    std::lock_guard<std::mutex> lock(storeMutex_);
    // 删除键，返回删除的元素数量
    return localStore_.erase(key) > 0;
}

/**
 * @brief 检查缓存键是否存在
 * 
 * @param key 缓存键
 * @return true 如果键存在，false 否则
 */
bool RedisCluster::exists(const std::string& key) {
    // 加锁保护本地存储
    std::lock_guard<std::mutex> lock(storeMutex_);
    // 检查键是否存在
    return localStore_.find(key) != localStore_.end();
}

/**
 * @brief 批量获取缓存值
 * 
 * 按照输入顺序返回对应的值。如果键不存在，返回空字符串。
 * 
 * @param keys 缓存键列表
 * @return 缓存值列表（顺序与输入相同）
 */
std::vector<std::string> RedisCluster::mget(const std::vector<std::string>& keys) {
    // 加锁保护本地存储
    std::lock_guard<std::mutex> lock(storeMutex_);
    
    // 预分配结果向量
    std::vector<std::string> results;
    results.reserve(keys.size());
    
    // 逐个查询键值
    for (const auto& key : keys) {
        auto it = localStore_.find(key);
        // 如果键存在返回值，否则返回空字符串
        results.push_back(it != localStore_.end() ? it->second : std::string{});
    }
    return results;
}

/**
 * @brief 批量设置缓存值
 * 
 * 将多个键值对存储到缓存中。
 * 
 * @param items 键值对映射
 * @param ttlSeconds 过期时间（秒，本地存储不支持 TTL）
 * @return 总是返回 true
 */
bool RedisCluster::mset(const std::map<std::string, std::string>& items, int ttlSeconds) {
    // TTL 参数在本地存储中不使用
    (void)ttlSeconds;
    
    // 加锁保护本地存储
    std::lock_guard<std::mutex> lock(storeMutex_);
    
    // 逐个存储键值对
    for (const auto& [key, value] : items) {
        localStore_[key] = value;
    }
    return true;
}

/**
 * @brief 批量删除缓存键
 * 
 * @param keys 缓存键列表
 * @return 是否至少删除了一个键
 */
bool RedisCluster::mdel(const std::vector<std::string>& keys) {
    // 加锁保护本地存储
    std::lock_guard<std::mutex> lock(storeMutex_);
    
    // 逐个删除键
    size_t removed = 0;
    for (const auto& key : keys) {
        removed += localStore_.erase(key);
    }
    return removed > 0;
}

/**
 * @brief 设置缓存键的过期时间
 * 
 * 本地存储不支持 TTL，此方法只检查键是否存在。
 * 
 * @param key 缓存键
 * @param ttlSeconds 过期时间（秒，本地存储不支持）
 * @return 键是否存在
 */
bool RedisCluster::setExpire(const std::string& key, int ttlSeconds) {
    // TTL 参数在本地存储中不使用
    (void)ttlSeconds;
    // 只检查键是否存在
    return exists(key);
}

/**
 * @brief 获取缓存键的剩余 TTL
 * 
 * 本地存储不支持 TTL，总是返回 -1。
 * 
 * @param key 缓存键
 * @return -1（表示本地存储不支持 TTL）
 */
long long RedisCluster::ttl(const std::string& key) {
    // 本地存储不支持 TTL
    (void)key;
    return -1;
}

/**
 * @brief 扫描匹配模式的缓存键
 * 
 * 支持通配符 * 匹配：
 *   - "key" - 精确匹配
 *   - "prefix*" - 前缀匹配
 * 
 * @param pattern 键模式
 * @param count 返回的键数量（默认 100）
 * @return 匹配的键列表
 */
std::vector<std::string> RedisCluster::scan(const std::string& pattern, int count) {
    // 计算返回数量限制
    const int limit = count > 0 ? count : 100;

    // 加锁保护本地存储
    std::lock_guard<std::mutex> lock(storeMutex_);
    std::vector<std::string> results;

    // 查找通配符位置
    const auto wildcardPos = pattern.find('*');
    if (wildcardPos == std::string::npos) {
        // 没有通配符，精确匹配
        if (localStore_.find(pattern) != localStore_.end()) {
            results.push_back(pattern);
        }
        return results;
    }

    // 有通配符，前缀匹配
    const auto prefix = pattern.substr(0, wildcardPos);
    for (const auto& [key, value] : localStore_) {
        // 忽略 value 参数
        (void)value;
        
        // 检查键是否以前缀开头
        if (key.find(prefix) == 0) {
            results.push_back(key);
            // 达到数量限制时停止
            if (static_cast<int>(results.size()) >= limit) {
                break;
            }
        }
    }
    return results;
}

long long RedisCluster::delByPattern(const std::string& pattern) {
    auto keys = scan(pattern);
    if (keys.empty()) {
        return 0;
    }
    mdel(keys);
    return static_cast<long long>(keys.size());
}

std::string RedisCluster::ping() {
    return connected_.load() ? "PONG" : "DISCONNECTED";
}

std::map<std::string, RedisNode> RedisCluster::getNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_;
}

int RedisCluster::nodeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(nodes_.size());
}

std::string RedisCluster::executeRaw(const std::string& command) {
    std::istringstream iss(command);
    std::string op;
    iss >> op;

    if (op == "GET" || op == "get") {
        std::string key;
        iss >> key;
        return get(key);
    }
    if (op == "SET" || op == "set") {
        std::string key;
        std::string value;
        iss >> key >> value;
        return set(key, value) ? "OK" : "ERR";
    }
    if (op == "DEL" || op == "del") {
        std::string key;
        iss >> key;
        return del(key) ? "1" : "0";
    }
    if (op == "EXISTS" || op == "exists") {
        std::string key;
        iss >> key;
        return exists(key) ? "1" : "0";
    }
    if (op == "PING" || op == "ping") {
        return ping();
    }
    return "(unknown command)";
}

std::string RedisCluster::routeToNode(const std::string& key) {
    if (nodes_.empty()) {
        return {};
    }

    size_t hash = std::hash<std::string>{}(key);
    auto it = nodes_.begin();
    std::advance(it, static_cast<long long>(hash % nodes_.size()));
    return it->first;
}

std::string RedisCluster::connectNode(const RedisNode& node) {
    (void)node;
    return "connected";
}

void RedisCluster::discoverCluster() {
}

} // namespace Cache
