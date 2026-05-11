#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <vector>

// 分页参数
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
