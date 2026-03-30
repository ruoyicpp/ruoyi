#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

class SysPostCtrl : public drogon::HttpController<SysPostCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysPostCtrl::list,        "/system/post/list",         drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysPostCtrl::getById,     "/system/post/{id}",         drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysPostCtrl::add,         "/system/post",              drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysPostCtrl::edit,        "/system/post",              drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysPostCtrl::remove,      "/system/post/{ids}",        drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysPostCtrl::optionselect,"/system/post/optionselect", drogon::Get,    "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:post:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT post_id,post_code,post_name,post_sort,status,create_time FROM sys_post WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto postCode = req->getParameter("postCode");
        auto postName = req->getParameter("postName");
        auto status   = req->getParameter("status");
        if (!postCode.empty()) { sql += " AND post_code LIKE $" + std::to_string(idx++); params.push_back("%" + postCode + "%"); }
        if (!postName.empty()) { sql += " AND post_name LIKE $" + std::to_string(idx++); params.push_back("%" + postName + "%"); }
        if (!status.empty())   { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY post_sort LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(postRowToJson(res, i));
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long id) {
        CHECK_PERM(req, cb, "system:post:query");
        auto res = DatabaseService::instance().queryParams(
            "SELECT post_id,post_code,post_name,post_sort,status,remark FROM sys_post WHERE post_id=$1",
            {std::to_string(id)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "��λ������"); return; }
        auto j = postRowToJson(res, 0);
        if (res.cols() > 5) j["remark"] = res.str(0, 5);
        RESP_OK(cb, j);
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:post:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        auto& db = DatabaseService::instance();
        std::string postName = (*body)["postName"].asString();
        std::string postCode = (*body)["postCode"].asString();
        auto e1 = db.queryParams("SELECT post_id FROM sys_post WHERE post_name=$1 LIMIT 1", {postName});
        if (e1.ok() && e1.rows() > 0) { RESP_ERR(cb, "������λ'" + postName + "'ʧ�ܣ���λ�����Ѵ���"); return; }
        auto e2 = db.queryParams("SELECT post_id FROM sys_post WHERE post_code=$1 LIMIT 1", {postCode});
        if (e2.ok() && e2.rows() > 0) { RESP_ERR(cb, "������λ'" + postName + "'ʧ�ܣ���λ�����Ѵ���"); return; }
        db.execParams(
            "INSERT INTO sys_post(post_code,post_name,post_sort,status,remark,create_by,create_time) VALUES($1,$2,$3,$4,$5,$6,NOW())",
            {postCode, postName, std::to_string((*body).get("postSort",0).asInt()),
             (*body).get("status","0").asString(), (*body).get("remark","").asString(), GET_USER_NAME(req)});
        LOG_OPER(req, "��λ����", BusinessType::INSERT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:post:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_post SET post_code=$1,post_name=$2,post_sort=$3,status=$4,remark=$5,update_by=$6,update_time=NOW() WHERE post_id=$7",
            {(*body)["postCode"].asString(), (*body)["postName"].asString(),
             std::to_string((*body).get("postSort",0).asInt()), (*body).get("status","0").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req),
             std::to_string((*body)["postId"].asInt64())});
        LOG_OPER(req, "��λ����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:post:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids)) {
            auto cnt = db.queryParams("SELECT COUNT(*) FROM sys_user_post WHERE post_id=$1", {idStr});
            if (cnt.ok() && cnt.rows() > 0 && cnt.intVal(0, 0) > 0) { RESP_ERR(cb, "��λ�ѷ��䣬����ɾ��"); return; }
            db.execParams("DELETE FROM sys_post WHERE post_id=$1", {idStr});
        }
        LOG_OPER_PARAM(req, "��λ����", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "�����ɹ�");
    }

    void optionselect(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto res = DatabaseService::instance().query(
            "SELECT post_id,post_code,post_name,post_sort,status FROM sys_post WHERE status='0' ORDER BY post_sort");
        Json::Value arr(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) arr.append(postRowToJson(res, i));
        RESP_OK(cb, arr);
    }

private:
    // list��: post_id(0),post_code(1),post_name(2),post_sort(3),status(4),create_time(5)
    Json::Value postRowToJson(const DatabaseService::QueryResult &res, int row) {
        Json::Value j;
        j["postId"]   = (Json::Int64)res.longVal(row, 0);
        j["postCode"] = res.str(row, 1);
        j["postName"] = res.str(row, 2);
        j["postSort"] = res.intVal(row, 3);
        j["status"]   = res.str(row, 4);
        if (res.cols() > 5) j["createTime"] = fmtTs(res.str(row, 5));
        return j;
    }
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};
