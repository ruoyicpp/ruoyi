#pragma once
#include <drogon/HttpController.h>
#include <chrono>
#include "../../common/AjaxResult.h"
#include "../../services/DatabaseService.h"
#include "../../common/TokenCache.h"
#include "../../common/RateLimiter.h"

// GET /dashboard/stats — 首页实时统计
class DashboardCtrl : public drogon::HttpController<DashboardCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(DashboardCtrl::stats, "/dashboard/stats", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    void stats(const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& db = DatabaseService::instance();

        auto count = [&](const std::string& sql) -> long {
            auto r = db.query(sql);
            return (r.ok() && r.rows() > 0) ? r.longVal(0, 0) : 0L;
        };

        // 今日日期（YYYY-MM-DD）
        auto now    = std::chrono::system_clock::now();
        auto tt     = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &tt);
#else
        localtime_r(&tt, &tm_buf);
#endif
        char today[16];
        snprintf(today, sizeof(today), "%04d-%02d-%02d",
                 tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);

        Json::Value r;
        r["totalUsers"]       = (Json::Int64)count("SELECT COUNT(*) FROM sys_user WHERE del_flag='0'");
        r["totalRoles"]       = (Json::Int64)count("SELECT COUNT(*) FROM sys_role WHERE del_flag='0'");
        r["totalMenus"]       = (Json::Int64)count("SELECT COUNT(*) FROM sys_menu");
        r["todayLogins"]      = (Json::Int64)db.queryParams(
            "SELECT COUNT(*) FROM sys_logininfor WHERE login_time >= $1", {std::string(today)})
            .longVal(0, 0);
        r["onlineUsers"]      = (Json::Int64)TokenCache::instance().size();
        r["bannedIps"]        = (Json::Int64)RateLimiter::instance().bannedList().size();
        r["dbBackend"]        = db.backendInfo();
        r["dbConnected"]      = db.isConnected();
        r["totalOperLogs"]    = (Json::Int64)count("SELECT COUNT(*) FROM sys_oper_log");
        r["totalLoginLogs"]   = (Json::Int64)count("SELECT COUNT(*) FROM sys_logininfor");

        // 今日操作日志
        r["todayOperLogs"]    = (Json::Int64)db.queryParams(
            "SELECT COUNT(*) FROM sys_oper_log WHERE oper_time >= $1", {std::string(today)})
            .longVal(0, 0);

        RESP_OK(cb, r);
    }
};
