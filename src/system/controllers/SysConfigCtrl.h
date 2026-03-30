#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../services/SysConfigService.h"

class SysConfigCtrl : public drogon::HttpController<SysConfigCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysConfigCtrl::list,         "/system/config/list",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::getById,      "/system/config/{id}",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::getByKey,     "/system/config/configKey/{key}",   drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::add,          "/system/config",                   drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::edit,         "/system/config",                   drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::remove,       "/system/config/{ids}",             drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::refreshCache, "/system/config/refreshCache",      drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT config_id,config_name,config_key,config_value,config_type,create_time FROM sys_config WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto configName = req->getParameter("configName");
        auto configKey  = req->getParameter("configKey");
        auto configType = req->getParameter("configType");
        if (!configName.empty()) { sql += " AND config_name LIKE $" + std::to_string(idx++); params.push_back("%" + configName + "%"); }
        if (!configKey.empty())  { sql += " AND config_key LIKE $" + std::to_string(idx++); params.push_back("%" + configKey + "%"); }
        if (!configType.empty()) { sql += " AND config_type=$" + std::to_string(idx++); params.push_back(configType); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY config_id LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["configId"]    = res.intVal(i, 0);
            j["configName"]  = res.str(i, 1);
            j["configKey"]   = res.str(i, 2);
            j["configValue"] = res.str(i, 3);
            j["configType"]  = res.str(i, 4);
            j["createTime"]  = fmtTs(res.str(i, 5));
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int id) {
        CHECK_PERM(req, cb, "system:config:query");
        auto res = DatabaseService::instance().queryParams(
            "SELECT config_id,config_name,config_key,config_value,config_type,remark FROM sys_config WHERE config_id=$1",
            {std::to_string(id)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "���ò�����"); return; }
        Json::Value j;
        j["configId"]    = res.intVal(0, 0);
        j["configName"]  = res.str(0, 1);
        j["configKey"]   = res.str(0, 2);
        j["configValue"] = res.str(0, 3);
        j["configType"]  = res.str(0, 4);
        j["remark"]      = res.str(0, 5);
        RESP_OK(cb, j);
    }

    void getByKey(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &key) {
        auto val = SysConfigService::instance().selectConfigByKey(key);
        RESP_OK(cb, Json::Value(val));
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        std::string configKey = (*body)["configKey"].asString();
        if (!SysConfigService::instance().checkConfigKeyUnique(configKey))
            { RESP_ERR(cb, "��������'" + (*body)["configName"].asString() + "'ʧ�ܣ����������Ѵ���"); return; }
        DatabaseService::instance().execParams(
            "INSERT INTO sys_config(config_name,config_key,config_value,config_type,remark,create_by,create_time) VALUES($1,$2,$3,$4,$5,$6,NOW())",
            {(*body)["configName"].asString(), configKey, (*body)["configValue"].asString(),
             (*body).get("configType","N").asString(), (*body).get("remark","").asString(), GET_USER_NAME(req)});
        MemCache::instance().setString(Constants::SYS_CONFIG_KEY + configKey, (*body)["configValue"].asString());
        LOG_OPER(req, "��������", BusinessType::INSERT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        int configId = (*body)["configId"].asInt();
        std::string configKey = (*body)["configKey"].asString();
        if (!SysConfigService::instance().checkConfigKeyUnique(configKey, configId))
            { RESP_ERR(cb, "�޸Ĳ���ʧ�ܣ����������Ѵ���"); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_config SET config_name=$1,config_key=$2,config_value=$3,config_type=$4,remark=$5,update_by=$6,update_time=NOW() WHERE config_id=$7",
            {(*body)["configName"].asString(), configKey, (*body)["configValue"].asString(),
             (*body).get("configType","N").asString(), (*body).get("remark","").asString(),
             GET_USER_NAME(req), std::to_string(configId)});
        MemCache::instance().setString(Constants::SYS_CONFIG_KEY + configKey, (*body)["configValue"].asString());
        LOG_OPER(req, "��������", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:config:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids)) {
            auto r = db.queryParams("SELECT config_key,config_type FROM sys_config WHERE config_id=$1", {idStr});
            if (r.ok() && r.rows() > 0) {
                if (r.str(0, 1) == "Y") { RESP_ERR(cb, "���ò�������ɾ��"); return; }
                MemCache::instance().remove(Constants::SYS_CONFIG_KEY + r.str(0, 0));
            }
            db.execParams("DELETE FROM sys_config WHERE config_id=$1", {idStr});
        }
        LOG_OPER_PARAM(req, "��������", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "�����ɹ�");
    }

    void refreshCache(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:remove");
        SysConfigService::instance().resetConfigCache();
        LOG_OPER(req, "��������", BusinessType::CLEAN);
        RESP_MSG(cb, "�����ɹ�");
    }

private:
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> result;
        std::stringstream ss(ids);
        std::string item;
        while (std::getline(ss, item, ','))
            if (!item.empty()) result.push_back(item);
        return result;
    }
};
