#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../services/SysDictService.h"

// �ֵ����� /system/dict/type
class SysDictTypeCtrl : public drogon::HttpController<SysDictTypeCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysDictTypeCtrl::list,         "/system/dict/type/list",         drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDictTypeCtrl::getById,      "/system/dict/type/{id}",         drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDictTypeCtrl::add,          "/system/dict/type",              drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysDictTypeCtrl::edit,         "/system/dict/type",              drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDictTypeCtrl::remove,       "/system/dict/type/{ids}",        drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysDictTypeCtrl::refreshCache, "/system/dict/type/refreshCache", drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysDictTypeCtrl::optionselect, "/system/dict/type/optionselect", drogon::Get,    "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dict:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT dict_id,dict_name,dict_type,status,create_time FROM sys_dict_type WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto dictName = req->getParameter("dictName");
        auto dictType = req->getParameter("dictType");
        auto status   = req->getParameter("status");
        if (!dictName.empty()) { sql += " AND dict_name LIKE $" + std::to_string(idx++); params.push_back("%" + dictName + "%"); }
        if (!dictType.empty()) { sql += " AND dict_type LIKE $" + std::to_string(idx++); params.push_back("%" + dictType + "%"); }
        if (!status.empty())   { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY dict_id LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(typeRowToJson(res, i));
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long id) {
        CHECK_PERM(req, cb, "system:dict:query");
        auto res = DatabaseService::instance().queryParams(
            "SELECT dict_id,dict_name,dict_type,status,remark FROM sys_dict_type WHERE dict_id=$1", {std::to_string(id)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "�ֵ����Ͳ�����"); return; }
        auto j = typeRowToJson(res, 0);
        if (res.cols() > 4) j["remark"] = res.str(0, 4);
        RESP_OK(cb, j);
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dict:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        auto& db = DatabaseService::instance();
        std::string dictType = (*body)["dictType"].asString();
        auto exist = db.queryParams("SELECT dict_id FROM sys_dict_type WHERE dict_type=$1 LIMIT 1", {dictType});
        if (exist.ok() && exist.rows() > 0) { RESP_ERR(cb, "�����ֵ�'" + (*body)["dictName"].asString() + "'ʧ�ܣ��ֵ������Ѵ���"); return; }
        db.execParams(
            "INSERT INTO sys_dict_type(dict_name,dict_type,status,remark,create_by,create_time) VALUES($1,$2,$3,$4,$5,NOW())",
            {(*body)["dictName"].asString(), dictType, (*body).get("status","0").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req)});
        LOG_OPER(req, "�ֵ�����", BusinessType::INSERT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dict:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        auto& db = DatabaseService::instance();
        std::string dictId = std::to_string((*body)["dictId"].asInt64());
        auto old = db.queryParams("SELECT dict_type FROM sys_dict_type WHERE dict_id=$1", {dictId});
        std::string oldType = (old.ok() && old.rows() > 0) ? old.str(0, 0) : "";
        std::string newType = (*body)["dictType"].asString();
        if (!oldType.empty() && oldType != newType)
            db.execParams("UPDATE sys_dict_data SET dict_type=$1 WHERE dict_type=$2", {newType, oldType});
        db.execParams(
            "UPDATE sys_dict_type SET dict_name=$1,dict_type=$2,status=$3,remark=$4,update_by=$5,update_time=NOW() WHERE dict_id=$6",
            {(*body)["dictName"].asString(), newType, (*body).get("status","0").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req), dictId});
        DictCache::instance().remove(oldType);
        LOG_OPER(req, "�ֵ�����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:dict:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids)) {
            auto r = db.queryParams("SELECT dict_type FROM sys_dict_type WHERE dict_id=$1", {idStr});
            if (!r.ok() || r.rows() == 0) continue;
            std::string dt = r.str(0, 0);
            auto cnt = db.queryParams("SELECT COUNT(*) FROM sys_dict_data WHERE dict_type=$1", {dt});
            if (cnt.ok() && cnt.rows() > 0 && cnt.intVal(0, 0) > 0) { RESP_ERR(cb, "�ֵ������ѷ��䣬����ɾ��"); return; }
            db.execParams("DELETE FROM sys_dict_type WHERE dict_id=$1", {idStr});
            DictCache::instance().remove(dt);
        }
        LOG_OPER_PARAM(req, "�ֵ�����", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "�����ɹ�");
    }

    void refreshCache(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dict:remove");
        SysDictService::instance().resetDictCache();
        LOG_OPER(req, "�ֵ�����", BusinessType::CLEAN);
        RESP_MSG(cb, "�����ɹ�");
    }

    void optionselect(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto res = DatabaseService::instance().query("SELECT dict_id,dict_name,dict_type,status FROM sys_dict_type ORDER BY dict_id");
        Json::Value arr(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) arr.append(typeRowToJson(res, i));
        RESP_OK(cb, arr);
    }

private:
    // list��: dict_id(0),dict_name(1),dict_type(2),status(3),create_time(4)
    Json::Value typeRowToJson(const DatabaseService::QueryResult &res, int row) {
        Json::Value j;
        j["dictId"]   = (Json::Int64)res.longVal(row, 0);
        j["dictName"] = res.str(row, 1);
        j["dictType"] = res.str(row, 2);
        j["status"]   = res.str(row, 3);
        if (res.cols() > 4) j["createTime"] = fmtTs(res.str(row, 4));
        return j;
    }
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};

// �ֵ����� /system/dict/data
class SysDictDataCtrl : public drogon::HttpController<SysDictDataCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysDictDataCtrl::list,       "/system/dict/data/list",          drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDictDataCtrl::getById,    "/system/dict/data/{dictCode}",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDictDataCtrl::getByType,  "/system/dict/data/type/{type}",   drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDictDataCtrl::add,        "/system/dict/data",               drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysDictDataCtrl::edit,       "/system/dict/data",               drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDictDataCtrl::remove,     "/system/dict/data/{ids}",         drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dict:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status FROM sys_dict_data WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto dictType  = req->getParameter("dictType");
        auto dictLabel = req->getParameter("dictLabel");
        auto status    = req->getParameter("status");
        if (!dictType.empty())  { sql += " AND dict_type=$" + std::to_string(idx++); params.push_back(dictType); }
        if (!dictLabel.empty()) { sql += " AND dict_label LIKE $" + std::to_string(idx++); params.push_back("%" + dictLabel + "%"); }
        if (!status.empty())    { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY dict_sort LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(dataRowToJson(res, i));
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long dictCode) {
        CHECK_PERM(req, cb, "system:dict:query");
        auto res = DatabaseService::instance().queryParams(
            "SELECT dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status FROM sys_dict_data WHERE dict_code=$1",
            {std::to_string(dictCode)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "�ֵ����ݲ�����"); return; }
        RESP_OK(cb, dataRowToJson(res, 0));
    }

    void getByType(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &type) {
        auto data = SysDictService::instance().selectDictDataByType(type);
        RESP_OK(cb, data);
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dict:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        std::string dictType = (*body)["dictType"].asString();
        DatabaseService::instance().execParams(
            "INSERT INTO sys_dict_data(dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,remark,create_by,create_time) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,NOW())",
            {std::to_string((*body).get("dictSort",0).asInt()),
             (*body)["dictLabel"].asString(), (*body)["dictValue"].asString(), dictType,
             (*body).get("cssClass","").asString(), (*body).get("listClass","").asString(),
             (*body).get("isDefault","N").asString(), (*body).get("status","0").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req)});
        DictCache::instance().remove(dictType);
        LOG_OPER(req, "�ֵ�����", BusinessType::INSERT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dict:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        std::string dictType = (*body)["dictType"].asString();
        DatabaseService::instance().execParams(
            "UPDATE sys_dict_data SET dict_sort=$1,dict_label=$2,dict_value=$3,dict_type=$4,"
            "css_class=$5,list_class=$6,is_default=$7,status=$8,remark=$9,update_by=$10,update_time=NOW() WHERE dict_code=$11",
            {std::to_string((*body).get("dictSort",0).asInt()),
             (*body)["dictLabel"].asString(), (*body)["dictValue"].asString(), dictType,
             (*body).get("cssClass","").asString(), (*body).get("listClass","").asString(),
             (*body).get("isDefault","N").asString(), (*body).get("status","0").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req),
             std::to_string((*body)["dictCode"].asInt64())});
        DictCache::instance().remove(dictType);
        LOG_OPER(req, "�ֵ�����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:dict:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids)) {
            auto r = db.queryParams("SELECT dict_type FROM sys_dict_data WHERE dict_code=$1", {idStr});
            if (r.ok() && r.rows() > 0) DictCache::instance().remove(r.str(0, 0));
            db.execParams("DELETE FROM sys_dict_data WHERE dict_code=$1", {idStr});
        }
        LOG_OPER_PARAM(req, "�ֵ�����", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "�����ɹ�");
    }

private:
    // ��˳��: dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status
    Json::Value dataRowToJson(const DatabaseService::QueryResult &res, int row) {
        Json::Value j;
        j["dictCode"]  = (Json::Int64)res.longVal(row, 0);
        j["dictSort"]  = res.intVal(row, 1);
        j["dictLabel"] = res.str(row, 2);
        j["dictValue"] = res.str(row, 3);
        j["dictType"]  = res.str(row, 4);
        j["cssClass"]  = res.str(row, 5);
        j["listClass"] = res.str(row, 6);
        j["isDefault"] = res.str(row, 7);
        j["status"]    = res.str(row, 8);
        return j;
    }
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};
