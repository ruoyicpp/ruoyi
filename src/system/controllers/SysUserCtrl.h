#pragma once
#include <drogon/HttpController.h>
#include <filesystem>
#include <set>
#include <algorithm>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../services/TokenService.h"
#include "../../common/OperLogUtils.h"
#include "../../common/DataScopeUtils.h"
#include "../../common/CsvUtils.h"

// /system/user �û������ʹ������ libpq ������
class SysUserCtrl : public drogon::HttpController<SysUserCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysUserCtrl::list,          "/system/user/list",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::getInfo,       "/system/user/",                  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::getInfoById,   "/system/user/{userId}",          drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::add,           "/system/user",                   drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::edit,          "/system/user",                   drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::remove,        "/system/user/{ids}",             drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::resetPwd,      "/system/user/resetPwd",          drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::changeStatus,  "/system/user/changeStatus",      drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::authRole,      "/system/user/authRole/{userId}", drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::insertAuthRole,"/system/user/authRole",          drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::deptTree,      "/system/user/deptTree",          drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::profile,       "/system/user/profile",           drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::updateProfile, "/system/user/profile",           drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::updatePwd,     "/system/user/profile/updatePwd", drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::exportData,    "/system/user/export",             drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::uploadAvatar,  "/system/user/profile/avatar",     drogon::Post,   "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = std::string("SELECT ") + USER_LIST_COLS +
            " FROM sys_user u LEFT JOIN sys_dept d ON u.dept_id=d.dept_id "
            "WHERE u.del_flag='0'";
        std::vector<std::string> params;
        int idx = 1;
        auto userName = req->getParameter("userName");
        auto status   = req->getParameter("status");
        auto deptId   = req->getParameter("deptId");
        if (!userName.empty()) { sql += " AND u.user_name LIKE $" + std::to_string(idx++); params.push_back("%" + userName + "%"); }
        if (!status.empty())   { sql += " AND u.status=$" + std::to_string(idx++); params.push_back(status); }
        if (!deptId.empty())   { sql += " AND (u.dept_id=$" + std::to_string(idx++) + " OR u.dept_id IN (SELECT dept_id FROM sys_dept WHERE ancestors LIKE '%'||$" + std::to_string(idx++) + "||'%'))"; params.push_back(deptId); params.push_back(deptId); }
        // ����Ȩ�޹��ˣ���Ӧ C# [DataScope]��
        sql += DATA_SCOPE_FILTER(req, "u", "u");

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY u.user_id LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) {
            for (int i = 0; i < res.rows(); ++i) rows.append(userRowToJson(res, i));
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getInfo(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:query");
        auto result = AjaxResult::successMap();
        result["roles"] = getRoleList();
        result["posts"] = getPostList();
        RESP_JSON(cb, result);
    }

    void getInfoById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long userId) {
        CHECK_PERM(req, cb, "system:user:query");
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            std::string("SELECT ") + USER_LIST_COLS +
            " FROM sys_user u LEFT JOIN sys_dept d ON u.dept_id=d.dept_id "
            "WHERE u.user_id=$1 AND u.del_flag='0'", {std::to_string(userId)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "�û�������"); return; }

        auto result = AjaxResult::successMap();
        result["data"]    = userRowToJson(res, 0);
        result["roles"]   = getRoleList();
        result["posts"]   = getPostList();
        result["roleIds"] = getUserRoleIds(userId);
        result["postIds"] = getUserPostIds(userId);
        RESP_JSON(cb, result);
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        auto& db = DatabaseService::instance();
        std::string userName = (*body)["userName"].asString();
        auto exist = db.queryParams("SELECT user_id FROM sys_user WHERE user_name=$1 AND del_flag='0' LIMIT 1", {userName});
        if (exist.ok() && exist.rows() > 0) { RESP_ERR(cb, "�����û�'" + userName + "'ʧ�ܣ���¼�˺��Ѵ���"); return; }

        std::string phonenumberCheck = (*body).get("phonenumber","").asString();
        std::string emailCheck       = (*body).get("email","").asString();
        if (!phonenumberCheck.empty()) {
            auto p = db.queryParams("SELECT user_id FROM sys_user WHERE phonenumber=$1 AND del_flag='0' LIMIT 1", {phonenumberCheck});
            if (p.ok() && p.rows() > 0) { RESP_ERR(cb, "�����û�'" + userName + "'ʧ�ܣ��ֻ������Ѵ���"); return; }
        }
        if (!emailCheck.empty()) {
            auto e = db.queryParams("SELECT user_id FROM sys_user WHERE email=$1 AND del_flag='0' LIMIT 1", {emailCheck});
            if (e.ok() && e.rows() > 0) { RESP_ERR(cb, "�����û�'" + userName + "'ʧ�ܣ������˺��Ѵ���"); return; }
        }

        std::string pwd = SecurityUtils::encryptPassword((*body).get("password","123456").asString());
        std::string deptId      = std::to_string((*body).get("deptId", 0).asInt64());
        std::string nickName    = (*body).get("nickName", userName).asString();
        std::string email       = (*body).get("email", "").asString();
        std::string phonenumber = (*body).get("phonenumber", "").asString();
        std::string sex         = (*body).get("sex", "0").asString();
        std::string status      = (*body).get("status", "0").asString();
        std::string remark      = (*body).get("remark", "").asString();
        std::string createBy    = GET_USER_NAME(req);

        auto ins = db.queryParams(
            "INSERT INTO sys_user(dept_id,user_name,nick_name,email,phonenumber,sex,"
            "password,status,del_flag,remark,create_by,create_time) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,$8,'0',$9,$10,NOW()) RETURNING user_id",
            {deptId, userName, nickName, email, phonenumber, sex, pwd, status, remark, createBy});
        if (!ins.ok() || ins.rows() == 0) { RESP_ERR(cb, "�����û�ʧ��"); return; }
        LOG_OPER(req, "�û�����", BusinessType::INSERT);
        std::string newUserId = std::to_string(ins.longVal(0, 0));

        if ((*body).isMember("roleIds"))
            for (auto &rid : (*body)["roleIds"])
                db.execParams("INSERT INTO sys_user_role(user_id,role_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                    {newUserId, std::to_string(rid.asInt64())});
        if ((*body).isMember("postIds"))
            for (auto &pid : (*body)["postIds"])
                db.execParams("INSERT INTO sys_user_post(user_id,post_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                    {newUserId, std::to_string(pid.asInt64())});
        RESP_MSG(cb, "�����ɹ�");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        long userId = (*body)["userId"].asInt64();
        if (SecurityUtils::isAdmin(userId)) { RESP_ERR(cb, "�����������������Ա�û�"); return; }
        auto& db = DatabaseService::instance();
        std::string uid      = std::to_string(userId);
        std::string userName = (*body).get("userName","").asString();
        // �û���Ψһ��
        if (!userName.empty()) {
            auto u = db.queryParams("SELECT user_id FROM sys_user WHERE user_name=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {userName, uid});
            if (u.ok() && u.rows() > 0) { RESP_ERR(cb, "�޸��û�'" + userName + "'ʧ�ܣ���¼�˺��Ѵ���"); return; }
        }
        // �ֻ���Ψһ��
        std::string editPhone = (*body).get("phonenumber","").asString();
        if (!editPhone.empty()) {
            auto p = db.queryParams("SELECT user_id FROM sys_user WHERE phonenumber=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {editPhone, uid});
            if (p.ok() && p.rows() > 0) { RESP_ERR(cb, "�޸��û�'" + userName + "'ʧ�ܣ��ֻ������Ѵ���"); return; }
        }
        // ����Ψһ��
        std::string editEmail = (*body).get("email","").asString();
        if (!editEmail.empty()) {
            auto e = db.queryParams("SELECT user_id FROM sys_user WHERE email=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {editEmail, uid});
            if (e.ok() && e.rows() > 0) { RESP_ERR(cb, "�޸��û�'" + userName + "'ʧ�ܣ������˺��Ѵ���"); return; }
        }
        db.execParams(
            "UPDATE sys_user SET nick_name=$1,email=$2,phonenumber=$3,sex=$4,"
            "status=$5,remark=$6,dept_id=$7,update_by=$8,update_time=NOW() WHERE user_id=$9",
            {(*body).get("nickName","").asString(), (*body).get("email","").asString(),
             (*body).get("phonenumber","").asString(), (*body).get("sex","0").asString(),
             (*body).get("status","0").asString(), (*body).get("remark","").asString(),
             std::to_string((*body).get("deptId",0).asInt64()), GET_USER_NAME(req), uid});

        db.execParams("DELETE FROM sys_user_role WHERE user_id=$1", {uid});
        if ((*body).isMember("roleIds"))
            for (auto &rid : (*body)["roleIds"])
                db.execParams("INSERT INTO sys_user_role(user_id,role_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                    {uid, std::to_string(rid.asInt64())});
        db.execParams("DELETE FROM sys_user_post WHERE user_id=$1", {uid});
        if ((*body).isMember("postIds"))
            for (auto &pid : (*body)["postIds"])
                db.execParams("INSERT INTO sys_user_post(user_id,post_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                    {uid, std::to_string(pid.asInt64())});
        LOG_OPER(req, "�û�����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:user:remove");
        long curUserId = GET_USER_ID(req);
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids)) {
            long uid = std::stol(idStr);
            if (uid == curUserId) { RESP_ERR(cb, "��ǰ�û�����ɾ��"); return; }
            if (SecurityUtils::isAdmin(uid)) { RESP_ERR(cb, "������ɾ����������Ա"); return; }
            std::string s = std::to_string(uid);
            db.execParams("DELETE FROM sys_user_role WHERE user_id=$1", {s});
            db.execParams("DELETE FROM sys_user_post WHERE user_id=$1", {s});
            db.execParams("UPDATE sys_user SET del_flag='2' WHERE user_id=$1", {s});
        }
        LOG_OPER_PARAM(req, "�û�����", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "�����ɹ�");
    }

    void resetPwd(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:resetPwd");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        long userId = (*body)["userId"].asInt64();
        if (SecurityUtils::isAdmin(userId)) { RESP_ERR(cb, "�����������������Ա�û�"); return; }
        std::string pwd = SecurityUtils::encryptPassword((*body)["password"].asString());
        DatabaseService::instance().execParams("UPDATE sys_user SET password=$1,update_time=NOW() WHERE user_id=$2",
            {pwd, std::to_string(userId)});
        LOG_OPER(req, "�û�����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void changeStatus(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        long userId = (*body)["userId"].asInt64();
        if (SecurityUtils::isAdmin(userId)) { RESP_ERR(cb, "�����������������Ա�û�"); return; }
        DatabaseService::instance().execParams("UPDATE sys_user SET status=$1,update_time=NOW() WHERE user_id=$2",
            {(*body)["status"].asString(), std::to_string(userId)});
        LOG_OPER(req, "�û�����", BusinessType::UPDATE);
        RESP_MSG(cb, "�����ɹ�");
    }

    void authRole(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long userId) {
        CHECK_PERM(req, cb, "system:user:query");
        auto& db = DatabaseService::instance();
        std::string uid = std::to_string(userId);
        auto uRes = db.queryParams("SELECT user_id,user_name,nick_name FROM sys_user WHERE user_id=$1", {uid});
        auto roleRes = db.queryParams(
            "SELECT r.role_id,r.role_name,r.role_key,r.status,"
            "CASE WHEN ur.user_id IS NOT NULL THEN true ELSE false END as flag "
            "FROM sys_role r LEFT JOIN sys_user_role ur ON r.role_id=ur.role_id AND ur.user_id=$1 "
            "WHERE r.del_flag='0' AND r.role_id!=1", {uid});
        Json::Value roleArr(Json::arrayValue);
        if (roleRes.ok()) {
            for (int i = 0; i < roleRes.rows(); ++i) {
                Json::Value r;
                r["roleId"]   = (Json::Int64)roleRes.longVal(i, 0);
                r["roleName"] = roleRes.str(i, 1);
                r["roleKey"]  = roleRes.str(i, 2);
                r["status"]   = roleRes.str(i, 3);
                r["flag"]     = roleRes.boolVal(i, 4);
                roleArr.append(r);
            }
        }
        auto result = AjaxResult::successMap();
        result["user"]  = (uRes.ok() && uRes.rows() > 0) ? userRowToJson(uRes, 0) : Json::Value();
        result["roles"] = roleArr;
        RESP_JSON(cb, result);
    }

    void insertAuthRole(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:edit");
        std::string uid = req->getParameter("userId");
        std::string roleIdsStr = req->getParameter("roleIds");
        auto& db = DatabaseService::instance();
        db.execParams("DELETE FROM sys_user_role WHERE user_id=$1", {uid});
        for (auto &idStr : splitIds(roleIdsStr))
            db.execParams("INSERT INTO sys_user_role(user_id,role_id) VALUES($1,$2) ON CONFLICT DO NOTHING", {uid, idStr});
        RESP_MSG(cb, "�����ɹ�");
    }

    void deptTree(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:list");
        auto& db = DatabaseService::instance();
        auto res = db.query("SELECT dept_id,parent_id,dept_name,order_num,status FROM sys_dept WHERE del_flag='0' ORDER BY parent_id,order_num");
        Json::Value tree(Json::arrayValue);
        if (res.ok()) tree = buildDeptTree(res, 0);
        RESP_OK(cb, tree);
    }

    void profile(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        auto& db = DatabaseService::instance();
        std::string uid = std::to_string(userId);
        auto uRes = db.queryParams(
            std::string("SELECT ") + USER_LIST_COLS +
            " FROM sys_user u LEFT JOIN sys_dept d ON u.dept_id=d.dept_id WHERE u.user_id=$1", {uid});
        if (!uRes.ok() || uRes.rows() == 0) { RESP_ERR(cb, "�û�������"); return; }

        auto roleRes = db.queryParams(
            "SELECT r.role_name FROM sys_role r INNER JOIN sys_user_role ur ON r.role_id=ur.role_id "
            "WHERE ur.user_id=$1 AND r.del_flag='0'", {uid});
        std::string roleGroup;
        if (roleRes.ok()) for (int i = 0; i < roleRes.rows(); ++i) {
            if (!roleGroup.empty()) roleGroup += ",";
            roleGroup += roleRes.str(i, 0);
        }

        auto postRes = db.queryParams(
            "SELECT p.post_name FROM sys_post p INNER JOIN sys_user_post up ON p.post_id=up.post_id "
            "WHERE up.user_id=$1", {uid});
        std::string postGroup;
        if (postRes.ok()) for (int i = 0; i < postRes.rows(); ++i) {
            if (!postGroup.empty()) postGroup += ",";
            postGroup += postRes.str(i, 0);
        }

        auto result = AjaxResult::successMap();
        result["data"]      = userRowToJson(uRes, 0);
        result["roleGroup"] = roleGroup;
        result["postGroup"] = postGroup;
        RESP_JSON(cb, result);
    }

    void updateProfile(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }
        auto& db = DatabaseService::instance();
        std::string uid   = std::to_string(userId);
        std::string phone = (*body).get("phonenumber","").asString();
        std::string email = (*body).get("email","").asString();
        // �ֻ���ΨһУ�飨��Ӧ C# CheckPhoneUniqueAsync��
        if (!phone.empty()) {
            auto p = db.queryParams("SELECT user_id FROM sys_user WHERE phonenumber=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {phone, uid});
            if (p.ok() && p.rows() > 0) { RESP_ERR(cb, "�޸ĸ�����Ϣʧ�ܣ��ֻ������Ѵ���"); return; }
        }
        // ����ΨһУ�飨��Ӧ C# CheckEmailUniqueAsync��
        if (!email.empty()) {
            auto e = db.queryParams("SELECT user_id FROM sys_user WHERE email=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {email, uid});
            if (e.ok() && e.rows() > 0) { RESP_ERR(cb, "�޸ĸ�����Ϣʧ�ܣ������˺��Ѵ���"); return; }
        }
        db.execParams(
            "UPDATE sys_user SET nick_name=$1,email=$2,phonenumber=$3,sex=$4,update_time=NOW() WHERE user_id=$5",
            {(*body).get("nickName","").asString(), email, phone,
             (*body).get("sex","0").asString(), uid});
        RESP_MSG(cb, "�����ɹ�");
    }

    void updatePwd(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        std::string oldPwd = req->getParameter("oldPassword");
        std::string newPwd = req->getParameter("newPassword");
        auto& db = DatabaseService::instance();
        auto res = db.queryParams("SELECT password FROM sys_user WHERE user_id=$1", {std::to_string(userId)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "�û�������"); return; }
        std::string encoded = res.str(0, 0);
        if (!SecurityUtils::matchesPassword(oldPwd, encoded)) { RESP_ERR(cb, "�޸�����ʧ�ܣ����������"); return; }
        if (SecurityUtils::matchesPassword(newPwd, encoded)) { RESP_ERR(cb, "�����벻�����������ͬ"); return; }
        std::string newEncoded = SecurityUtils::encryptPassword(newPwd);
        db.execParams("UPDATE sys_user SET password=$1,update_time=NOW() WHERE user_id=$2",
            {newEncoded, std::to_string(userId)});
        // ���� token �����е����루��Ӧ C# SetLoginUser��
        auto userOpt = TokenService::instance().getLoginUser(req);
        if (userOpt) {
            userOpt->password = newEncoded;
            TokenService::instance().setLoginUser(*userOpt);
        }
        RESP_MSG(cb, "�����ɹ�");
    }

private:
    // ��׼�û��б��ѯ�У�
    // user_id(0),dept_id(1),user_name(2),nick_name(3),email(4),phonenumber(5),
    // sex(6),avatar(7),status(8),login_ip(9),login_date(10),create_time(11),
    // remark(12),dept_name(13)
    static constexpr const char* USER_LIST_COLS =
        "u.user_id,u.dept_id,u.user_name,u.nick_name,u.email,u.phonenumber,"
        "u.sex,u.avatar,u.status,u.login_ip,u.login_date,u.create_time,"
        "u.remark,d.dept_name";

    Json::Value userRowToJson(const DatabaseService::QueryResult &res, int row) {
        int cols = res.cols();
        Json::Value j;
        if (cols < 3) return j;
        j["userId"]      = (Json::Int64)res.longVal(row, 0);
        if (cols >= 9) {
            j["deptId"]      = (Json::Int64)res.longVal(row, 1);
            j["userName"]    = res.str(row, 2);
            j["nickName"]    = res.str(row, 3);
            j["email"]       = res.str(row, 4);
            j["phonenumber"] = res.str(row, 5);
            j["sex"]         = res.str(row, 6);
            j["avatar"]      = res.str(row, 7);
            j["status"]      = res.str(row, 8);
        }
        if (cols >= 12) {
            j["loginIp"]    = res.str(row, 9);
            j["loginDate"]  = fmtTs(res.str(row, 10));
            j["createTime"] = fmtTs(res.str(row, 11));
        }
        if (cols >= 13) {
            j["remark"]     = res.str(row, 12);
        }
        if (cols >= 14) {
            Json::Value dept;
            dept["deptName"] = res.str(row, 13);
            j["dept"] = dept;
        }
        return j;
    }

    Json::Value getRoleList() {
        auto res = DatabaseService::instance().query(
            "SELECT role_id,role_name,role_key,status FROM sys_role WHERE del_flag='0' AND status='0' ORDER BY role_sort");
        Json::Value arr(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value r;
            r["roleId"]   = (Json::Int64)res.longVal(i, 0);
            r["roleName"] = res.str(i, 1);
            r["roleKey"]  = res.str(i, 2);
            r["status"]   = res.str(i, 3);
            arr.append(r);
        }
        return arr;
    }

    Json::Value getPostList() {
        auto res = DatabaseService::instance().query(
            "SELECT post_id,post_name,post_code,status FROM sys_post WHERE status='0' ORDER BY post_sort");
        Json::Value arr(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value p;
            p["postId"]   = (Json::Int64)res.longVal(i, 0);
            p["postName"] = res.str(i, 1);
            p["postCode"] = res.str(i, 2);
            p["status"]   = res.str(i, 3);
            arr.append(p);
        }
        return arr;
    }

    Json::Value getUserRoleIds(long userId) {
        auto res = DatabaseService::instance().queryParams(
            "SELECT role_id FROM sys_user_role WHERE user_id=$1", {std::to_string(userId)});
        Json::Value arr(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) arr.append((Json::Int64)res.longVal(i, 0));
        return arr;
    }

    Json::Value getUserPostIds(long userId) {
        auto res = DatabaseService::instance().queryParams(
            "SELECT post_id FROM sys_user_post WHERE user_id=$1", {std::to_string(userId)});
        Json::Value arr(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) arr.append((Json::Int64)res.longVal(i, 0));
        return arr;
    }

    void exportData(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:export");
        auto& db = DatabaseService::instance();
        std::string sql = std::string("SELECT ") + USER_LIST_COLS +
            " FROM sys_user u LEFT JOIN sys_dept d ON u.dept_id=d.dept_id WHERE u.del_flag='0'";
        sql += DATA_SCOPE_FILTER(req, "u", "u");
        sql += " ORDER BY u.user_id LIMIT 10000";
        auto res = db.query(sql);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(userRowToJson(res, i));
        LOG_OPER(req, "�û�����", BusinessType::EXPORT);
        auto csv = CsvUtils::toCsv(rows, {
            {"�û����","userId"},{"��¼����","userName"},{"�û��ǳ�","nickName"},
            {"����","deptName"},{"�ֻ�����","phonenumber"},{"����","email"},
            {"�Ա�","sex"},{"״̬","status"},{"����¼IP","loginIp"},
            {"����ʱ��","createTime"}});
        cb(CsvUtils::makeCsvResponse(csv, "user.csv"));
    }

    // POST /system/user/profile/avatar �� �ϴ�ͷ�񣨶�Ӧ C# SysProfileController.UploadAvatar��
    void uploadAvatar(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        drogon::MultiPartParser parser;
        if (parser.parse(req) != 0) { RESP_ERR(cb, "�ϴ��ļ��쳣"); return; }
        auto &files = parser.getFiles();
        if (files.empty()) { RESP_ERR(cb, "��ѡ��Ҫ�ϴ���ͷ��"); return; }
        auto &file = files[0];
        std::string ext = std::filesystem::path(file.getFileName()).extension().string();
        for (auto &c : ext) c = (char)std::tolower((unsigned char)c);
        static const std::set<std::string> allowed = {".jpg",".jpeg",".png",".gif",".bmp",".webp"};
        if (allowed.find(ext) == allowed.end()) { RESP_ERR(cb, "ֻ�����ϴ�ͼƬ�ļ�"); return; }
        std::string uploadDir = "uploads/avatar/";
        std::filesystem::create_directories(uploadDir);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::string newName = std::to_string(userId) + "_" + std::to_string(ms) + ext;
        std::string filePath = uploadDir + newName;
        file.saveAs(filePath);
        std::string imgUrl = "/profile/avatar/" + newName;
        DatabaseService::instance().execParams(
            "UPDATE sys_user SET avatar=$1,update_time=NOW() WHERE user_id=$2",
            {imgUrl, std::to_string(userId)});
        auto result = AjaxResult::successMap();
        result["imgUrl"] = imgUrl;
        RESP_JSON(cb, result);
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
