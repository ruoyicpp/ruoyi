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
        auto pn = req->getParameter("pageNum");
        auto ps = req->getParameter("pageSize");
        if (!pn.empty()) p.pageNum  = std::stoi(pn);
        if (!ps.empty()) p.pageSize = std::stoi(ps);
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
