/**
 * @file DataSourceCtrl.h
 * @brief 多数据源管理控制器 — 注册/测试/查询/默认切换
 *
 * 功能概述：
 *   - 数据源 CRUD：sys_datasource 表持久化，运行时生效
 *   - 连通性测试：实时探测 PG/SQLite 连接
 *   - 只读查询：queryOn 按名路由（仅 SELECT/WITH/SHOW/EXPLAIN）
 *   - 默认切换：is_default 标记（主连接切换需重启生效）
 *
 * API 端点（均需登录 + 权限）：
 *   - GET    /system/datasource/list          数据源列表（passwd 脱敏）
 *   - POST   /system/datasource               新增数据源
 *   - DELETE /system/datasource/{name}        删除数据源
 *   - POST   /system/datasource/test/{name}   连通性测试
 *   - POST   /system/datasource/query/{name}  只读查询 {sql}
 *   - POST   /system/datasource/default/{name} 设为默认
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include "../common/AjaxResult.h"
#include "../common/OperLogUtils.h"
#include "../filters/PermFilter.h"
#include "DataSourceManager.h"

/**
 * @class DataSourceCtrl
 * @brief 多数据源管理控制器
 */
class DataSourceCtrl : public drogon::HttpController<DataSourceCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(DataSourceCtrl::list,       "/system/datasource/list",           drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(DataSourceCtrl::add,        "/system/datasource",                drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(DataSourceCtrl::remove,     "/system/datasource/{name}",         drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(DataSourceCtrl::test,       "/system/datasource/test/{name}",    drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(DataSourceCtrl::query,      "/system/datasource/query/{name}",   drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(DataSourceCtrl::setDefault, "/system/datasource/default/{name}", drogon::Post,   "JwtAuthFilter");
    METHOD_LIST_END

    /// GET /system/datasource/list
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:datasource:list");
        Json::Value arr(Json::arrayValue);
        for (auto& ds : DataSourceManager::instance().list()) {
            Json::Value j;
            j["id"]          = (Json::Int64)ds.id;
            j["name"]        = ds.name;
            j["db_type"]     = ds.dbType;
            j["host"]        = ds.host;
            j["port"]        = ds.port;
            j["dbname"]      = ds.dbname;
            j["username"]    = ds.username;
            j["status"]      = ds.status;
            j["is_default"]  = ds.isDefault;
            j["last_ok"]     = ds.lastOk;
            j["last_error"]  = ds.lastError;
            j["last_test_at"]= (Json::Int64)ds.lastTestAt;
            arr.append(j);
        }
        RESP_OK(cb, arr);
    }

    /// POST /system/datasource {name, db_type, host, port, dbname, username, passwd, status}
    void add(const drogon::HttpRequestPtr& req,
             std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:datasource:add");
        auto b = req->getJsonObject();
        if (!b) { RESP_ERR(cb, "请求体须为 JSON"); return; }
        DataSourceManager::DataSource ds;
        ds.name     = b->get("name", "").asString();
        ds.dbType   = b->get("db_type", "postgres").asString();
        ds.host     = b->get("host", "127.0.0.1").asString();
        ds.port     = b->get("port", 5432).asInt();
        ds.dbname   = b->get("dbname", "").asString();
        ds.username = b->get("username", "").asString();
        ds.passwd   = b->get("passwd", "").asString();
        ds.status   = b->get("status", "0").asString();
        std::string err;
        if (!DataSourceManager::instance().add(ds, err)) { RESP_ERR(cb, err); return; }
        LOG_OPER_PARAM(req, "数据源", BusinessType::INSERT, ds.name);
        RESP_MSG(cb, "添加成功");
    }

    /// DELETE /system/datasource/{name}
    void remove(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                const std::string& name) {
        CHECK_PERM(req, cb, "system:datasource:remove");
        if (!DataSourceManager::instance().remove(name)) {
            RESP_ERR(cb, "数据源不存在: " + name); return;
        }
        LOG_OPER_PARAM(req, "数据源", BusinessType::REMOVE, name);
        RESP_MSG(cb, "删除成功");
    }

    /// POST /system/datasource/test/{name}
    void test(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb,
              const std::string& name) {
        CHECK_PERM(req, cb, "system:datasource:list");
        std::string err;
        if (!DataSourceManager::instance().test(name, err)) {
            RESP_ERR(cb, "连接失败: " + err); return;
        }
        RESP_MSG(cb, "连接正常");
    }

    /// POST /system/datasource/query/{name} {sql}
    void query(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb,
               const std::string& name) {
        CHECK_PERM(req, cb, "system:datasource:query");
        auto b = req->getJsonObject();
        if (!b || !(*b).isMember("sql")) { RESP_ERR(cb, "缺少 sql"); return; }
        std::string err;
        auto rows = DataSourceManager::instance().queryOn(
            name, (*b)["sql"].asString(), err);
        if (!rows) { RESP_ERR(cb, err); return; }
        RESP_OK(cb, *rows);
    }

    /// POST /system/datasource/default/{name}
    void setDefault(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                    const std::string& name) {
        CHECK_PERM(req, cb, "system:datasource:edit");
        if (!DataSourceManager::instance().setDefault(name)) {
            RESP_ERR(cb, "数据源不存在: " + name); return;
        }
        LOG_OPER_PARAM(req, "数据源", BusinessType::UPDATE, name);
        RESP_MSG(cb, "已设为默认（主连接切换需重启生效）");
    }
};
