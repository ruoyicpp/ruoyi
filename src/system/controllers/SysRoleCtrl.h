#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../services/TokenService.h"
#include "../../common/OperLogUtils.h"
#include "../../common/CsvUtils.h"

class SysRoleCtrl : public drogon::HttpController<SysRoleCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysRoleCtrl::list,            "/system/role/list",                    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::getById,         "/system/role/{id}",                    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::add,             "/system/role",                         drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::edit,            "/system/role",                         drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::remove,          "/system/role/{ids}",                   drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::changeStatus,    "/system/role/changeStatus",            drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::dataScope,       "/system/role/dataScope",               drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::optionselect,    "/system/role/optionselect",            drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::allocatedList,   "/system/role/authUser/allocatedList",  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::unallocatedList, "/system/role/authUser/unallocatedList",drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::cancelAuthUser,  "/system/role/authUser/cancel",         drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::cancelAuthAll,   "/system/role/authUser/cancelAll",      drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::selectAll,       "/system/role/authUser/selectAll",      drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::deptTree,        "/system/role/deptTree/{roleId}",       drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysRoleCtrl::exportData,     "/system/role/export",                  drogon::Post,   "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT role_id,role_name,role_key,role_sort,data_scope,status,create_time,remark FROM sys_role WHERE del_flag='0'";
        auto roleName = req->getParameter("roleName");
        auto status   = req->getParameter("status");
        std::vector<std::string> params;
        int idx = 1;
        if (!roleName.empty()) { sql += " AND role_name LIKE $" + std::to_string(idx++); params.push_back("%" + roleName + "%"); }
        if (!status.empty())   { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY role_sort LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(roleRowToJson(res, i));
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long id) {
        CHECK_PERM(req, cb, "system:role:query");
        auto res = DatabaseService::instance().queryParams(
            "SELECT role_id,role_name,role_key,role_sort,data_scope,status,remark FROM sys_role WHERE role_id=$1 AND del_flag='0'",
            {std::to_string(id)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "��ɫ������"); return; }
        RESP_OK(cb, roleRowToJson(res, 0));
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        auto& db = DatabaseService::instance();
        std::string roleName = (*body)["roleName"].asString();
        std::string roleKey  = (*body)["roleKey"].asString();
        auto exist = db.queryParams("SELECT role_id FROM sys_role WHERE role_name=$1 AND del_flag='0' LIMIT 1", {roleName});
        if (exist.ok() && exist.rows() > 0) { RESP_ERR(cb, "������ɫ'" + roleName + "'ʧ�ܣ���ɫ�����Ѵ���"); return; }
        auto exist2 = db.queryParams("SELECT role_id FROM sys_role WHERE role_key=$1 AND del_flag='0' LIMIT 1", {roleKey});
        if (exist2.ok() && exist2.rows() > 0) { RESP_ERR(cb, "������ɫ'" + roleName + "'ʧ�ܣ���ɫȨ���Ѵ���"); return; }

        auto ins = db.queryParams(
            "INSERT INTO sys_role(role_name,role_key,role_sort,data_scope,menu_check_strictly,dept_check_strictly,status,del_flag,remark,create_by,create_time) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,'0',$8,$9,NOW()) RETURNING role_id",
            {roleName, roleKey,
             std::to_string((*body).get("roleSort",0).asInt()),
             (*body).get("dataScope","1").asString(),
             (*body).get("menuCheckStrictly",true).asBool() ? "true" : "false",
             (*body).get("deptCheckStrictly",true).asBool() ? "true" : "false",
             (*body).get("status","0").asString(),
             (*body).get("remark","").asString(),
             GET_USER_NAME(req)});
        if (!ins.ok() || ins.rows() == 0) { RESP_ERR(cb, "������ɫʧ��"); return; }
        std::string roleId = std::to_string(ins.longVal(0, 0));
        if ((*body).isMember("menuIds"))
            for (auto &mid : (*body)["menuIds"])
                db.execParams("INSERT INTO sys_role_menu(role_id,menu_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                    {roleId, std::to_string(mid.asInt64())});
        LOG_OPER(req, "��ɫ����", BusinessType::INSERT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        long roleId = (*body)["roleId"].asInt64();
        if (SecurityUtils::isAdminRole(roleId)) { RESP_ERR(cb, "�����������������Ա��ɫ"); return; }
        auto& db = DatabaseService::instance();
        std::string rid      = std::to_string(roleId);
        std::string roleName = (*body).get("roleName","").asString();
        std::string roleKey  = (*body).get("roleKey","").asString();
        // ��ɫ��Ψһ�ԣ��ų��������Ӧ C# CheckRoleNameUniqueAsync��
        auto n = db.queryParams("SELECT role_id FROM sys_role WHERE role_name=$1 AND del_flag='0' AND role_id!=$2 LIMIT 1", {roleName, rid});
        if (n.ok() && n.rows() > 0) { RESP_ERR(cb, "�޸Ľ�ɫ'" + roleName + "'ʧ�ܣ���ɫ�����Ѵ���"); return; }
        // ��ɫȨ���ַ�Ψһ�ԣ��ų��������Ӧ C# CheckRoleKeyUniqueAsync��
        auto k = db.queryParams("SELECT role_id FROM sys_role WHERE role_key=$1 AND del_flag='0' AND role_id!=$2 LIMIT 1", {roleKey, rid});
        if (k.ok() && k.rows() > 0) { RESP_ERR(cb, "�޸Ľ�ɫ'" + roleName + "'ʧ�ܣ���ɫȨ���Ѵ���"); return; }
        db.execParams(
            "UPDATE sys_role SET role_name=$1,role_key=$2,role_sort=$3,data_scope=$4,"
            "menu_check_strictly=$5,dept_check_strictly=$6,status=$7,remark=$8,update_by=$9,update_time=NOW() "
            "WHERE role_id=$10",
            {roleName, roleKey,
             std::to_string((*body).get("roleSort",0).asInt()), (*body).get("dataScope","1").asString(),
             (*body).get("menuCheckStrictly",true).asBool() ? "true" : "false",
             (*body).get("deptCheckStrictly",true).asBool() ? "true" : "false",
             (*body).get("status","0").asString(), (*body).get("remark","").asString(),
             GET_USER_NAME(req), rid});
        db.execParams("DELETE FROM sys_role_menu WHERE role_id=$1", {rid});
        if ((*body).isMember("menuIds"))
            for (auto &mid : (*body)["menuIds"])
                db.execParams("INSERT INTO sys_role_menu(role_id,menu_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                    {rid, std::to_string(mid.asInt64())});
        // ���µ�ǰ�û� token cache Ȩ�ޣ���Ӧ C# TokenService.SetLoginUser��
        auto userOpt = TokenService::instance().getLoginUser(req);
        if (userOpt && !SecurityUtils::isAdmin(userOpt->userId)) {
            auto permsRes = db.queryParams(
                "SELECT DISTINCT m.perms FROM sys_menu m "
                "JOIN sys_role_menu rm ON m.menu_id=rm.menu_id "
                "JOIN sys_user_role ur ON rm.role_id=ur.role_id "
                "WHERE ur.user_id=$1 AND m.status='0' AND m.del_flag='0' AND m.perms!='' ",
                {std::to_string(userOpt->userId)});
            userOpt->permissions.clear();
            if (permsRes.ok()) for (int i = 0; i < permsRes.rows(); ++i)
                userOpt->permissions.push_back(permsRes.str(i, 0));
            TokenService::instance().setLoginUser(*userOpt);
        }
        LOG_OPER(req, "��ɫ����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:role:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids)) {
            long rid = std::stol(idStr);
            if (SecurityUtils::isAdminRole(rid)) { RESP_ERR(cb, "������ɾ����������Ա��ɫ"); return; }
            std::string s = std::to_string(rid);
            auto cnt = db.queryParams("SELECT COUNT(*) FROM sys_user_role WHERE role_id=$1", {s});
            if (cnt.ok() && cnt.rows() > 0 && cnt.intVal(0, 0) > 0) { RESP_ERR(cb, "��ɫ�ѷ��䣬����ɾ��"); return; }
            db.execParams("DELETE FROM sys_role_menu WHERE role_id=$1", {s});
            db.execParams("DELETE FROM sys_role_dept WHERE role_id=$1", {s});
            db.execParams("UPDATE sys_role SET del_flag='2' WHERE role_id=$1", {s});
        }
        LOG_OPER_PARAM(req, "��ɫ����", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "�����ɹ�");
    }

    void changeStatus(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        long roleId = (*body)["roleId"].asInt64();
        if (SecurityUtils::isAdminRole(roleId)) { RESP_ERR(cb, "�����������������Ա��ɫ"); return; }
        DatabaseService::instance().execParams("UPDATE sys_role SET status=$1,update_time=NOW() WHERE role_id=$2",
            {(*body)["status"].asString(), std::to_string(roleId)});
        LOG_OPER(req, "��ɫ����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void dataScope(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        long roleId = (*body)["roleId"].asInt64();
        if (SecurityUtils::isAdminRole(roleId)) { RESP_ERR(cb, "�����������������Ա��ɫ"); return; }
        auto& db = DatabaseService::instance();
        std::string rid = std::to_string(roleId);
        db.execParams("UPDATE sys_role SET data_scope=$1,update_time=NOW() WHERE role_id=$2",
            {(*body).get("dataScope","1").asString(), rid});
        db.execParams("DELETE FROM sys_role_dept WHERE role_id=$1", {rid});
        if ((*body).isMember("deptIds"))
            for (auto &did : (*body)["deptIds"])
                db.execParams("INSERT INTO sys_role_dept(role_id,dept_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                    {rid, std::to_string(did.asInt64())});
        LOG_OPER(req, "��ɫ����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void optionselect(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:query");
        auto res = DatabaseService::instance().query(
            "SELECT role_id,role_name,role_key,role_sort,data_scope,status,remark FROM sys_role WHERE del_flag='0' ORDER BY role_sort");
        Json::Value arr(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) arr.append(roleRowToJson(res, i));
        RESP_OK(cb, arr);
    }

    void allocatedList(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:list");
        std::string roleId = req->getParameter("roleId");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto cntRes = db.queryParams(
            "SELECT COUNT(*) FROM sys_user u INNER JOIN sys_user_role ur ON u.user_id=ur.user_id WHERE ur.role_id=$1 AND u.del_flag='0'", {roleId});
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
        auto res = db.queryParams(
            "SELECT u.user_id,u.user_name,u.nick_name,u.email,u.phonenumber,u.status "
            "FROM sys_user u INNER JOIN sys_user_role ur ON u.user_id=ur.user_id "
            "WHERE ur.role_id=$1 AND u.del_flag='0' LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset()),
            {roleId});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value u;
            u["userId"]   = (Json::Int64)res.longVal(i, 0);
            u["userName"] = res.str(i, 1);
            u["nickName"] = res.str(i, 2);
            u["email"]    = res.str(i, 3);
            u["status"]   = res.str(i, 5);
            rows.append(u);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void unallocatedList(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:list");
        std::string roleId = req->getParameter("roleId");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto cntRes = db.queryParams(
            "SELECT COUNT(*) FROM sys_user u WHERE u.del_flag='0' AND u.user_id NOT IN (SELECT ur.user_id FROM sys_user_role ur WHERE ur.role_id=$1)", {roleId});
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
        auto res = db.queryParams(
            "SELECT u.user_id,u.user_name,u.nick_name,u.email,u.phonenumber,u.status "
            "FROM sys_user u WHERE u.del_flag='0' AND u.user_id NOT IN "
            "(SELECT ur.user_id FROM sys_user_role ur WHERE ur.role_id=$1) LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset()),
            {roleId});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value u;
            u["userId"]   = (Json::Int64)res.longVal(i, 0);
            u["userName"] = res.str(i, 1);
            u["nickName"] = res.str(i, 2);
            rows.append(u);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void cancelAuthUser(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        DatabaseService::instance().execParams("DELETE FROM sys_user_role WHERE role_id=$1 AND user_id=$2",
            {std::to_string((*body)["roleId"].asInt64()), std::to_string((*body)["userId"].asInt64())});
        LOG_OPER(req, "��ɫ����", BusinessType::GRANT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void cancelAuthAll(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:edit");
        std::string roleId = req->getParameter("roleId");
        std::string userIds = req->getParameter("userIds");
        auto& db = DatabaseService::instance();
        for (auto &uid : splitIds(userIds))
            db.execParams("DELETE FROM sys_user_role WHERE role_id=$1 AND user_id=$2", {roleId, uid});
        LOG_OPER(req, "��ɫ����", BusinessType::GRANT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void selectAll(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:edit");
        std::string roleId = req->getParameter("roleId");
        std::string userIds = req->getParameter("userIds");
        auto& db = DatabaseService::instance();
        for (auto &uid : splitIds(userIds))
            db.execParams("INSERT INTO sys_user_role(user_id,role_id) VALUES($1,$2) ON CONFLICT DO NOTHING", {uid, roleId});
        LOG_OPER(req, "��ɫ����", BusinessType::GRANT);
        RESP_MSG(cb, "�����ɹ�");
    }

    void exportData(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:role:export");
        auto res = DatabaseService::instance().query(
            "SELECT role_id,role_name,role_key,role_sort,data_scope,status,create_time,remark FROM sys_role WHERE del_flag='0' ORDER BY role_sort LIMIT 10000");
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(roleRowToJson(res, i));
        LOG_OPER(req, "��ɫ����", BusinessType::EXPORT);
        auto csv = CsvUtils::toCsv(rows, {
            {"��ɫ���","roleId"},{"��ɫ����","roleName"},{"��ɫȨ���ַ�","roleKey"},
            {"��ʾ˳��","roleSort"},{"���ݷ�Χ","dataScope"},{"״̬","status"},
            {"����ʱ��","createTime"},{"��ע","remark"}});
        cb(CsvUtils::makeCsvResponse(csv, "role.csv"));
    }

    void deptTree(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long roleId) {
        auto& db = DatabaseService::instance();
        std::string rid = std::to_string(roleId);
        auto keyRes = db.queryParams("SELECT dept_id FROM sys_role_dept WHERE role_id=$1", {rid});
        Json::Value keys(Json::arrayValue);
        if (keyRes.ok()) for (int i = 0; i < keyRes.rows(); ++i) keys.append((Json::Int64)keyRes.longVal(i, 0));

        auto deptRes = db.query("SELECT dept_id,parent_id,dept_name FROM sys_dept WHERE del_flag='0' ORDER BY parent_id,order_num");
        Json::Value tree(Json::arrayValue);
        if (deptRes.ok()) tree = buildDeptTree(deptRes, 0);

        auto result = AjaxResult::successMap();
        result["checkedKeys"] = keys;
        result["depts"]       = tree;
        RESP_JSON(cb, result);
    }

private:
    // ��˳��: role_id,role_name,role_key,role_sort,data_scope,status,...,remark
    // list��: role_id(0),role_name(1),role_key(2),role_sort(3),data_scope(4),status(5),create_time(6),remark(7)
    Json::Value roleRowToJson(const DatabaseService::QueryResult &res, int row) {
        Json::Value j;
        j["roleId"]    = (Json::Int64)res.longVal(row, 0);
        j["roleName"]  = res.str(row, 1);
        j["roleKey"]   = res.str(row, 2);
        j["roleSort"]  = res.intVal(row, 3);
        j["dataScope"] = res.str(row, 4);
        j["status"]    = res.str(row, 5);
        j["admin"]     = (res.longVal(row, 0) == 1);
        if (res.cols() > 6) j["createTime"] = fmtTs(res.str(row, 6));
        if (res.cols() > 7) j["remark"]     = res.str(row, 7);
        return j;
    }

    Json::Value buildDeptTree(const DatabaseService::QueryResult &res, long parentId) {
        Json::Value arr(Json::arrayValue);
        for (int i = 0; i < res.rows(); ++i) {
            if (res.longVal(i, 1) == parentId) {
                long deptId = res.longVal(i, 0);
                Json::Value node;
                node["id"]       = (Json::Int64)deptId;
                node["label"]    = res.str(i, 2);
                node["children"] = buildDeptTree(res, deptId);
                arr.append(node);
            }
        }
        return arr;
    }

    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> result;
        std::stringstream ss(ids);
        std::string item;
        while (std::getline(ss, item, ','))
            if (!item.empty()) result.push_back(item);
        return result;
    }
};
