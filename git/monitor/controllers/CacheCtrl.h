#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/TokenCache.h"
#include "../../common/Constants.h"
#include "../../filters/PermFilter.h"
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#else
#  include <sys/resource.h>
#endif

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

        size_t memKeys   = MemCache::instance().size();
        size_t tokenKeys = TokenCache::instance().size();
        size_t totalKeys = memKeys + tokenKeys;

        // 获取进程内存用量
        std::string memHuman = "N/A";
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            size_t mb = pmc.WorkingSetSize / 1024 / 1024;
            memHuman = std::to_string(mb) + "M";
        }
#else
        struct rusage ru{};
        if (getrusage(RUSAGE_SELF, &ru) == 0) {
            size_t mb = (size_t)ru.ru_maxrss / 1024;
            memHuman = std::to_string(mb) + "M";
        }
#endif
        Json::Value info;
        info["redis_version"]      = "in-process-cache";
        info["redis_mode"]         = "standalone";
        info["tcp_port"]           = 0;
        info["connected_clients"]  = (Json::Int)tokenKeys;
        info["used_memory_human"]  = memHuman;
        info["used_memory_rss_human"] = memHuman;
        info["mem_fragmentation_ratio"] = "1.00";
        info["uptime_in_days"]     = 0;
        info["aof_enabled"]        = 0;
        result["info"]  = info;
        result["dbSize"] = (Json::UInt64)totalKeys;

        // 按前缀统计各缓存组命中数
        Json::Value commandStats(Json::arrayValue);
        auto addStat = [&](const std::string& name, size_t cnt) {
            Json::Value cs;
            cs["name"]  = name;
            cs["value"] = "calls=" + std::to_string(cnt) + ",usec=0,usec_per_call=0.00";
            commandStats.append(cs);
        };
        addStat("token_sessions",  tokenKeys);
        addStat("mem_cache_keys",  memKeys);
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
