/**
 * @file BuildCtrl.h
 * @brief 表单构建控制器 — 提供拖拽表单设计和管理功能
 * 
 * 功能概述：
 *   - 表单设计：拖拽式表单设计器，支持多种字段类型
 *   - 表单管理：创建、编辑、删除表单配置
 *   - 表单查询：查询表单列表和详情
 *   - 配置存储：存储表单配置和字段定义
 *   - 权限控制：基于权限字符串的访问控制
 * 
 * API 端点：
 *   - GET    /tool/build/list      - 查询表单列表
 *   - GET    /tool/build/{id}      - 查询表单详情
 *   - POST   /tool/build           - 创建表单
 *   - PUT    /tool/build           - 更新表单
 *   - DELETE /tool/build/{ids}     - 删除表单
 * 
 * 权限要求：
 *   - tool:build:list   - 查询表单列表
 *   - tool:build:query  - 查询表单详情
 *   - tool:build:add    - 创建表单
 *   - tool:build:edit   - 编辑表单
 *   - tool:build:remove - 删除表单
 * 
 * 表单配置结构：
 *   {
 *     "formId": 1,
 *     "formName": "用户注册表单",
 *     "formConf": { ... },        // 表单配置 JSON
 *     "formFields": [ ... ],      // 字段定义数组
 *     "remark": "用户注册"
 *   }
 * 
 * @see DatabaseService - 数据库服务
 * @see PermFilter - 权限检查工具
 */

#pragma once
#include <drogon/HttpController.h>
#include <sstream>
#include "../../common/AjaxResult.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

/**
 * @class BuildCtrl
 * @brief 表单构建控制器
 * 
 * 提供拖拽式表单设计和管理功能，支持表单配置的创建、编辑、删除和查询。
 * 与 Java 版若依保持一致，存储和查询拖拽表单配置。
 */
class BuildCtrl : public drogon::HttpController<BuildCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(BuildCtrl::list,   "/tool/build/list",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(BuildCtrl::getById,"/tool/build/{id}",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(BuildCtrl::save,   "/tool/build",         drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(BuildCtrl::update, "/tool/build",         drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(BuildCtrl::remove, "/tool/build/{ids}",   drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    /**
     * @brief 查询表单列表
     * 
     * GET /tool/build/list
     * 
     * @param req HTTP 请求
     * @param cb 回调函数
     * @return 表单列表
     */
    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:build:list");
        auto& db = DatabaseService::instance();
        auto res = db.query(
            "SELECT form_id,form_name,remark,create_by,create_time FROM tool_form_conf ORDER BY form_id DESC");
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["formId"]     = (Json::Int64)res.longVal(i, 0);
            j["formName"]   = res.str(i, 1);
            j["remark"]     = res.str(i, 2);
            j["createBy"]   = res.str(i, 3);
            j["createTime"] = fmtTs(res.str(i, 4));
            rows.append(j);
        }
        Json::Value data;
        data["total"] = (Json::Int64)(res.ok() ? res.rows() : 0);
        data["rows"]  = rows;
        RESP_OK(cb, data);
    }

    // 查询表单详情（含配置 JSON）
    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long id) {
        CHECK_PERM(req, cb, "tool:build:query");
        auto res = DatabaseService::instance().queryParams(
            "SELECT form_id,form_name,form_conf,form_fields,remark FROM tool_form_conf WHERE form_id=$1",
            {std::to_string(id)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "表单不存在"); return; }
        Json::Value j;
        j["formId"]     = (Json::Int64)res.longVal(0, 0);
        j["formName"]   = res.str(0, 1);
        j["formConf"]   = res.str(0, 2);
        j["formFields"] = res.str(0, 3);
        j["remark"]     = res.str(0, 4);
        RESP_OK(cb, j);
    }

    // 保存表单配置
    void save(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:build:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        DatabaseService::instance().exec(
            "CREATE TABLE IF NOT EXISTS tool_form_conf("
            "form_id BIGSERIAL PRIMARY KEY,form_name VARCHAR(100) NOT NULL,"
            "form_conf TEXT,form_fields TEXT,remark VARCHAR(500),"
            "create_by VARCHAR(64),create_time TIMESTAMP,"
            "update_by VARCHAR(64),update_time TIMESTAMP)");
        DatabaseService::instance().execParams(
            "INSERT INTO tool_form_conf(form_name,form_conf,form_fields,remark,create_by,create_time) "
            "VALUES($1,$2,$3,$4,$5,NOW())",
            {(*body)["formName"].asString(),
             (*body).get("formConf",   "").asString(),
             (*body).get("formFields", "").asString(),
             (*body).get("remark",     "").asString(),
             GET_USER_NAME(req)});
        RESP_MSG(cb, "保存成功");
    }

    // 更新表单配置
    void update(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:build:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        DatabaseService::instance().execParams(
            "UPDATE tool_form_conf SET form_name=$1,form_conf=$2,form_fields=$3,remark=$4,"
            "update_by=$5,update_time=NOW() WHERE form_id=$6",
            {(*body)["formName"].asString(),
             (*body).get("formConf",   "").asString(),
             (*body).get("formFields", "").asString(),
             (*body).get("remark",     "").asString(),
             GET_USER_NAME(req),
             std::to_string((*body)["formId"].asInt64())});
        RESP_MSG(cb, "更新成功");
    }

    // 删除表单
    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "tool:build:remove");
        std::istringstream ss(ids); std::string id;
        while (std::getline(ss, id, ','))
            DatabaseService::instance().execParams(
                "DELETE FROM tool_form_conf WHERE form_id=$1", {id});
        RESP_MSG(cb, "删除成功");
    }

private:
    static std::string fmtTs(const std::string& s) {
        return (s.empty() || s == "null") ? "" : s.substr(0, 19);
    }
};
