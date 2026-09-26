/**
 * @file ScreenCtrl.h
 * @brief 大屏可视化接口 — 统计指标统一输出，给数据看板
 *
 * 功能概述：
 *   - 聚合 MetricsCollector 计数器 + DB 统计 + WS 在线 + 慢查询
 *   - 单端点输出 JSON，前端大屏轮询（建议 5~10s）
 *   - 无需登录（内网大屏场景），可选配 token 参数鉴权
 *
 * API 端点：
 *   - GET /screen/overview    总览（请求/DB/登录/在线/慢查询/系统）
 *   - GET /screen/realtime    实时指标（仅计数器，轻量高频）
 *
 * 输出结构（overview）：
 *   {
 *     "requests":  {"total":N,"2xx":N,"4xx":N,"5xx":N,"slow":N,"avg_ms":N},
 *     "db":        {"queries":N,"execs":N,"slow":N,"errors":N,"backend":"postgres"},
 *     "auth":      {"login_ok":N,"login_fail":N,"rate_limited":N},
 *     "online":    {"ws_users":N,"cluster_users":N},
 *     "business":  {"users":N,"roles":N,"depts":N,"notices":N},
 *     "slow_sql":  {"count":N,"pending":N},
 *     "uptime_s":  N,
 *     "ts":        N
 *   }
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include <chrono>
#include <ctime>
#include "../common/AjaxResult.h"
#include "../common/MetricsCollector.h"
#include "../common/ClusterSession.h"
#include "../monitor/controllers/WsNotifyCtrl.h"
#include "../log/SlowLogQueue.h"
#include "../services/DatabaseService.h"

/**
 * @class ScreenCtrl
 * @brief 大屏可视化控制器
 */
class ScreenCtrl : public drogon::HttpController<ScreenCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ScreenCtrl::overview, "/screen/overview", drogon::Get);
        ADD_METHOD_TO(ScreenCtrl::realtime, "/screen/realtime", drogon::Get);
    METHOD_LIST_END

    /// GET /screen/overview — 总览（聚合所有维度）
    void overview(const drogon::HttpRequestPtr&,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& m = MetricsCollector::instance();
        Json::Value j;

        // 请求
        j["requests"]["total"]  = (Json::Int64)m.reqTotal.load();
        j["requests"]["2xx"]    = (Json::Int64)m.req2xx.load();
        j["requests"]["4xx"]    = (Json::Int64)m.req4xx.load();
        j["requests"]["5xx"]    = (Json::Int64)m.req5xx.load();
        j["requests"]["slow"]   = (Json::Int64)m.slowReqs.load();
        uint64_t cnt = m.histCount.load(), sum = m.histSumMs.load();
        j["requests"]["avg_ms"] = cnt ? (Json::Int64)(sum / cnt) : 0;

        // DB
        auto& db = DatabaseService::instance();
        j["db"]["queries"] = (Json::Int64)m.dbQueries.load();
        j["db"]["execs"]   = (Json::Int64)m.dbExecs.load();
        j["db"]["slow"]    = (Json::Int64)m.dbSlow.load();
        j["db"]["errors"]  = (Json::Int64)m.dbErrors.load();
        j["db"]["backend"] = db.isUsingSqlite() ? "sqlite" : "postgres";

        // 认证
        j["auth"]["login_ok"]     = (Json::Int64)m.loginSuccess.load();
        j["auth"]["login_fail"]   = (Json::Int64)m.loginFail.load();
        j["auth"]["rate_limited"] = (Json::Int64)m.rateLimited.load();

        // 在线
        j["online"]["ws_users"]      = (Json::Int64)WsNotifyCtrl::onlineUsers().size();
        j["online"]["cluster_users"] = (Json::Int64)ClusterSession::instance()
                                           .clusterOnlineUsers().size();

        // 业务统计（DB 计数，低频查询可接受）
        j["business"]["users"]   = countTable("sys_user");
        j["business"]["roles"]   = countTable("sys_role");
        j["business"]["depts"]   = countTable("sys_dept");
        j["business"]["notices"] = countTable("sys_notice");

        // 慢查询
        j["slow_sql"]["count"]   = countTable("sys_slow_log");
        j["slow_sql"]["pending"] = (Json::Int64)SlowLogQueue::instance().pending();

        j["uptime_s"] = (Json::Int64)uptime();
        j["ts"]       = (Json::Int64)std::time(nullptr);
        RESP_OK(cb, j);
    }

    /// GET /screen/realtime — 轻量实时指标（仅计数器，无 DB 查询）
    void realtime(const drogon::HttpRequestPtr&,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& m = MetricsCollector::instance();
        Json::Value j;
        j["req_total"]    = (Json::Int64)m.reqTotal.load();
        j["req_5xx"]      = (Json::Int64)m.req5xx.load();
        j["db_queries"]   = (Json::Int64)m.dbQueries.load();
        j["db_errors"]    = (Json::Int64)m.dbErrors.load();
        j["ws_online"]    = (Json::Int64)WsNotifyCtrl::onlineUsers().size();
        j["slow_pending"] = (Json::Int64)SlowLogQueue::instance().pending();
        j["ts"]           = (Json::Int64)std::time(nullptr);
        RESP_OK(cb, j);
    }

private:
    /// 表行数（容错：表不存在返回 0）
    static Json::Int64 countTable(const char* table) {
        auto res = DatabaseService::instance().query(
            std::string("SELECT COUNT(*) FROM ") + table);
        return (res.ok() && res.rows() > 0) ? res.longVal(0, 0) : 0;
    }

    /// 进程运行秒数
    static int64_t uptime() {
        static auto t0 = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - t0).count();
    }
};
