#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

// ��ʱ���� /monitor/job  (C++ �汾ʹ�� drogon ���ö�ʱ���������� Quartz)
class SysJobCtrl : public drogon::HttpController<SysJobCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysJobCtrl::list,        "/monitor/job/list",           drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysJobCtrl::getById,     "/monitor/job/{jobId}",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysJobCtrl::add,         "/monitor/job",                drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysJobCtrl::edit,        "/monitor/job",                drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysJobCtrl::remove,      "/monitor/job/{ids}",          drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysJobCtrl::changeStatus,"/monitor/job/changeStatus",   drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysJobCtrl::run,         "/monitor/job/run",            drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysJobCtrl::logList,     "/monitor/jobLog/list",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysJobCtrl::logClean,    "/monitor/jobLog/clean",       drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysJobCtrl::logRemove,   "/monitor/jobLog/{ids}",       drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:job:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT job_id,job_name,job_group,invoke_target,cron_expression,misfire_policy,concurrent,status,create_time FROM sys_job WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto jobName  = req->getParameter("jobName");
        auto jobGroup = req->getParameter("jobGroup");
        auto status   = req->getParameter("status");
        if (!jobName.empty())  { sql += " AND job_name LIKE $" + std::to_string(idx++); params.push_back("%" + jobName + "%"); }
        if (!jobGroup.empty()) { sql += " AND job_group=$" + std::to_string(idx++); params.push_back(jobGroup); }
        if (!status.empty())   { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY job_id LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(jobRowToJson(res, i));
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long jobId) {
        CHECK_PERM(req, cb, "monitor:job:query");
        auto res = DatabaseService::instance().queryParams(
            "SELECT job_id,job_name,job_group,invoke_target,cron_expression,misfire_policy,concurrent,status,remark FROM sys_job WHERE job_id=$1",
            {std::to_string(jobId)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "���񲻴���"); return; }
        auto j = jobRowToJson(res, 0);
        if (res.cols() > 8) j["remark"] = res.str(0, 8);
        RESP_OK(cb, j);
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:job:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        DatabaseService::instance().execParams(
            "INSERT INTO sys_job(job_name,job_group,invoke_target,cron_expression,misfire_policy,concurrent,status,remark,create_by,create_time) "
            "VALUES($1,$2,$3,$4,$5,$6,'1',$7,$8,NOW())",
            {(*body)["jobName"].asString(), (*body).get("jobGroup","DEFAULT").asString(),
             (*body)["invokeTarget"].asString(), (*body)["cronExpression"].asString(),
             (*body).get("misfirePolicy","3").asString(), (*body).get("concurrent","1").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req)});
        LOG_OPER(req, "��ʱ����", BusinessType::INSERT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:job:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_job SET job_name=$1,job_group=$2,invoke_target=$3,cron_expression=$4,"
            "misfire_policy=$5,concurrent=$6,remark=$7,update_by=$8,update_time=NOW() WHERE job_id=$9",
            {(*body)["jobName"].asString(), (*body).get("jobGroup","DEFAULT").asString(),
             (*body)["invokeTarget"].asString(), (*body)["cronExpression"].asString(),
             (*body).get("misfirePolicy","3").asString(), (*body).get("concurrent","1").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req),
             std::to_string((*body)["jobId"].asInt64())});
        LOG_OPER(req, "��ʱ����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "monitor:job:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids))
            db.execParams("DELETE FROM sys_job WHERE job_id=$1", {idStr});
        LOG_OPER_PARAM(req, "��ʱ����", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "�����ɹ�");
    }

    void changeStatus(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:job:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_job SET status=$1,update_time=NOW() WHERE job_id=$2",
            {(*body)["status"].asString(), std::to_string((*body)["jobId"].asInt64())});
        LOG_OPER(req, "��ʱ����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void run(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:job:changeStatus");
        // C++ �汾����¼һ���ֶ�������־
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        auto& db = DatabaseService::instance();
        auto r = db.queryParams("SELECT job_name,job_group,invoke_target FROM sys_job WHERE job_id=$1",
            {std::to_string((*body)["jobId"].asInt64())});
        if (!r.ok() || r.rows() == 0) { RESP_ERR(cb, "���񲻴���"); return; }
        db.execParams(
            "INSERT INTO sys_job_log(job_name,job_group,invoke_target,job_message,status,create_time) VALUES($1,$2,$3,'�ֶ�����','0',NOW())",
            {r.str(0, 0), r.str(0, 1), r.str(0, 2)});
        LOG_OPER(req, "��ʱ����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void logList(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:job:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT job_log_id,job_name,job_group,invoke_target,job_message,status,exception_info,create_time FROM sys_job_log WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto jobName = req->getParameter("jobName");
        auto status  = req->getParameter("status");
        if (!jobName.empty()) { sql += " AND job_name LIKE $" + std::to_string(idx++); params.push_back("%" + jobName + "%"); }
        if (!status.empty())  { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY job_log_id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        // ��: job_log_id(0),job_name(1),job_group(2),invoke_target(3),job_message(4),status(5),exception_info(6),create_time(7)
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["jobLogId"]      = (Json::Int64)res.longVal(i, 0);
            j["jobName"]       = res.str(i, 1);
            j["jobGroup"]      = res.str(i, 2);
            j["invokeTarget"]  = res.str(i, 3);
            j["jobMessage"]    = res.str(i, 4);
            j["status"]        = res.str(i, 5);
            j["exceptionInfo"] = res.str(i, 6);
            j["createTime"]    = fmtTs(res.str(i, 7));
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void logClean(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:job:remove");
        DatabaseService::instance().exec("DELETE FROM sys_job_log");
        LOG_OPER(req, "��ʱ������־", BusinessType::CLEAN);
        RESP_MSG(cb, "�����ɹ�");
    }

    void logRemove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "monitor:job:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids))
            db.execParams("DELETE FROM sys_job_log WHERE job_log_id=$1", {idStr});
        RESP_MSG(cb, "�����ɹ�");
    }

private:
    // ��˳��: job_id(0),job_name(1),job_group(2),invoke_target(3),cron_expression(4),misfire_policy(5),concurrent(6),status(7),create_time(8)
    Json::Value jobRowToJson(const DatabaseService::QueryResult &res, int row) {
        Json::Value j;
        j["jobId"]          = (Json::Int64)res.longVal(row, 0);
        j["jobName"]        = res.str(row, 1);
        j["jobGroup"]       = res.str(row, 2);
        j["invokeTarget"]   = res.str(row, 3);
        j["cronExpression"] = res.str(row, 4);
        j["misfirePolicy"]  = res.str(row, 5);
        j["concurrent"]     = res.str(row, 6);
        j["status"]         = res.str(row, 7);
        if (res.cols() > 8) j["createTime"] = fmtTs(res.str(row, 8));
        return j;
    }
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};
