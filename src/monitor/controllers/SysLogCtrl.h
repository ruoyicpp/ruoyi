#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

// ������־ /monitor/operlog
class SysOperLogCtrl : public drogon::HttpController<SysOperLogCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysOperLogCtrl::list,  "/monitor/operlog/list",  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysOperLogCtrl::remove,"/monitor/operlog/{ids}", drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysOperLogCtrl::clean, "/monitor/operlog/clean", drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:operlog:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT oper_id,title,business_type,method,request_method,operator_type,oper_name,dept_name,oper_url,oper_ip,oper_location,oper_param,json_result,status,error_msg,oper_time,cost_time FROM sys_oper_log WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto title     = req->getParameter("title");
        auto operName  = req->getParameter("operName");
        auto status    = req->getParameter("status");
        auto beginTime = req->getParameter("params[beginTime]");
        auto endTime   = req->getParameter("params[endTime]");
        if (!title.empty())     { sql += " AND title LIKE $" + std::to_string(idx++); params.push_back("%" + title + "%"); }
        if (!operName.empty())  { sql += " AND oper_name LIKE $" + std::to_string(idx++); params.push_back("%" + operName + "%"); }
        if (!status.empty())    { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }
        if (!beginTime.empty()) { sql += " AND oper_time >= $" + std::to_string(idx++); params.push_back(beginTime); }
        if (!endTime.empty())   { sql += " AND oper_time <= $" + std::to_string(idx++); params.push_back(endTime); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY oper_id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        // ��: oper_id(0),title(1),business_type(2),method(3),request_method(4),operator_type(5),
        //     oper_name(6),dept_name(7),oper_url(8),oper_ip(9),oper_location(10),oper_param(11),
        //     json_result(12),status(13),error_msg(14),oper_time(15),cost_time(16)
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["operId"]        = (Json::Int64)res.longVal(i, 0);
            j["title"]         = res.str(i, 1);
            j["businessType"]  = res.intVal(i, 2);
            j["method"]        = res.str(i, 3);
            j["requestMethod"] = res.str(i, 4);
            j["operatorType"]  = res.intVal(i, 5);
            j["operName"]      = res.str(i, 6);
            j["deptName"]      = res.str(i, 7);
            j["operUrl"]       = res.str(i, 8);
            j["operIp"]        = res.str(i, 9);
            j["operLocation"]  = res.str(i, 10);
            j["operParam"]     = res.str(i, 11);
            j["jsonResult"]    = res.str(i, 12);
            j["status"]        = res.intVal(i, 13);
            j["errorMsg"]      = res.str(i, 14);
            j["operTime"]      = res.str(i, 15);
            j["costTime"]      = (Json::Int64)res.longVal(i, 16);
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "monitor:operlog:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids))
            db.execParams("DELETE FROM sys_oper_log WHERE oper_id=$1", {idStr});
        LOG_OPER_PARAM(req, "������־", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "�����ɹ�");
    }

    void clean(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:operlog:remove");
        DatabaseService::instance().exec("DELETE FROM sys_oper_log");
        LOG_OPER(req, "������־", BusinessType::CLEAN);
        RESP_MSG(cb, "�����ɹ�");
    }

private:
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};

// ��¼��־ /monitor/logininfor
class SysLogininforCtrl : public drogon::HttpController<SysLogininforCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysLogininforCtrl::list,  "/monitor/logininfor/list",  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysLogininforCtrl::remove,"/monitor/logininfor/{ids}", drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysLogininforCtrl::clean, "/monitor/logininfor/clean", drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysLogininforCtrl::unlock,"/monitor/logininfor/unlock/{userName}", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:logininfor:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT info_id,user_name,ipaddr,login_location,browser,os,status,msg,login_time FROM sys_logininfor WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto userName  = req->getParameter("userName");
        auto ipaddr    = req->getParameter("ipaddr");
        auto status    = req->getParameter("status");
        auto beginTime = req->getParameter("params[beginTime]");
        auto endTime   = req->getParameter("params[endTime]");
        if (!userName.empty())  { sql += " AND user_name LIKE $" + std::to_string(idx++); params.push_back("%" + userName + "%"); }
        if (!ipaddr.empty())    { sql += " AND ipaddr LIKE $" + std::to_string(idx++); params.push_back("%" + ipaddr + "%"); }
        if (!status.empty())    { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }
        if (!beginTime.empty()) { sql += " AND login_time >= $" + std::to_string(idx++); params.push_back(beginTime); }
        if (!endTime.empty())   { sql += " AND login_time <= $" + std::to_string(idx++); params.push_back(endTime); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY info_id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        // ��: info_id(0),user_name(1),ipaddr(2),login_location(3),browser(4),os(5),status(6),msg(7),login_time(8)
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["infoId"]        = (Json::Int64)res.longVal(i, 0);
            j["userName"]      = res.str(i, 1);
            j["ipaddr"]        = res.str(i, 2);
            j["loginLocation"] = res.str(i, 3);
            j["browser"]       = res.str(i, 4);
            j["os"]            = res.str(i, 5);
            j["status"]        = res.str(i, 6);
            j["msg"]           = res.str(i, 7);
            j["loginTime"]     = res.str(i, 8);
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "monitor:logininfor:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids))
            db.execParams("DELETE FROM sys_logininfor WHERE info_id=$1", {idStr});
        LOG_OPER_PARAM(req, "��¼��־", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "�����ɹ�");
    }

    void clean(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:logininfor:remove");
        DatabaseService::instance().exec("DELETE FROM sys_logininfor");
        LOG_OPER(req, "��¼��־", BusinessType::CLEAN);
        RESP_MSG(cb, "�����ɹ�");
    }

    void unlock(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &userName) {
        CHECK_PERM(req, cb, "monitor:logininfor:unlock");
        MemCache::instance().remove(Constants::PWD_ERR_CNT_KEY + userName);
        RESP_MSG(cb, "�����ɹ�");
    }

private:
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};
