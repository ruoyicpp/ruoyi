#pragma once

#include <drogon/HttpController.h>

namespace Monitor {

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

    void getStats(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getKeys(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getKeyValue(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& key);

    void deleteKey(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& key);

    void warmup(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void clearCache(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getClusterNodes(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getClusterSlots(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getRecentEvents(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace Monitor
