/**
 * @file CacheCtrl.cc
 * @brief 缓存管理和监控控制器实现
 * 
 * 实现了缓存系统的监控和管理功能，包括：
 *   - 缓存统计信息查询
 *   - 缓存键值查询和管理
 *   - 缓存预热和清理
 *   - Redis 集群监控
 *   - 缓存操作事件追踪
 */

#include "CacheCtrl.h"

#include "CacheInvalidation.h"
#include "CacheStrategy.h"
#include "CacheWarmup.h"
#include "RedisCluster.h"

#include <drogon/HttpResponse.h>
#include <json/json.h>

#include <exception>
#include <string>
#include <variant>

namespace Monitor {
namespace {

/**
 * @brief 将缓存值转换为 JSON
 * 
 * 支持多种缓存值类型的转换：
 *   - std::string → JSON 字符串
 *   - Json::Value → JSON 对象
 *   - 其他 → 空 JSON
 * 
 * @param value 缓存值
 * @return JSON 表示
 */
Json::Value cacheValueToJson(const Cache::CacheValue& value) {
    if (std::holds_alternative<std::string>(value)) {
        return Json::Value(std::get<std::string>(value));
    }
    if (std::holds_alternative<Json::Value>(value)) {
        return std::get<Json::Value>(value);
    }
    return Json::Value();
}

/**
 * @brief 将缓存条目转换为 JSON
 * 
 * 包含缓存值、空值标记、过期状态等信息。
 * 
 * @param entry 缓存条目
 * @return JSON 表示
 */
Json::Value cacheEntryToJson(const Cache::CacheEntry& entry) {
    Json::Value json;
    json["value"] = cacheValueToJson(entry.value);
    json["isNull"] = entry.isNullValue();
    json["isExpired"] = entry.isExpired();
    return json;
}

/**
 * @brief 创建 JSON HTTP 响应
 * 
 * @param body 响应体
 * @param status HTTP 状态码（默认 200）
 * @return HTTP 响应对象
 */
drogon::HttpResponsePtr makeJsonResponse(const Json::Value& body,
                                         drogon::HttpStatusCode status = drogon::k200OK) {
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
}

/**
 * @brief 解析计数参数
 * 
 * 从字符串解析整数，如果解析失败或值无效则返回默认值。
 * 
 * @param value 参数值字符串
 * @param fallback 默认值
 * @return 解析的整数值
 */
int parseCountParam(const std::string& value, int fallback) {
    if (value.empty()) {
        return fallback;
    }

    try {
        const int parsed = std::stoi(value);
        return parsed > 0 ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

} // namespace

/**
 * @brief 获取缓存统计信息
 * 
 * GET /monitor/cache/stats
 * 
 * 返回缓存系统的性能指标：
 *   - 命中次数、未命中次数、命中率
 *   - 设置次数、驱逐次数、错误次数
 *   - Redis 连接状态和节点数
 *   - 预热状态和进度
 *   - 失效统计
 */
void CacheCtrl::getStats(const drogon::HttpRequestPtr&,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // 获取缓存统计信息
    const auto& stats = Cache::CacheStrategy::instance().stats();

    // 构建响应体
    Json::Value body;
    
    // 缓存命中统计
    body["hits"] = Json::UInt64(stats.hits.load());
    body["misses"] = Json::UInt64(stats.misses.load());
    body["sets"] = Json::UInt64(stats.sets.load());
    body["evictions"] = Json::UInt64(stats.evictions.load());
    body["errors"] = Json::UInt64(stats.errors.load());
    body["hitRate"] = stats.hitRate();
    
    // Redis 集群状态
    body["redisConnected"] = Cache::RedisCluster::instance().isConnected();
    body["redisNodeCount"] = Cache::RedisCluster::instance().nodeCount();
    
    // 缓存预热状态
    body["warmupRunning"] = Cache::CacheWarmup::instance().isRunning();
    body["warmupCompleted"] = Json::UInt64(Cache::CacheWarmup::instance().completedCount());
    body["warmupPending"] = Json::UInt64(Cache::CacheWarmup::instance().taskCount());
    body["warmupFailed"] = Json::UInt64(Cache::CacheWarmup::instance().failedCount());
    body["warmupProgress"] = Cache::CacheWarmup::instance().progress();
    
    // 缓存失效统计
    body["totalInvalidations"] = Json::Int64(Cache::CacheInvalidation::instance().totalInvalidations());
    body["totalPatternInvalidations"] = Json::Int64(
        Cache::CacheInvalidation::instance().totalPatternInvalidations());

    // 返回响应
    callback(makeJsonResponse(body));
}

/**
 * @brief 获取缓存键列表
 * 
 * GET /monitor/cache/keys?pattern=*&count=100
 * 
 * 参数：
 *   - pattern: 键模式（支持通配符 *，默认 "*"）
 *   - count: 返回数量（默认 100）
 * 
 * 返回：
 *   - pattern: 搜索模式
 *   - count: 返回的键数量
 *   - keys: 键列表
 */
void CacheCtrl::getKeys(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // 获取搜索模式（默认 "*"）
    const auto pattern = req->getParameter("pattern").empty()
        ? std::string("*")
        : req->getParameter("pattern");
    
    // 获取返回数量（默认 100）
    const int count = parseCountParam(req->getParameter("count"), 100);

    // 扫描匹配模式的键
    const auto keys = Cache::RedisCluster::instance().scan(pattern, count);

    // 构建响应体
    Json::Value body;
    body["pattern"] = pattern;
    body["count"] = Json::UInt64(keys.size());
    body["keys"] = Json::arrayValue;
    
    // 添加键到响应
    for (const auto& key : keys) {
        body["keys"].append(key);
    }

    // 返回响应
    callback(makeJsonResponse(body));
}

/**
 * @brief 获取指定键的值
 * 
 * GET /monitor/cache/key/{key}
 * 
 * 返回：
 *   - key: 缓存键
 *   - found: 是否找到
 *   - entry: 缓存条目（如果找到）
 *   - HTTP 状态码：200（找到）或 404（未找到）
 */
void CacheCtrl::getKeyValue(const drogon::HttpRequestPtr&,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& key) {
    // 从缓存中查询键
    auto entry = Cache::CacheStrategy::instance().get(key);

    // 构建响应体
    Json::Value body;
    body["key"] = key;
    body["found"] = static_cast<bool>(entry);
    
    // 如果找到，添加缓存条目
    if (entry) {
        body["entry"] = cacheEntryToJson(*entry);
    }

    // 返回响应（根据是否找到返回 200 或 404）
    callback(makeJsonResponse(body, entry ? drogon::k200OK : drogon::k404NotFound));
}

/**
 * @brief 删除指定的缓存键
 * 
 * DELETE /monitor/cache/key/{key}
 * 
 * 流程：
 *   1. 从缓存中删除键
 *   2. 如果删除成功，记录失效事件
 *   3. 返回删除结果
 * 
 * 返回：
 *   - key: 缓存键
 *   - removed: 是否删除成功
 */
void CacheCtrl::deleteKey(const drogon::HttpRequestPtr&,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          const std::string& key) {
    // 从缓存中删除键
    const bool removed = Cache::CacheStrategy::instance().remove(key);
    
    // 如果删除成功，记录失效事件
    if (removed) {
        Cache::CacheInvalidation::instance().invalidate(key, "api_delete", "CacheCtrl");
    }

    // 构建响应体
    Json::Value body;
    body["key"] = key;
    body["removed"] = removed;

    // 返回响应
    callback(makeJsonResponse(body));
}

/**
 * @brief 启动缓存预热
 * 
 * POST /monitor/cache/warmup
 * 
 * 异步启动缓存预热，返回预热状态。
 * 
 * 返回：
 *   - started: 是否启动成功
 *   - running: 是否正在运行
 *   - completedTasks: 已完成任务数
 *   - pendingTasks: 待执行任务数
 *   - failedTasks: 失败任务数
 *   - progress: 预热进度（0-1）
 */
void CacheCtrl::warmup(const drogon::HttpRequestPtr&,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // 异步启动缓存预热
    Cache::CacheWarmup::instance().runAsync();

    // 构建响应体
    Json::Value body;
    body["started"] = true;
    body["running"] = Cache::CacheWarmup::instance().isRunning();
    body["completedTasks"] = Json::UInt64(Cache::CacheWarmup::instance().completedCount());
    body["pendingTasks"] = Json::UInt64(Cache::CacheWarmup::instance().taskCount());
    body["failedTasks"] = Json::UInt64(Cache::CacheWarmup::instance().failedCount());
    body["progress"] = Cache::CacheWarmup::instance().progress();

    // 返回响应
    callback(makeJsonResponse(body));
}

/**
 * @brief 清空所有缓存
 * 
 * POST /monitor/cache/clear
 * 
 * 清空 L1（本地）和 L2（Redis）缓存中的所有数据。
 * 
 * 返回：
 *   - cleared: 是否清空成功
 */
void CacheCtrl::clearCache(const drogon::HttpRequestPtr&,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // 清空所有缓存
    Cache::CacheStrategy::instance().clear();

    // 构建响应体
    Json::Value body;
    body["cleared"] = true;

    // 返回响应
    callback(makeJsonResponse(body));
}

/**
 * @brief 获取 Redis 集群节点信息
 * 
 * GET /monitor/cache/cluster/nodes
 * 
 * 返回集群中所有节点的配置信息。
 */
void CacheCtrl::getClusterNodes(const drogon::HttpRequestPtr&,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // 获取所有集群节点
    const auto nodes = Cache::RedisCluster::instance().getNodes();

    Json::Value body;
    body["connected"] = Cache::RedisCluster::instance().isConnected();
    body["count"] = Json::UInt64(nodes.size());
    body["nodes"] = Json::arrayValue;

    for (const auto& [name, node] : nodes) {
        Json::Value item;
        item["name"] = name;
        item["host"] = node.host;
        item["port"] = node.port;
        item["db"] = node.db;
        item["master"] = node.master;
        item["nodeId"] = node.nodeId;
        body["nodes"].append(item);
    }

    callback(makeJsonResponse(body));
}

void CacheCtrl::getClusterSlots(const drogon::HttpRequestPtr&,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const int nodeCount = Cache::RedisCluster::instance().nodeCount();

    Json::Value body;
    body["totalSlots"] = 16384;
    body["nodeCount"] = nodeCount;
    body["slotsPerNode"] = nodeCount > 0 ? 16384 / nodeCount : 0;

    callback(makeJsonResponse(body));
}

void CacheCtrl::getRecentEvents(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const int count = parseCountParam(req->getParameter("count"), 100);
    const auto events = Cache::CacheInvalidation::instance().getRecentEvents(count);

    Json::Value body;
    body["count"] = Json::UInt64(events.size());
    body["events"] = Json::arrayValue;

    for (const auto& event : events) {
        Json::Value item;
        item["key"] = event.key;
        item["reason"] = event.reason;
        item["source"] = event.source;
        item["timestamp"] = Json::Int64(event.timestamp);
        body["events"].append(item);
    }

    callback(makeJsonResponse(body));
}

} // namespace Monitor
