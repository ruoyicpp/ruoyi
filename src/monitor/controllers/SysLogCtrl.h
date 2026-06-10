#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../../common/CsvUtils.h"

/**
 * @file SysLogCtrl.h
 * @brief 操作日志管理控制器 — 记录和查询系统操作审计日志
 * 
 * 功能概述：
 *   - 日志记录：自动记录所有用户操作
 *   - 日志查询：支持多条件查询操作日志
 *   - 日志导出：支持导出为 CSV 格式
 *   - 日志删除：支持删除指定日志
 *   - 日志清空：支持清空所有日志
 *   - 审计追踪：完整的操作审计追踪
 * 
 * 核心特性：
 *   - 自动记录：所有操作自动记录到数据库
 *   - 详细信息：记录操作者、操作时间、IP 地址、操作内容
 *   - 多条件查询：支持按操作类型、操作人、时间范围查询
 *   - 数据导出：支持 CSV 导出，便于分析
 *   - 性能监控：记录操作耗时，便于性能分析
 *   - 错误追踪：记录操作失败原因，便于问题诊断
 * 
 * API 端点：
 *   - GET /monitor/operlog/list - 获取操作日志列表
 *   - POST /monitor/operlog/export - 导出操作日志
 *   - DELETE /monitor/operlog/{ids} - 删除指定日志
 *   - DELETE /monitor/operlog/clean - 清空所有日志
 * 
 * 请求/响应示例：
 *   ```
 *   GET /monitor/operlog/list?title=用户管理&operName=admin&status=0
 *   Authorization: Bearer <JWT>
 *   
 *   响应：
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": [
 *       {
 *         "operId": 1,
 *         "title": "用户管理",
 *         "businessType": "INSERT",
 *         "method": "POST",
 *         "operName": "admin",
 *         "operUrl": "/system/user",
 *         "operIp": "192.168.1.100",
 *         "operTime": "2026-06-10 10:30:00",
 *         "costTime": 125,
 *         "status": 0,
 *         "errorMsg": ""
 *       }
 *     ],
 *     "total": 100
 *   }
 *   ```
 * 
 * 权限要求：
 *   - monitor:operlog:list - 查看操作日志
 *   - monitor:operlog:export - 导出操作日志
 *   - monitor:operlog:remove - 删除操作日志
 *   - monitor:operlog:clean - 清空操作日志
 * 
 * 配置项（config.json）：
 *   - operlog.enabled: 是否启用操作日志（默认 true）
 *   - operlog.retention_days: 日志保留天数（默认 30）
 *   - operlog.max_size: 单条日志最大大小（默认 4000 字符）
 *   - operlog.exclude_urls: 排除的 URL 列表
 * 
 * 日志字段说明：
 *   - operId：日志 ID
 *   - title：操作标题
 *   - businessType：业务类型（INSERT、UPDATE、DELETE、QUERY）
 *   - method：操作方法（GET、POST、PUT、DELETE）
 *   - operName：操作人
 *   - operUrl：操作 URL
 *   - operIp：操作 IP
 *   - operParam：操作参数
 *   - jsonResult：操作结果
 *   - status：操作状态（0 成功，1 失败）
 *   - errorMsg：错误信息
 *   - costTime：耗时（毫秒）
 * 
 * 业务类型：
 *   - INSERT：新增操作
 *   - UPDATE：修改操作
 *   - DELETE：删除操作
 *   - QUERY：查询操作
 *   - EXPORT：导出操作
 *   - IMPORT：导入操作
 *   - GRANT：授权操作
 *   - OTHER：其他操作
 * 
 * 查询条件：
 *   - title：操作标题
 *   - operName：操作人
 *   - status：操作状态
 *   - beginTime：开始时间
 *   - endTime：结束时间
 *   - businessType：业务类型
 * 
 * @see OperLogUtils - 操作日志工具
 * @see DatabaseService - 数据库服务
 * @see CsvUtils - CSV 导出工具
 */
class SysOperLogCtrl : public drogon::HttpController<SysOperLogCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysOperLogCtrl::list,       "/monitor/operlog/list",   drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysOperLogCtrl::exportData,  "/monitor/operlog/export", drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysOperLogCtrl::remove,      "/monitor/operlog/{ids}",  drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysOperLogCtrl::clean,       "/monitor/operlog/clean",  drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:operlog:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT oper_id,title,business_type,method,request_method,operator_type,oper_name,dept_name,oper_url,oper_ip,oper_location,oper_param,json_result,status,error_msg,oper_time,cost_time,before_data,after_data FROM sys_oper_log WHERE 1=1";
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
        // 字段: oper_id(0),title(1),business_type(2),method(3),request_method(4),operator_type(5)
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
            j["operTime"]      = fmtTs(res.str(i, 15));
            j["costTime"]      = (Json::Int64)res.longVal(i, 16);
            j["beforeData"]    = res.str(i, 17);   // f17 审计：变更前 JSON 快照
            j["afterData"]     = res.str(i, 18);   // f17 审计：变更后 JSON 快照（或 diff）
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void exportData(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:operlog:export");
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT oper_id,title,oper_name,request_method,oper_url,oper_ip,oper_location,status,oper_time,cost_time FROM sys_oper_log WHERE 1=1";
        std::vector<std::string> params; int idx=1;
        auto title    = req->getParameter("title");
        auto operName = req->getParameter("operName");
        auto status   = req->getParameter("status");
        if (!title.empty())    { sql += " AND title LIKE $"+std::to_string(idx++);    params.push_back("%"+title+"%"); }
        if (!operName.empty()) { sql += " AND oper_name LIKE $"+std::to_string(idx++); params.push_back("%"+operName+"%"); }
        if (!status.empty())   { sql += " AND status=$"+std::to_string(idx++);         params.push_back(status); }
        sql += " ORDER BY oper_id DESC LIMIT 10000";
        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i=0;i<res.rows();++i) {
            Json::Value j;
            j["operId"]      = res.str(i,0); j["title"]    = res.str(i,1);
            j["operName"]    = res.str(i,2); j["requestMethod"] = res.str(i,3);
            j["operUrl"]     = res.str(i,4); j["operIp"]   = res.str(i,5);
            j["operLocation"]= res.str(i,6);
            j["status"]      = res.intVal(i,7)==0 ? "成功" : "失败";
            j["operTime"]    = fmtTs(res.str(i,8));
            j["costTime"]    = res.str(i,9)+"ms";
            rows.append(j);
        }
        LOG_OPER(req, "操作日志", BusinessType::EXPORT);
        auto csv = CsvUtils::toCsv(rows, {
            {"日志编号","operId"},{"系统模块","title"},{"操作人员","operName"},
            {"请求方式","requestMethod"},{"操作地址","operUrl"},{"主机地址","operIp"},
            {"操作地点","operLocation"},{"操作状态","status"},
            {"操作时间","operTime"},{"消耗时间","costTime"}});
        cb(CsvUtils::makeCsvResponse(csv, "operlog.csv"));
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "monitor:operlog:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids))
            db.execParams("DELETE FROM sys_oper_log WHERE oper_id=$1", {idStr});
        LOG_OPER_PARAM(req, "操作日志", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "操作成功");
    }

    void clean(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:operlog:remove");
        DatabaseService::instance().exec("DELETE FROM sys_oper_log");
        LOG_OPER(req, "操作日志", BusinessType::CLEAN);
        RESP_MSG(cb, "操作成功");
    }

private:
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};

// 登录日志 /monitor/logininfor
class SysLogininforCtrl : public drogon::HttpController<SysLogininforCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysLogininforCtrl::list,       "/monitor/logininfor/list",            drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysLogininforCtrl::exportData, "/monitor/logininfor/export",           drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysLogininforCtrl::remove,     "/monitor/logininfor/{ids}",            drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysLogininforCtrl::clean,      "/monitor/logininfor/clean",            drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysLogininforCtrl::unlock,     "/monitor/logininfor/unlock/{userName}",drogon::Get,    "JwtAuthFilter");
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
        // 字段: info_id(0),user_name(1),ipaddr(2),login_location(3),browser(4),os(5),status(6)
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
            j["loginTime"]     = fmtTs(res.str(i, 8));
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void exportData(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:logininfor:export");
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT info_id,user_name,ipaddr,login_location,browser,os,status,msg,login_time FROM sys_logininfor WHERE 1=1";
        std::vector<std::string> params; int idx=1;
        auto userName = req->getParameter("userName");
        auto ipaddr   = req->getParameter("ipaddr");
        auto status   = req->getParameter("status");
        if (!userName.empty()) { sql += " AND user_name LIKE $"+std::to_string(idx++); params.push_back("%"+userName+"%"); }
        if (!ipaddr.empty())   { sql += " AND ipaddr LIKE $"+std::to_string(idx++);    params.push_back("%"+ipaddr+"%"); }
        if (!status.empty())   { sql += " AND status=$"+std::to_string(idx++);          params.push_back(status); }
        sql += " ORDER BY info_id DESC LIMIT 10000";
        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i=0;i<res.rows();++i) {
            Json::Value j;
            j["infoId"]       = res.str(i,0); j["userName"]      = res.str(i,1);
            j["ipaddr"]       = res.str(i,2); j["loginLocation"] = res.str(i,3);
            j["browser"]      = res.str(i,4); j["os"]            = res.str(i,5);
            j["status"]       = res.str(i,6)=="0" ? "成功" : "失败";
            j["msg"]          = res.str(i,7); j["loginTime"]     = fmtTs(res.str(i,8));
            rows.append(j);
        }
        LOG_OPER(req, "登录日志", BusinessType::EXPORT);
        auto csv = CsvUtils::toCsv(rows, {
            {"访问ID","infoId"},{"用户账号","userName"},{"登录地址","ipaddr"},
            {"登录地点","loginLocation"},{"浏览器","browser"},{"操作系统","os"},
            {"登录状态","status"},{"提示消息","msg"},{"访问时间","loginTime"}});
        cb(CsvUtils::makeCsvResponse(csv, "logininfor.csv"));
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "monitor:logininfor:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids))
            db.execParams("DELETE FROM sys_logininfor WHERE info_id=$1", {idStr});
        LOG_OPER_PARAM(req, "操作日志", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "操作成功");
    }

    void clean(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:logininfor:remove");
        DatabaseService::instance().exec("DELETE FROM sys_logininfor");
        LOG_OPER(req, "登录日志", BusinessType::CLEAN);
        RESP_MSG(cb, "操作成功");
    }

    void unlock(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &userName) {
        CHECK_PERM(req, cb, "monitor:logininfor:unlock");
        // 同时清除用户名维度的密码错误计数 和 IP 维度的登录失败锁
        // 避免管理员解锁后用户仍因 IP 锁无法登录
        MemCache::instance().remove(Constants::PWD_ERR_CNT_KEY + userName);
        // 查询该用户最后一次登录的 IP，并清除该 IP 的失败计数
        auto& db = DatabaseService::instance();
        auto ipRes = db.queryParams(
            "SELECT login_ip FROM sys_user WHERE user_name=$1 AND del_flag='0' LIMIT 1",
            {userName});
        if (ipRes.ok() && ipRes.rows() > 0) {
            std::string ip = ipRes.str(0, 0);
            if (!ip.empty()) MemCache::instance().remove("login:fail:ip:" + ip);
        }
        // 此外扫描 sys_logininfor 最近3条失败记录，一并清理它们的 IP 锁
        auto failRes = db.queryParams(
            "SELECT DISTINCT ipaddr FROM sys_logininfor "
            "WHERE user_name=$1 AND status='1' "
            "ORDER BY ipaddr LIMIT 5", {userName});
        if (failRes.ok())
            for (int i = 0; i < failRes.rows(); ++i) {
                std::string ip = failRes.str(i, 0);
                if (!ip.empty()) MemCache::instance().remove("login:fail:ip:" + ip);
            }
        LOG_OPER_PARAM(req, "登录日志", BusinessType::UPDATE, "解锁账户: " + userName);
        RESP_MSG(cb, "操作成功");
    }

private:
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};
