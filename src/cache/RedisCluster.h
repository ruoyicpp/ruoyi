/**
 * @file RedisCluster.h
 * @brief Redis 集群管理 — 支持单机和集群模式的 Redis 客户端
 * 
 * 功能概述：
 *   - 单机和集群模式支持
 *   - 自动故障转移和重连
 *   - 连接池管理
 *   - 哈希槽路由
 *   - 批量操作支持
 *   - 模式匹配扫描
 * 
 * 特性：
 *   - 自动发现集群拓扑
 *   - 连接池优化（最小空闲连接、最大连接数）
 *   - 自动重连机制
 *   - 超时控制（连接超时、命令超时）
 *   - 本地备份存储（Redis 不可用时）
 * 
 * 使用场景：
 *   - 分布式缓存存储
 *   - 会话管理
 *   - 消息队列
 *   - 实时计数器
 * 
 * @see Cache::RedisNode - Redis 节点配置
 * @see Cache::ClusterConfig - 集群配置
 */

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * @namespace Cache
 * @brief 缓存管理命名空间
 */
namespace Cache {

/**
 * @struct RedisNode
 * @brief Redis 节点配置
 * 
 * 表示 Redis 集群中的一个节点。
 */
struct RedisNode {
    std::string host;                              ///< 节点主机地址
    int port = 6379;                              ///< 节点端口（默认 6379）
    std::string password;                          ///< 连接密码
    int db = 0;                                    ///< 数据库编号（0-15）
    bool master = true;                            ///< 是否为主节点
    std::string nodeId;                            ///< 节点 ID（集群模式）
};

/**
 * @struct ClusterConfig
 * @brief Redis 集群配置
 * 
 * 配置 Redis 集群的各项参数，包括节点列表、连接池、超时等。
 */
struct ClusterConfig {
    bool enabled = false;                          ///< 是否启用 Redis 集群
    std::vector<RedisNode> nodes;                  ///< 节点列表
    int connectionTimeoutMs = 5000;                ///< 连接超时（毫秒）
    int commandTimeoutMs = 3000;                   ///< 命令超时（毫秒）
    int maxConnectionsPerNode = 8;                 ///< 每个节点最大连接数
    int minIdleConnections = 2;                    ///< 最小空闲连接数
    int maxQueueSize = 1024;                       ///< 最大命令队列大小
    bool autoReconnect = true;                     ///< 是否自动重连
    int reconnectIntervalSeconds = 3;              ///< 重连间隔（秒）
};

/**
 * @class RedisCluster
 * @brief Redis 集群客户端
 * 
 * 单例模式，管理 Redis 集群的连接和操作。支持：
 *   - 单机和集群模式
 *   - 自动故障转移
 *   - 连接池管理
 *   - 批量操作
 *   - 模式匹配扫描
 */
class RedisCluster {
public:
    /**
     * @brief 获取单例实例
     * @return RedisCluster 单例引用
     */
    static RedisCluster& instance();

    /**
     * @brief 初始化 Redis 集群
     * @param config 集群配置
     */
    void init(const ClusterConfig& config);

    /**
     * @brief 关闭 Redis 集群连接
     */
    void shutdown();

    /**
     * @brief 检查是否已连接
     * @return true 如果已连接，false 否则
     */
    bool isConnected() const { return connected_.load(); }

    /**
     * @brief 获取集群节点数
     * @return 节点数量
     */
    int nodeCount() const;

    /**
     * @brief 获取缓存值
     * @param key 缓存键
     * @return 缓存值（如果不存在返回空字符串）
     */
    std::string get(const std::string& key);

    /**
     * @brief 设置缓存值
     * @param key 缓存键
     * @param value 缓存值
     * @param ttlSeconds 过期时间（秒，0 表示永不过期）
     * @return 是否设置成功
     */
    bool set(const std::string& key, const std::string& value, int ttlSeconds = 0);

    /**
     * @brief 删除缓存键
     * @param key 缓存键
     * @return 是否删除成功
     */
    bool del(const std::string& key);

    /**
     * @brief 检查缓存键是否存在
     * @param key 缓存键
     * @return true 如果存在，false 否则
     */
    bool exists(const std::string& key);

    /**
     * @brief 批量获取缓存值
     * @param keys 缓存键列表
     * @return 缓存值列表（顺序与输入相同）
     */
    std::vector<std::string> mget(const std::vector<std::string>& keys);

    /**
     * @brief 批量设置缓存值
     * @param items 键值对映射
     * @param ttlSeconds 过期时间（秒）
     * @return 是否设置成功
     */
    bool mset(const std::map<std::string, std::string>& items, int ttlSeconds = 0);

    /**
     * @brief 批量删除缓存键
     * @param keys 缓存键列表
     * @return 是否删除成功
     */
    bool mdel(const std::vector<std::string>& keys);

    /**
     * @brief 设置缓存键的过期时间
     * @param key 缓存键
     * @param ttlSeconds 过期时间（秒）
     * @return 是否设置成功
     */
    bool setExpire(const std::string& key, int ttlSeconds);

    /**
     * @brief 获取缓存键的剩余 TTL
     * @param key 缓存键
     * @return 剩余 TTL（秒），-1 表示永不过期，-2 表示键不存在
     */
    long long ttl(const std::string& key);

    /**
     * @brief 扫描匹配模式的缓存键
     * @param pattern 键模式（支持通配符 *）
     * @param count 返回的键数量（默认 100）
     * @return 匹配的键列表
     */
    std::vector<std::string> scan(const std::string& pattern, int count = 100);

    /**
     * @brief 删除匹配模式的所有缓存键
     * @param pattern 键模式（支持通配符 *）
     * @return 删除的键数量
     */
    long long delByPattern(const std::string& pattern);

    /**
     * @brief 检查 Redis 连接是否正常
     * @return 如果连接正常返回 "PONG"，否则返回错误信息
     */
    std::string ping();

    /**
     * @brief 获取所有集群节点
     * @return 节点映射（节点 ID → 节点配置）
     */
    std::map<std::string, RedisNode> getNodes() const;

    /**
     * @brief 执行原始 Redis 命令
     * @param command Redis 命令字符串
     * @return 命令执行结果
     */
    std::string executeRaw(const std::string& command);

private:
    RedisCluster() = default;
    ~RedisCluster() = default;
    RedisCluster(const RedisCluster&) = delete;
    RedisCluster& operator=(const RedisCluster&) = delete;

    /**
     * @brief 根据键路由到对应节点
     * @param key 缓存键
     * @return 目标节点 ID
     */
    std::string routeToNode(const std::string& key);

    /**
     * @brief 连接到指定节点
     * @param node 节点配置
     * @return 连接 ID
     */
    std::string connectNode(const RedisNode& node);

    /**
     * @brief 自动发现集群拓扑
     */
    void discoverCluster();

    mutable std::mutex mutex_;                     ///< 互斥锁
    ClusterConfig config_;                         ///< 集群配置
    std::map<std::string, RedisNode> nodes_;       ///< 节点映射
    std::atomic<bool> connected_{false};           ///< 连接状态
    std::map<std::string, std::string> localStore_;  ///< 本地备份存储（Redis 不可用时）
    mutable std::mutex storeMutex_;                ///< 本地存储互斥锁
};

} // namespace Cache
