#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

// 代码生成工具 /tool/gen (stub)
class GenCtrl : public drogon::HttpController<GenCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(GenCtrl::list,         "/tool/gen/list",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::getById,      "/tool/gen/{tableId}",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::dbList,       "/tool/gen/db/list",          drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::columnList,   "/tool/gen/column/{tableId}", drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::importTable,  "/tool/gen/importTable",      drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::edit,         "/tool/gen",                  drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::remove,       "/tool/gen/{tableIds}",       drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::preview,      "/tool/gen/preview/{tableId}",drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::genCode,      "/tool/gen/genCode/{tableName}", drogon::Get, "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::batchGenCode, "/tool/gen/batchGenCode",     drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::synchDb,      "/tool/gen/synchDb/{tableName}", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto res = db.query(
            "SELECT table_id,table_name,table_comment,class_name,create_time,update_time "
            "FROM gen_table ORDER BY table_id DESC LIMIT " + std::to_string(page.pageSize) +
            " OFFSET " + std::to_string(page.offset()));
        auto cntRes = db.query("SELECT COUNT(*) FROM gen_table");
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["tableId"]      = (Json::Int64)res.longVal(i, 0);
            j["tableName"]    = res.str(i, 1);
            j["tableComment"] = res.str(i, 2);
            j["className"]    = res.str(i, 3);
            j["createTime"]   = fmtTs(res.str(i, 4));
            j["updateTime"]   = fmtTs(res.str(i, 5));
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long tableId) {
        CHECK_PERM(req, cb, "tool:gen:query");
        RESP_OK(cb, Json::Value(Json::objectValue));
    }

    void dbList(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:list");
        // 查询 PostgreSQL information_schema 获取数据库表列表
        auto& db = DatabaseService::instance();
        auto tableName = req->getParameter("tableName");
        auto tableComment = req->getParameter("tableComment");
        std::string sql =
            "SELECT table_name,COALESCE(obj_description((quote_ident(table_schema)||'.'||quote_ident(table_name))::regclass),'') AS table_comment "
            "FROM information_schema.tables WHERE table_schema='public' AND table_type='BASE TABLE' "
            "AND table_name NOT IN (SELECT table_name FROM gen_table)";
        std::vector<std::string> params;
        int idx = 1;
        if (!tableName.empty()) { sql += " AND table_name LIKE $" + std::to_string(idx++); params.push_back("%" + tableName + "%"); }
        auto page = PageParam::fromRequest(req);
        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
        sql += " ORDER BY table_name LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());
        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["tableName"]    = res.str(i, 0);
            j["tableComment"] = res.str(i, 1);
            j["createTime"]   = "";
            j["updateTime"]   = "";
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void columnList(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long tableId) {
        CHECK_PERM(req, cb, "tool:gen:list");
        PageResult pr; pr.total = 0; pr.rows = Json::Value(Json::arrayValue);
        RESP_JSON(cb, pr.toJson());
    }

    void importTable(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:import");
        RESP_ERR(cb, "代码生成功能暂未实现");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:edit");
        RESP_ERR(cb, "代码生成功能暂未实现");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &tableIds) {
        CHECK_PERM(req, cb, "tool:gen:remove");
        RESP_ERR(cb, "代码生成功能暂未实现");
    }

    void preview(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long tableId) {
        CHECK_PERM(req, cb, "tool:gen:preview");
        RESP_ERR(cb, "代码生成功能暂未实现");
    }

    void genCode(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &tableName) {
        CHECK_PERM(req, cb, "tool:gen:code");
        RESP_ERR(cb, "代码生成功能暂未实现");
    }

    void batchGenCode(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:code");
        RESP_ERR(cb, "代码生成功能暂未实现");
    }

    void synchDb(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &tableName) {
        CHECK_PERM(req, cb, "tool:gen:edit");
        RESP_ERR(cb, "代码生成功能暂未实现");
    }
};
