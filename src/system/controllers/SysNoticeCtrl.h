#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

class SysNoticeCtrl : public drogon::HttpController<SysNoticeCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysNoticeCtrl::list,    "/system/notice/list",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::listTop, "/system/notice/listTop", drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::getById, "/system/notice/{id}",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::add,     "/system/notice",         drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::edit,    "/system/notice",         drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::remove,  "/system/notice/{ids}",   drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notice:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT notice_id,notice_title,notice_type,status,create_by,create_time FROM sys_notice WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto title    = req->getParameter("noticeTitle");
        auto type     = req->getParameter("noticeType");
        auto createBy = req->getParameter("createBy");
        if (!title.empty())    { sql += " AND notice_title LIKE $" + std::to_string(idx++); params.push_back("%" + title + "%"); }
        if (!type.empty())     { sql += " AND notice_type=$" + std::to_string(idx++); params.push_back(type); }
        if (!createBy.empty()) { sql += " AND create_by LIKE $" + std::to_string(idx++); params.push_back("%" + createBy + "%"); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY notice_id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        // 列字段: notice_id(0),notice_title(1),notice_type(2),status(3),create_by(4),create_time(5)
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["noticeId"]    = res.intVal(i, 0);
            j["noticeTitle"] = res.str(i, 1);
            j["noticeType"]  = res.str(i, 2);
            j["status"]      = res.str(i, 3);
            j["createBy"]    = res.str(i, 4);
            j["createTime"]  = fmtTs(res.str(i, 5));
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    // 前端多通知菜单，显示最新5条公告
    void listTop(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto& db = DatabaseService::instance();
        auto res = db.query(
            "SELECT notice_id,notice_title,notice_type,status,create_by,create_time "
            "FROM sys_notice WHERE status='0' ORDER BY notice_id DESC LIMIT 5");
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["noticeId"]    = res.intVal(i, 0);
            j["noticeTitle"] = res.str(i, 1);
            j["noticeType"]  = res.str(i, 2);
            j["status"]      = res.str(i, 3);
            j["createBy"]    = res.str(i, 4);
            j["createTime"]  = fmtTs(res.str(i, 5));
            rows.append(j);
        }
        RESP_OK(cb, rows);
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int id) {
        CHECK_PERM(req, cb, "system:notice:query");
        // 列字段: notice_id(0),notice_title(1),notice_type(2),notice_content(3),status(4),remark(5)
        auto res = DatabaseService::instance().queryParams(
            "SELECT notice_id,notice_title,notice_type,notice_content,status,remark FROM sys_notice WHERE notice_id=$1",
            {std::to_string(id)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "公告不存在"); return; }
        Json::Value j;
        j["noticeId"]      = res.intVal(0, 0);
        j["noticeTitle"]   = res.str(0, 1);
        j["noticeType"]    = res.str(0, 2);
        j["noticeContent"] = res.str(0, 3);
        j["status"]        = res.str(0, 4);
        j["remark"]        = res.str(0, 5);
        RESP_OK(cb, j);
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notice:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        DatabaseService::instance().execParams(
            "INSERT INTO sys_notice(notice_title,notice_type,notice_content,status,remark,create_by,create_time) VALUES($1,$2,$3,$4,$5,$6,NOW())",
            {(*body)["noticeTitle"].asString(), (*body)["noticeType"].asString(),
             (*body).get("noticeContent","").asString(), (*body).get("status","0").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req)});
        LOG_OPER(req, "通知公告", BusinessType::INSERT);
        RESP_MSG(cb, "操作成功");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notice:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_notice SET notice_title=$1,notice_type=$2,notice_content=$3,status=$4,remark=$5,update_by=$6,update_time=NOW() WHERE notice_id=$7",
            {(*body)["noticeTitle"].asString(), (*body)["noticeType"].asString(),
             (*body).get("noticeContent","").asString(), (*body).get("status","0").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req),
             std::to_string((*body)["noticeId"].asInt())});
        LOG_OPER(req, "通知公告", BusinessType::UPDATE);
        RESP_MSG(cb, "操作成功");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:notice:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids))
            db.execParams("DELETE FROM sys_notice WHERE notice_id=$1", {idStr});
        LOG_OPER_PARAM(req, "通知公告", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "操作成功");
    }

private:
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};
