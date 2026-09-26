/**
 * @file SlowLogCtrl.h
 * @brief SQL 慢查询审计控制器 — 列表/统计/清理
 *
 * 功能概述：
 *   - 慢查询列表：sys_slow_log 分页查询（按耗时/操作类型过滤）
 *   - 统计：按操作类型聚合平均/最大耗时
 *   - 队列状态：SlowLogQueue 待落库/已落库/丢弃计数
 *   - 清理：手动清空慢查询日志
 *
 * API 端点（均需登录 + monitor:slowlog:* 权限）：
 *   - GET    /monitor/slowlog/list       分页列表（?op=&min_ms=&page=&size=）
 *   - GET    /monitor/slowlog/stats      按操作类型聚合统计
 *   - GET    /monitor/slowlog/queue      异步队列状态
 *   - DELETE /monitor/slowlog/clean      清空日志
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include "../common/AjaxResult.h"
#include "../common/OperLogUtils.h"
#include "../common/PageUtils.h"
#include "../filters/PermFilter.h"
#include "../services/DatabaseService.h"
#include "SlowLogQueue.h"

/**
 * @class SlowLogCtrl
 * @brief 慢查询审计控制器
 */
class SlowLogCtrl : public drogon::HttpController<SlowLogCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SlowLogCtrl::list,   "/monitor/slowlog/list",  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SlowLogCtrl::stats,  "/monitor/slowlog/stats", drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SlowLogCtrl::queue,  "/monitor/slowlog/queue", drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SlowLogCtrl::clean,  "/monitor/slowlog/clean", drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    /// GET /monitor/slowlog/list?op=&min_ms=&page=&size=
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:slowlog:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT id,op,sql_text,cost_ms,create_time FROM sys_slow_log WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto op    = req->getParameter("op");
        auto minMs = req->getParameter("min_ms");
        if (!op.empty())    { sql += " AND op=$" + std::to_string(idx++);    params.push_back(op); }
        if (!minMs.empty()) { sql += " AND cost_ms>=$" + std::to_string(idx++); params.push_back(minMs); }

        auto cnt = db.queryParams("SELECT COUNT(*) FROM (" + sql + ") t", params);
        long total = (cnt.ok() && cnt.rows() > 0) ? cnt.longVal(0, 0) : 0;
        sql += " ORDER BY id DESC LIMIT $" + std::to_string(idx++)
             + " OFFSET $" + std::to_string(idx++);
        params.push_back(std::to_string(page.pageSize));
        params.push_back(std::to_string(page.offset()));
        auto res = db.queryParams(sql, params);

        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]          = (Json::Int64)res.longVal(i, 0);
            j["op"]          = res.str(i, 1);
            j["sql_text"]    = res.str(i, 2);
            j["cost_ms"]     = (Json::Int64)res.longVal(i, 3);
            j["create_time"] = res.str(i, 4);
            rows.append(j);
        }
        Json::Value data; data["total"] = (Json::Int64)total; data["rows"] = rows;
        RESP_OK(cb, data);
    }

    /// GET /monitor/slowlog/stats — 按操作类型聚合
    void stats(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:slowlog:list");
        auto res = DatabaseService::instance().query(
            "SELECT op, COUNT(*), AVG(cost_ms)::bigint, MAX(cost_ms) "
            "FROM sys_slow_log GROUP BY op ORDER BY MAX(cost_ms) DESC");
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["op"]       = res.str(i, 0);
            j["count"]    = (Json::Int64)res.longVal(i, 1);
            j["avg_ms"]   = (Json::Int64)res.longVal(i, 2);
            j["max_ms"]   = (Json::Int64)res.longVal(i, 3);
            rows.append(j);
        }
        RESP_OK(cb, rows);
    }

    /// GET /monitor/slowlog/queue — 异步队列状态
    void queue(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:slowlog:list");
        Json::Value j;
        j["pending"] = (Json::Int64)SlowLogQueue::instance().pending();
        j["total"]   = (Json::Int64)SlowLogQueue::instance().total();
        j["flushed"] = (Json::Int64)SlowLogQueue::instance().flushed();
        j["dropped"] = (Json::Int64)SlowLogQueue::instance().dropped();
        RESP_OK(cb, j);
    }

    /// DELETE /monitor/slowlog/clean — 清空
    void clean(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:slowlog:remove");
        DatabaseService::instance().exec("DELETE FROM sys_slow_log");
        LOG_OPER(req, "慢查询日志", BusinessType::CLEAN);
        RESP_MSG(cb, "已清空");
    }
};
