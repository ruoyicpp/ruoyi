/**
 * @file PageUtils.h
 * @brief 分页工具 — 处理分页参数和结果
 * 
 * 功能概述：
 *   - 分页参数解析：从 HTTP 请求中解析分页参数
 *   - 参数验证：验证分页参数的合理性，防止恶意请求
 *   - 分页结果：统一的分页结果格式
 *   - 排序支持：支持自定义排序字段和排序方向
 * 
 * 分页参数：
 *   - pageNum: 页码（从 1 开始，默认 1）
 *   - pageSize: 每页记录数（默认 10，最大 1000）
 *   - orderByColumn: 排序字段（可选）
 *   - isAsc: 排序方向（asc 升序，desc 降序，默认 asc）
 * 
 * 使用示例：
 *   // 解析分页参数
 *   auto page = PageParam::fromRequest(req);
 *   
 *   // 计算数据库 OFFSET
 *   int offset = page.offset();  // (pageNum - 1) * pageSize
 *   
 *   // 构建分页结果
 *   PageResult result;
 *   result.total = totalCount;
 *   result.rows = dataArray;
 *   RESP_JSON(cb, result.toJson());
 * 
 * 响应格式：
 *   {
 *     "code": 200,
 *     "msg": "查询成功",
 *     "total": 100,
 *     "rows": [...]
 *   }
 */

#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <vector>

/**
 * @struct PageParam
 * @brief 分页参数
 * 
 * 用于从 HTTP 请求中解析和验证分页参数。
 * 支持自动参数验证和范围限制，防止恶意请求。
 */
struct PageParam {
    int pageNum  = 1;
    int pageSize = 10;
    std::string orderByColumn;
    std::string isAsc = "asc";

    static PageParam fromRequest(const drogon::HttpRequestPtr &req) {
        PageParam p;
        auto safeStoi = [](const std::string &s, int defaultVal) {
            if (s.empty()) return defaultVal;
            try { return std::stoi(s); } catch (...) { return defaultVal; }
        };
        p.pageNum  = safeStoi(req->getParameter("pageNum"),  1);
        p.pageSize = safeStoi(req->getParameter("pageSize"), 10);
        // 防御：限制 pageNum/pageSize 合理范围，避免恶意大值或负数
        if (p.pageNum  < 1)    p.pageNum  = 1;
        if (p.pageSize < 1)    p.pageSize = 10;
        if (p.pageSize > 1000) p.pageSize = 1000;
        p.orderByColumn = req->getParameter("orderByColumn");
        auto asc = req->getParameter("isAsc");
        if (!asc.empty()) p.isAsc = asc;
        return p;
    }

    int offset() const { return (pageNum - 1) * pageSize; }
};

// 分页结果
struct PageResult {
    long total = 0;
    Json::Value rows;  // array

    Json::Value toJson() const {
        Json::Value j;
        j["total"] = (Json::Int64)total;
        j["rows"]  = rows;
        j["code"]  = 200;
        j["msg"]   = "查询成功";
        return j;
    }
};
