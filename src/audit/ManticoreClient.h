/**
 * @file ManticoreClient.h
 * @brief Manticore Search HTTP JSON API 客户端 — 安全审计日志存储
 *
 * 功能概述：
 *   - 纯 HTTP JSON API：只走 Manticore 的 HTTP 端口（默认 9308，本项目用 7700）
 *   - 禁止 MySQL 协议：不引入任何 MySQL 客户端依赖
 *   - 批量插入：/bulk 端点 NDJSON 格式，一次请求写入多条
 *   - 全文检索：/search 端点，支持 match/filter/聚合
 *   - SQL 透传：/sql 端点执行管理语句（建表/清理）
 *
 * Manticore 部署：
 *   - 使用官方预编译二进制（manticore 包），独立进程运行
 *   - 不静态链接源码，仅通过 HTTP 通信
 *   - 启动：systemctl start manticore 或 searchd --nodetach
 *
 * 表结构（waf_logs）：
 *   CREATE TABLE waf_logs (
 *     ts bigint, ip text, method string, uri text, ua text,
 *     rule string, target string, matched text, action string
 *   );
 *
 * 配置项（config.json → security.audit）：
 *   - enabled: 是否启用审计上报（默认 false）
 *   - endpoint: Manticore HTTP 地址（默认 http://127.0.0.1:7700）
 *   - index: 索引名（默认 waf_logs）
 *   - batch_size: 批量上报条数（默认 100）
 *   - flush_interval_ms: 强制刷新间隔（默认 3000）
 *   - retention_days: 日志留存天数（默认 30）
 */

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <json/json.h>
#include "../common/HttpCaller.h"

/**
 * @class ManticoreClient
 * @brief Manticore HTTP JSON API 静态客户端
 *
 * 所有方法异步非阻塞，回调签名与 HttpCaller 一致。
 */
class ManticoreClient {
public:
    using Cb = HttpCaller::Cb;

    /**
     * @brief 批量插入文档（/bulk NDJSON）
     * @param endpoint Manticore 地址，如 http://127.0.0.1:7700
     * @param index 索引名
     * @param docs JSON 文档数组
     */
    static void bulkInsert(const std::string& endpoint,
                           const std::string& index,
                           const std::vector<Json::Value>& docs,
                           Cb cb = nullptr) {
        // /bulk 要求 NDJSON：每行一个 {"insert":{"index":..,"doc":{..}}}
        std::string body;
        body.reserve(docs.size() * 256);
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        for (auto& d : docs) {
            Json::Value line;
            line["insert"]["index"] = index;
            line["insert"]["doc"]   = d;
            body += Json::writeString(wb, line);
            body += '\n';
        }
        HttpCaller::asyncPost(endpoint + "/bulk", body,
                              "application/x-ndjson", std::move(cb));
    }

    /**
     * @brief 全文检索（/search JSON）
     * @param endpoint Manticore 地址
     * @param query 完整 search JSON（含 index/query/limit/aggs）
     */
    static void search(const std::string& endpoint,
                       const Json::Value& query, Cb cb) {
        HttpCaller::asyncPost(endpoint + "/search",
                              Json::writeString(Json::StreamWriterBuilder(), query),
                              "application/json", std::move(cb));
    }

    /**
     * @brief 执行管理 SQL（/sql，如建表/删除过期数据）
     * @param endpoint Manticore 地址
     * @param sql SQL 语句
     */
    static void sql(const std::string& endpoint,
                    const std::string& sql, Cb cb = nullptr) {
        Json::Value j;
        j["query"] = sql;
        HttpCaller::asyncPost(endpoint + "/sql",
                              Json::writeString(Json::StreamWriterBuilder(), j),
                              "application/json", std::move(cb));
    }

    /// 建表语句（waf_logs 审计索引）
    static std::string createTableSql(const std::string& index) {
        return "CREATE TABLE IF NOT EXISTS " + index + " ("
               "ts bigint, ip text, method string, uri text, ua text, "
               "rule string, target string, matched text, action string, "
               "event_type string, detail text)";
    }

    /// 清理过期日志 SQL
    static std::string cleanupSql(const std::string& index, int64_t beforeTs) {
        return "DELETE FROM " + index + " WHERE ts < " + std::to_string(beforeTs);
    }
};
