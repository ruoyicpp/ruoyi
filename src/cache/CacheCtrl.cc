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

Json::Value cacheValueToJson(const Cache::CacheValue& value) {
    if (std::holds_alternative<std::string>(value)) {
        return Json::Value(std::get<std::string>(value));
    }
    if (std::holds_alternative<Json::Value>(value)) {
        return std::get<Json::Value>(value);
    }
    return Json::Value();
}

Json::Value cacheEntryToJson(const Cache::CacheEntry& entry) {
    Json::Value json;
    json["value"] = cacheValueToJson(entry.value);
    json["isNull"] = entry.isNullValue();
    json["isExpired"] = entry.isExpired();
    return json;
}

drogon::HttpResponsePtr makeJsonResponse(const Json::Value& body,
                                         drogon::HttpStatusCode status = drogon::k200OK) {
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
}

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

void CacheCtrl::getStats(const drogon::HttpRequestPtr&,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto& stats = Cache::CacheStrategy::instance().stats();

    Json::Value body;
    body["hits"] = Json::UInt64(stats.hits.load());
    body["misses"] = Json::UInt64(stats.misses.load());
    body["sets"] = Json::UInt64(stats.sets.load());
    body["evictions"] = Json::UInt64(stats.evictions.load());
    body["errors"] = Json::UInt64(stats.errors.load());
    body["hitRate"] = stats.hitRate();
    body["redisConnected"] = Cache::RedisCluster::instance().isConnected();
    body["redisNodeCount"] = Cache::RedisCluster::instance().nodeCount();
    body["warmupRunning"] = Cache::CacheWarmup::instance().isRunning();
    body["warmupCompleted"] = Json::UInt64(Cache::CacheWarmup::instance().completedCount());
    body["warmupPending"] = Json::UInt64(Cache::CacheWarmup::instance().taskCount());
    body["warmupFailed"] = Json::UInt64(Cache::CacheWarmup::instance().failedCount());
    body["warmupProgress"] = Cache::CacheWarmup::instance().progress();
    body["totalInvalidations"] = Json::Int64(Cache::CacheInvalidation::instance().totalInvalidations());
    body["totalPatternInvalidations"] = Json::Int64(
        Cache::CacheInvalidation::instance().totalPatternInvalidations());

    callback(makeJsonResponse(body));
}

void CacheCtrl::getKeys(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto pattern = req->getParameter("pattern").empty()
        ? std::string("*")
        : req->getParameter("pattern");
    const int count = parseCountParam(req->getParameter("count"), 100);

    const auto keys = Cache::RedisCluster::instance().scan(pattern, count);

    Json::Value body;
    body["pattern"] = pattern;
    body["count"] = Json::UInt64(keys.size());
    body["keys"] = Json::arrayValue;
    for (const auto& key : keys) {
        body["keys"].append(key);
    }

    callback(makeJsonResponse(body));
}

void CacheCtrl::getKeyValue(const drogon::HttpRequestPtr&,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& key) {
    auto entry = Cache::CacheStrategy::instance().get(key);

    Json::Value body;
    body["key"] = key;
    body["found"] = static_cast<bool>(entry);
    if (entry) {
        body["entry"] = cacheEntryToJson(*entry);
    }

    callback(makeJsonResponse(body, entry ? drogon::k200OK : drogon::k404NotFound));
}

void CacheCtrl::deleteKey(const drogon::HttpRequestPtr&,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          const std::string& key) {
    const bool removed = Cache::CacheStrategy::instance().remove(key);
    if (removed) {
        Cache::CacheInvalidation::instance().invalidate(key, "api_delete", "CacheCtrl");
    }

    Json::Value body;
    body["key"] = key;
    body["removed"] = removed;

    callback(makeJsonResponse(body));
}

void CacheCtrl::warmup(const drogon::HttpRequestPtr&,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    Cache::CacheWarmup::instance().runAsync();

    Json::Value body;
    body["started"] = true;
    body["running"] = Cache::CacheWarmup::instance().isRunning();
    body["completedTasks"] = Json::UInt64(Cache::CacheWarmup::instance().completedCount());
    body["pendingTasks"] = Json::UInt64(Cache::CacheWarmup::instance().taskCount());
    body["failedTasks"] = Json::UInt64(Cache::CacheWarmup::instance().failedCount());
    body["progress"] = Cache::CacheWarmup::instance().progress();

    callback(makeJsonResponse(body));
}

void CacheCtrl::clearCache(const drogon::HttpRequestPtr&,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    Cache::CacheStrategy::instance().clear();

    Json::Value body;
    body["cleared"] = true;

    callback(makeJsonResponse(body));
}

void CacheCtrl::getClusterNodes(const drogon::HttpRequestPtr&,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
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
