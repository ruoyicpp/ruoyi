#pragma once
#include <drogon/HttpController.h>
#include <sstream>
#include "../../common/AjaxResult.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

// 表单构建 /tool/build  (与 Java 版一致：存储、查询拖拽表单配置)
class BuildCtrl : public drogon::HttpController<BuildCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(BuildCtrl::list,   "/tool/build/list",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(BuildCtrl::getById,"/tool/build/{id}",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(BuildCtrl::save,   "/tool/build",         drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(BuildCtrl::update, "/tool/build",         drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(BuildCtrl::remove, "/tool/build/{ids}",   drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    // 查询表单列表
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
