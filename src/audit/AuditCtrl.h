/**
 * @file AuditCtrl.h
 * @brief 安全审计控制器 — Manticore 全文检索 + 聚合统计 + 队列状态
 *
 * 功能概述：
 *   - 全文检索：透传 Manticore /search，支持 match 关键字 + 字段过滤 + 分页
 *   - 聚合统计：按 rule/action/ip/event_type 分组计数（多维聚合）
 *   - 队列状态：查看上报队列深度、已上报/丢弃计数
 *   - 手动清理：触发过期日志清理（也可由定时任务自动执行）
 *
 * API 端点：
 *   GET  /monitor/audit/search  - 检索（?q=&ip=&rule=&action=&from=&to=&page=&size=）
 *   GET  /monitor/audit/aggs    - 聚合统计（?field=rule|action|ip|event_type）
 *   GET  /monitor/audit/status  - 上报队列状态
 *   POST /monitor/audit/cleanup - 手动清理过期日志
 *
 * 权限要求：monitor:audit:* 系列权限
 * 平台说明：仅 Linux 版本可用（非 Linux 返回 501）
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include <cstdlib>
#include <sstream>
#include <memory>
#include "../common/AjaxResult.h"
#include "../common/OperLogUtils.h"
#include "../filters/PermFilter.h"
#include "AuditQueue.h"
#include "ManticoreClient.h"

/**
 * @class AuditCtrl
 * @brief 安全审计检索控制器
 */
class AuditCtrl : public drogon::HttpController<AuditCtrl> {
public:
    /// 非 Linux 平台统一返回"不可用"
    static bool auditUnavailable(std::function<void(const drogon::HttpResponsePtr&)>& cb) {
#ifndef __linux__
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            Json::Value("安全审计功能仅在 Linux 构建中可用"));
        resp->setStatusCode(drogon::k501NotImplemented);
        cb(resp);
        return true;
#else
        (void)cb;
        return false;
#endif
    }

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(AuditCtrl::search,  "/monitor/audit/search",  drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(AuditCtrl::aggs,    "/monitor/audit/aggs",    drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(AuditCtrl::status,  "/monitor/audit/status",  drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(AuditCtrl::cleanup, "/monitor/audit/cleanup", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    /// GET /monitor/audit/search — 全文检索 + 字段过滤 + 分页
    void search(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:audit:list");
        if (auditUnavailable(cb)) return;
        if (!AuditQueue::instance().isEnabled()) {
            RESP_ERR(cb, "审计上报未启用（security.audit.enabled=false）"); return;
        }

        auto q      = req->getParameter("q");        // 全文关键字
        auto ip     = req->getParameter("ip");
        auto rule   = req->getParameter("rule");
        auto action = req->getParameter("action");
        auto etype  = req->getParameter("event_type");
        int64_t from = std::atoll(req->getParameter("from").c_str()); // ts 起
        int64_t to   = std::atoll(req->getParameter("to").c_str());   // ts 止
        int page = std::atoi(req->getParameter("page").c_str());
        int size = std::atoi(req->getParameter("size").c_str());
        if (page <= 0) page = 1;
        if (size <= 0 || size > 200) size = 50;

        // 组装 Manticore /search 查询
        Json::Value query, must(Json::arrayValue);
        if (!q.empty()) {
            Json::Value m; m["match"]["*"] = q; must.append(m);
        }
        auto eq = [&](const char* f, const std::string& v) {
            if (v.empty()) return;
            Json::Value m; m["equals"][f] = v; must.append(m);
        };
        eq("ip", ip); eq("rule", rule); eq("action", action);
        eq("event_type", etype);
        if (from > 0 || to > 0) {
            Json::Value m;
            if (from > 0) m["range"]["ts"]["gte"] = (Json::Int64)from;
            if (to   > 0) m["range"]["ts"]["lte"] = (Json::Int64)to;
            must.append(m);
        }
        if (must.empty()) query["query"]["match_all"] = Json::Value(Json::objectValue);
        else              query["query"]["bool"]["must"] = must;
        query["index"]  = AuditQueue::instance().index();
        query["limit"]  = size;
        query["offset"] = (page - 1) * size;
        query["sort"]   = Json::Value(Json::arrayValue);
        query["sort"].append(Json::Value("_score"));   // 相关度优先
        Json::Value tsSort; tsSort["ts"] = "desc";
        query["sort"].append(tsSort);

        // cb 是右值引用参数，decltype(cb) 为引用类型，须显式指定 function 类型
        using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
        auto cbShared = std::make_shared<CbT>(std::move(cb));
        ManticoreClient::search(AuditQueue::instance().endpoint(), query,
            [cbShared](bool ok, int status, const std::string& body) {
                auto& cb = *cbShared;
                if (!ok || status != 200) {
                    RESP_ERR(cb, "Manticore 查询失败: HTTP " + std::to_string(status));
                    return;
                }
                Json::Value j; std::string err;
                Json::CharReaderBuilder rb;
                std::istringstream ss(body);
                if (!Json::parseFromStream(rb, ss, &j, &err)) {
                    RESP_ERR(cb, "Manticore 响应解析失败"); return;
                }
                // 提取 hits + total
                Json::Value out;
                out["total"] = j["hits"]["total"];
                Json::Value arr(Json::arrayValue);
                for (auto& h : j["hits"]["hits"])
                    arr.append(h["_source"]);
                out["list"] = arr;
                RESP_OK(cb, out);
            });
    }

    /// GET /monitor/audit/aggs — 按字段聚合统计
    void aggs(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:audit:list");
        if (auditUnavailable(cb)) return;
        if (!AuditQueue::instance().isEnabled()) {
            RESP_ERR(cb, "审计上报未启用"); return;
        }
        std::string field = req->getParameter("field");
        if (field.empty()) field = "rule";
        // 只允许白名单字段，防注入
        static const char* allowed[] = {"rule","action","ip","event_type","method","target"};
        bool ok = false;
        for (auto* a : allowed) if (field == a) { ok = true; break; }
        if (!ok) { RESP_ERR(cb, "不支持的聚合字段: " + field); return; }

        Json::Value query;
        query["index"] = AuditQueue::instance().index();
        query["limit"] = 0;
        query["aggs"]["by_field"]["terms"]["field"]    = field;
        query["aggs"]["by_field"]["terms"]["size"]     = 50;

        using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
        auto cbShared = std::make_shared<CbT>(std::move(cb));
        ManticoreClient::search(AuditQueue::instance().endpoint(), query,
            [cbShared](bool ok, int status, const std::string& body) {
                auto& cb = *cbShared;
                if (!ok || status != 200) {
                    RESP_ERR(cb, "Manticore 聚合失败"); return;
                }
                Json::Value j; std::string err;
                Json::CharReaderBuilder rb;
                std::istringstream ss(body);
                if (!Json::parseFromStream(rb, ss, &j, &err)) {
                    RESP_ERR(cb, "响应解析失败"); return;
                }
                RESP_OK(cb, j["aggs"]["by_field"]["buckets"]);
            });
    }

    /// GET /monitor/audit/status — 上报队列状态
    void status(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:audit:list");
        if (auditUnavailable(cb)) return;
        Json::Value j;
        j["enabled"]  = AuditQueue::instance().isEnabled();
        j["pending"]  = (Json::UInt64)AuditQueue::instance().pending();
        j["reported"] = (Json::UInt64)AuditQueue::instance().reported();
        j["dropped"]  = (Json::UInt64)AuditQueue::instance().dropped();
        RESP_OK(cb, j);
    }

    /// POST /monitor/audit/cleanup — 手动清理过期日志
    void cleanup(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:audit:remove");
        if (auditUnavailable(cb)) return;
        AuditQueue::instance().cleanupExpired();
        LOG_OPER(req, "审计日志清理", BusinessType::REMOVE);
        RESP_MSG(cb, "清理任务已提交");
    }
};
