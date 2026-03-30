#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

class SysDeptCtrl : public drogon::HttpController<SysDeptCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysDeptCtrl::list,        "/system/dept/list",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDeptCtrl::excludeChild,"/system/dept/list/exclude/{id}", drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDeptCtrl::getById,     "/system/dept/{deptId}",          drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDeptCtrl::add,         "/system/dept",                   drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysDeptCtrl::edit,        "/system/dept",                   drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysDeptCtrl::remove,      "/system/dept/{deptId}",          drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dept:list");
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,create_time FROM sys_dept WHERE del_flag='0'";
        std::vector<std::string> params;
        int idx = 1;
        auto deptName = req->getParameter("deptName");
        auto status   = req->getParameter("status");
        if (!deptName.empty()) { sql += " AND dept_name LIKE $" + std::to_string(idx++); params.push_back("%" + deptName + "%"); }
        if (!status.empty())   { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }
        sql += " ORDER BY parent_id,order_num";
        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value arr(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) arr.append(deptRowToJson(res, i));
        RESP_OK(cb, arr);
    }

    void excludeChild(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long id) {
        CHECK_PERM(req, cb, "system:dept:list");
        std::string sid = std::to_string(id);
        auto res = DatabaseService::instance().queryParams(
            "SELECT dept_id,parent_id,ancestors,dept_name,order_num,status FROM sys_dept "
            "WHERE del_flag='0' AND dept_id!=$1 AND ancestors NOT LIKE '%'||$2||'%' "
            "ORDER BY parent_id,order_num", {sid, sid});
        Json::Value arr(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) arr.append(deptRowToJson(res, i));
        RESP_OK(cb, arr);
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long deptId) {
        CHECK_PERM(req, cb, "system:dept:query");
        auto res = DatabaseService::instance().queryParams(
            "SELECT dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status FROM sys_dept WHERE dept_id=$1 AND del_flag='0'",
            {std::to_string(deptId)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "���Ų�����"); return; }
        RESP_OK(cb, deptRowToJson(res, 0));
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dept:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        auto& db = DatabaseService::instance();
        std::string parentId = std::to_string((*body).get("parentId", 0).asInt64());
        std::string deptName = (*body)["deptName"].asString();
        auto exist = db.queryParams("SELECT dept_id FROM sys_dept WHERE dept_name=$1 AND parent_id=$2 AND del_flag='0' LIMIT 1", {deptName, parentId});
        if (exist.ok() && exist.rows() > 0) { RESP_ERR(cb, "��������'" + deptName + "'ʧ�ܣ����������Ѵ���"); return; }
        auto parent = db.queryParams("SELECT ancestors,status FROM sys_dept WHERE dept_id=$1", {parentId});
        if (!parent.ok() || parent.rows() == 0 || parent.str(0, 1) == "1")
            { RESP_ERR(cb, "����ͣ�ぁ���������"); return; }
        std::string ancestors = parent.str(0, 0) + "," + parentId;
        db.execParams(
            "INSERT INTO sys_dept(parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,$8,'0',$9,NOW())",
            {parentId, ancestors, deptName,
             std::to_string((*body).get("orderNum",0).asInt()),
             (*body).get("leader","").asString(), (*body).get("phone","").asString(),
             (*body).get("email","").asString(), (*body).get("status","0").asString(),
             GET_USER_NAME(req)});
        LOG_OPER(req, "���Ź���", BusinessType::INSERT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:dept:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        long deptId   = (*body)["deptId"].asInt64();
        long parentId = (*body).get("parentId", 0).asInt64();
        if (deptId == parentId) { RESP_ERR(cb, "�޸Ĳ���ʧ�ܣ��ϼ����Ų������Լ�"); return; }
        auto& db = DatabaseService::instance();
        std::string sDeptId   = std::to_string(deptId);
        std::string sParentId = std::to_string(parentId);
        // ��ȡ�ɵ� ancestors
        auto oldRes = db.queryParams("SELECT ancestors FROM sys_dept WHERE dept_id=$1 AND del_flag='0'", {sDeptId});
        if (!oldRes.ok() || oldRes.rows() == 0) { RESP_ERR(cb, "���Ų�����"); return; }
        std::string oldAncestors = oldRes.str(0, 0);
        // �����µ� ancestors�����ϼ��� ancestors + "," + ���ϼ�ID
        auto parentRes = db.queryParams("SELECT ancestors FROM sys_dept WHERE dept_id=$1", {sParentId});
        std::string parentAnc = (parentRes.ok() && parentRes.rows() > 0) ? parentRes.str(0, 0) : "0";
        std::string newAncestors = parentAnc + "," + sParentId;
        // �������������Ӳ��ŵ� ancestors����Ӧ C# UpdateDeptChildrenAsync��
        if (oldAncestors != newAncestors) {
            int oldLen = (int)oldAncestors.size();
            db.execParams(
                "UPDATE sys_dept SET ancestors = $1 || substring(ancestors, " + std::to_string(oldLen + 1) + ")"
                " WHERE del_flag='0' AND dept_id!=$2 AND ancestors LIKE $3",
                {newAncestors, sDeptId, oldAncestors + ",%"});
        }
        std::string status = (*body).get("status","0").asString();
        // ���²��ű������ ancestors��
        db.execParams(
            "UPDATE sys_dept SET parent_id=$1,ancestors=$2,dept_name=$3,order_num=$4,leader=$5,"
            "phone=$6,email=$7,status=$8,update_by=$9,update_time=NOW() WHERE dept_id=$10",
            {sParentId, newAncestors, (*body).get("deptName","").asString(),
             std::to_string((*body).get("orderNum",0).asInt()),
             (*body).get("leader","").asString(), (*body).get("phone","").asString(),
             (*body).get("email","").asString(), status,
             GET_USER_NAME(req), sDeptId});
        // �����øò��ţ�ͬʱ���������ϼ����ţ���Ӧ C# UpdateParentDeptStatusNormalAsync��
        if (status == "0" && !newAncestors.empty() && newAncestors != "0") {
            std::string inSql = "UPDATE sys_dept SET status='0' WHERE del_flag='0' AND dept_id IN (";
            std::vector<std::string> ancParams;
            std::stringstream ss(newAncestors);
            std::string tok;
            int pi = 1;
            while (std::getline(ss, tok, ',')) {
                if (!tok.empty() && tok != "0") {
                    if (pi > 1) inSql += ",";
                    inSql += "$" + std::to_string(pi++);
                    ancParams.push_back(tok);
                }
            }
            inSql += ")";
            if (!ancParams.empty()) db.execParams(inSql, ancParams);
        }
        LOG_OPER(req, "���Ź���", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long deptId) {
        CHECK_PERM(req, cb, "system:dept:remove");
        auto& db = DatabaseService::instance();
        std::string sid = std::to_string(deptId);
        auto child = db.queryParams("SELECT COUNT(*) FROM sys_dept WHERE parent_id=$1 AND del_flag='0'", {sid});
        if (child.ok() && child.rows() > 0 && child.intVal(0, 0) > 0) { RESP_ERR(cb, "�����¼����ţ�������ɾ��"); return; }
        auto users = db.queryParams("SELECT COUNT(*) FROM sys_user WHERE dept_id=$1 AND del_flag='0'", {sid});
        if (users.ok() && users.rows() > 0 && users.intVal(0, 0) > 0) { RESP_ERR(cb, "���Ŵ����û���������ɾ��"); return; }
        db.execParams("UPDATE sys_dept SET del_flag='2' WHERE dept_id=$1", {sid});
        LOG_OPER_PARAM(req, "���Ź���", BusinessType::REMOVE, sid);
        RESP_MSG(cb, "�����ɹ�");
    }

private:
    // list��: dept_id(0),parent_id(1),ancestors(2),dept_name(3),order_num(4),leader(5),phone(6),email(7),status(8),create_time(9)
    Json::Value deptRowToJson(const DatabaseService::QueryResult &res, int row) {
        Json::Value j;
        j["deptId"]    = (Json::Int64)res.longVal(row, 0);
        j["parentId"]  = (Json::Int64)res.longVal(row, 1);
        if (res.cols() > 2) j["ancestors"] = res.str(row, 2);
        j["deptName"]  = res.str(row, 3);
        j["orderNum"]  = res.intVal(row, 4);
        if (res.cols() > 5) j["leader"] = res.str(row, 5);
        if (res.cols() > 6) j["phone"]  = res.str(row, 6);
        if (res.cols() > 7) j["email"]  = res.str(row, 7);
        if (res.cols() > 8) j["status"] = res.str(row, 8);
        if (res.cols() > 9) j["createTime"] = fmtTs(res.str(row, 9));
        return j;
    }
};
