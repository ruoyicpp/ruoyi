#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/TokenCache.h"
#include "../../common/Constants.h"
#include "../../filters/PermFilter.h"

// 缓存监控 /monitor/cache
class CacheCtrl : public drogon::HttpController<CacheCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(CacheCtrl::getInfo,          "/monitor/cache",                      drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(CacheCtrl::getNames,         "/monitor/cache/getNames",             drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(CacheCtrl::getKeys,          "/monitor/cache/getKeys/{cacheName}",  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(CacheCtrl::getValue,         "/monitor/cache/getValue/{cacheName}/{cacheKey}", drogon::Get, "JwtAuthFilter");
        ADD_METHOD_TO(CacheCtrl::clearCacheName,   "/monitor/cache/clearCacheName/{cacheName}",   drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(CacheCtrl::clearCacheKey,    "/monitor/cache/clearCacheKey/{cacheKey}",     drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(CacheCtrl::clearCacheAll,    "/monitor/cache/clearCacheAll",                drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void getInfo(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:cache:list");
        Json::Value result;

        // 模拟 Redis info（内存缓存版本）
        Json::Value info;
        info["redis_version"] = "memory-cache";
        info["redis_mode"]    = "standalone";
        info["tcp_port"]      = 0;
        info["connected_clients"]   = (int)TokenCache::instance().size();
        info["used_memory_human"]   = std::to_string(MemCache::instance().size()) + " keys";
        info["uptime_in_days"]      = 0;
        result["info"] = info;

        result["dbSize"] = (Json::UInt64)(MemCache::instance().size() + TokenCache::instance().size());

        // command stats (simplified)
        Json::Value commandStats(Json::arrayValue);
        Json::Value cs1; cs1["name"] = "get"; cs1["value"] = "0";
        Json::Value cs2; cs2["name"] = "set"; cs2["value"] = "0";
        commandStats.append(cs1);
        commandStats.append(cs2);
        result["commandStats"] = commandStats;

        RESP_OK(cb, result);
    }

    void getNames(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:cache:list");
        Json::Value arr(Json::arrayValue);
        auto addCache = [&](const std::string &key, const std::string &remark) {
            Json::Value c;
            c["cacheName"] = key;
            c["remark"]    = remark;
            arr.append(c);
        };
        addCache(Constants::LOGIN_TOKEN_KEY, "用户信息");
        addCache(Constants::SYS_CONFIG_KEY,  "配置信息");
        addCache(Constants::SYS_DICT_KEY,    "数据字典");
        addCache(Constants::CAPTCHA_CODE_KEY,"验证码");
        addCache(Constants::PWD_ERR_CNT_KEY, "密码错误次数");
        RESP_OK(cb, arr);
    }

    void getKeys(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &cacheName) {
        CHECK_PERM(req, cb, "monitor:cache:list");
        auto keys = MemCache::instance().getKeysByPrefix(cacheName);
        Json::Value arr(Json::arrayValue);
        for (auto &k : keys) arr.append(k);
        RESP_OK(cb, arr);
    }

    void getValue(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                  const std::string &cacheName, const std::string &cacheKey) {
        CHECK_PERM(req, cb, "monitor:cache:list");
        auto val = MemCache::instance().getString(cacheKey);
        Json::Value c;
        c["cacheName"]  = cacheName;
        c["cacheKey"]   = cacheKey;
        c["cacheValue"] = val.value_or("");
        RESP_OK(cb, c);
    }

    void clearCacheName(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &cacheName) {
        CHECK_PERM(req, cb, "monitor:cache:list");
        MemCache::instance().removeByPrefix(cacheName);
        RESP_MSG(cb, "操作成功");
    }

    void clearCacheKey(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &cacheKey) {
        CHECK_PERM(req, cb, "monitor:cache:list");
        MemCache::instance().remove(cacheKey);
        RESP_MSG(cb, "操作成功");
    }

    void clearCacheAll(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:cache:list");
        MemCache::instance().clear();
        RESP_MSG(cb, "操作成功");
    }
};
