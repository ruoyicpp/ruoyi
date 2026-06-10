/**
 * @file CacheCtrl.h
 * @brief 缓存管理和监控控制器 — 提供缓存统计、查询、清理、预热等功能
 * 
 * 功能概述：
 *   - 缓存统计：获取缓存命中率、大小、性能指标
 *   - 缓存查询：查询缓存键列表、键值详情
 *   - 缓存管理：删除指定键、清空缓存、缓存预热
 *   - 集群监控：查看 Redis 集群节点、槽位分配
 *   - 事件追踪：查看最近的缓存操作事件
 * 
 * API 路由列表：
 *   - GET /monitor/cache/stats - 获取缓存统计信息
 *   - GET /monitor/cache/keys - 获取缓存键列表
 *   - GET /monitor/cache/key/{key} - 获取指定键的值
 *   - DELETE /monitor/cache/key/{key} - 删除指定键
 *   - POST /monitor/cache/warmup - 缓存预热
 *   - POST /monitor/cache/clear - 清空所有缓存
 *   - GET /monitor/cache/cluster/nodes - 获取集群节点信息
 *   - GET /monitor/cache/cluster/slots - 获取集群槽位分配
 *   - GET /monitor/cache/events - 获取最近的缓存事件
 * 
 * 权限要求：
 *   - monitor:cache:view - 查看缓存信息
 *   - monitor:cache:manage - 管理缓存（删除、清空、预热）
 * 
 * @see Cache::CacheStrategy - 缓存策略实现
 * @see Cache::RedisCluster - Redis 集群管理
 * @see Cache::CacheWarmup - 缓存预热策略
 * @see Cache::CacheInvalidation - 缓存失效策略
 */

#pragma once

#include <drogon/HttpController.h>

/**
 * @namespace Monitor
 * @brief 监控模块命名空间
 */
namespace Monitor {

/**
 * @class CacheCtrl
 * @brief 缓存管理和监控控制器
 * 
 * 提供缓存系统的监控和管理功能，包括：
 *   - 缓存统计和性能指标
 *   - 缓存键值查询和管理
 *   - 缓存预热和清理
 *   - Redis 集群监控
 *   - 缓存操作事件追踪
 */
class CacheCtrl : public drogon::HttpController<CacheCtrl> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(CacheCtrl::getStats, "/monitor/cache/stats", drogon::Get);
    ADD_METHOD_TO(CacheCtrl::getKeys, "/monitor/cache/keys", drogon::Get);
    ADD_METHOD_TO(CacheCtrl::getKeyValue, "/monitor/cache/key/{key}", drogon::Get);
    ADD_METHOD_TO(CacheCtrl::deleteKey, "/monitor/cache/key/{key}", drogon::Delete);
    ADD_METHOD_TO(CacheCtrl::warmup, "/monitor/cache/warmup", drogon::Post);
    ADD_METHOD_TO(CacheCtrl::clearCache, "/monitor/cache/clear", drogon::Post);
    ADD_METHOD_TO(CacheCtrl::getClusterNodes, "/monitor/cache/cluster/nodes", drogon::Get);
    ADD_METHOD_TO(CacheCtrl::getClusterSlots, "/monitor/cache/cluster/slots", drogon::Get);
    ADD_METHOD_TO(CacheCtrl::getRecentEvents, "/monitor/cache/events", drogon::Get);
    METHOD_LIST_END

    /**
     * @brief 获取缓存统计信息
     * 
     * GET /monitor/cache/stats - 返回缓存系统的性能指标
     * 
     * 响应包含：
     *   - 命中次数、未命中次数、命中率
     *   - 设置次数、驱逐次数
     *   - 错误次数
     *   - L1（本地）和 L2（Redis）缓存大小
     *   - 平均响应时间
     */
    void getStats(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief 获取缓存键列表
     * 
     * GET /monitor/cache/keys - 返回所有缓存键的列表
     * 
     * 支持参数：
     *   - pattern: 键名模式（支持通配符 *）
     *   - limit: 返回数量限制（默认 100）
     */
    void getKeys(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief 获取指定键的值
     * 
     * GET /monitor/cache/key/{key} - 返回指定键的值和元数据
     * 
     * 返回信息：
     *   - 键值
     *   - 数据类型（string/json）
     *   - 过期时间
     *   - 大小（字节）
     *   - 访问次数
     */
    void getKeyValue(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& key);

    /**
     * @brief 删除指定键
     * 
     * DELETE /monitor/cache/key/{key} - 删除指定的缓存键
     * 
     * 删除范围：
     *   - 同时删除 L1（本地）和 L2（Redis）缓存
     *   - 支持通配符删除多个键
     */
    void deleteKey(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& key);

    /**
     * @brief 缓存预热
     * 
     * POST /monitor/cache/warmup - 执行缓存预热操作
     * 
     * 预热策略：
     *   - 加载常用数据到缓存
     *   - 预加载配置、字典、菜单等
     *   - 支持指定预热类型
     */
    void warmup(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief 清空所有缓存
     * 
     * POST /monitor/cache/clear - 清空 L1 和 L2 的所有缓存
     * 
     * 清空范围：
     *   - 本地内存缓存
     *   - Redis 缓存
     *   - 缓存统计信息
     */
    void clearCache(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief 获取 Redis 集群节点信息
     * 
     * GET /monitor/cache/cluster/nodes - 返回集群中所有节点的信息
     * 
     * 节点信息：
     *   - 节点 ID、地址、端口
     *   - 角色（master/slave）
     *   - 连接状态
     *   - 处理的键数量
     */
    void getClusterNodes(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief 获取 Redis 集群槽位分配
     * 
     * GET /monitor/cache/cluster/slots - 返回集群的槽位分配信息
     * 
     * 槽位信息：
     *   - 槽位范围
     *   - 主节点和从节点
     *   - 槽位状态
     */
    void getClusterSlots(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief 获取最近的缓存事件
     * 
     * GET /monitor/cache/events - 返回最近的缓存操作事件
     * 
     * 事件类型：
     *   - SET - 设置键值
     *   - GET - 获取键值
     *   - DEL - 删除键
     *   - EVICT - 驱逐键
     *   - EXPIRE - 键过期
     * 
     * 支持参数：
     *   - type: 事件类型过滤
     *   - limit: 返回数量限制（默认 100）
     */
    void getRecentEvents(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace Monitor
