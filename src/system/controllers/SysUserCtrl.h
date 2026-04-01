#pragma once
#include <drogon/HttpController.h>
#include <filesystem>
#include <sstream>
#include <set>
#include <algorithm>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../services/TokenService.h"
#include "../../common/TokenCache.h"
#include "../../common/OperLogUtils.h"
#include "../../common/DataScopeUtils.h"
#include "../../common/CsvUtils.h"
#include "../services/SysPasswordService.h"
#include "SysLoginCtrl.h"

// /system/user 用户管理，使用直接 libpq 查询
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
        ADD_METHOD_TO(SysUserCtrl::exportData,      "/system/user/export",             drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::importData,      "/system/user/importData",         drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::importTemplate,  "/system/user/importTemplate",     drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::uploadAvatar,    "/system/user/profile/avatar",     drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysUserCtrl::unlock,          "/system/user/unlock/{userName}",  drogon::Delete, "JwtAuthFilter");
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
        // 数据权限过滤（对应 C# [DataScope]）
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
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "用户不存在"); return; }

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
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        auto& db = DatabaseService::instance();
        std::string userName = (*body)["userName"].asString();
        auto exist = db.queryParams("SELECT user_id FROM sys_user WHERE user_name=$1 AND del_flag='0' LIMIT 1", {userName});
        if (exist.ok() && exist.rows() > 0) { RESP_ERR(cb, "新增用户'" + userName + "'失败，登录账号已存在"); return; }

        std::string phonenumberCheck = (*body).get("phonenumber","").asString();
        std::string emailCheck       = (*body).get("email","").asString();
        if (!phonenumberCheck.empty()) {
            auto p = db.queryParams("SELECT user_id FROM sys_user WHERE phonenumber=$1 AND del_flag='0' LIMIT 1", {phonenumberCheck});
            if (p.ok() && p.rows() > 0) { RESP_ERR(cb, "新增用户'" + userName + "'失败，手机号码已存在"); return; }
        }
        if (!emailCheck.empty()) {
            auto e = db.queryParams("SELECT user_id FROM sys_user WHERE email=$1 AND del_flag='0' LIMIT 1", {emailCheck});
            if (e.ok() && e.rows() > 0) { RESP_ERR(cb, "新增用户'" + userName + "'失败，邮箱账号已存在"); return; }
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
        if (!ins.ok() || ins.rows() == 0) { RESP_ERR(cb, "新增用户失败"); return; }
        LOG_OPER(req, "用户管理", BusinessType::INSERT);
        std::string newUserId = std::to_string(ins.longVal(0, 0));

        if ((*body).isMember("roleIds"))
            for (auto &rid : (*body)["roleIds"])
                db.execParams("INSERT INTO sys_user_role(user_id,role_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                    {newUserId, std::to_string(rid.asInt64())});
        if ((*body).isMember("postIds"))
            for (auto &pid : (*body)["postIds"])
                db.execParams("INSERT INTO sys_user_post(user_id,post_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                    {newUserId, std::to_string(pid.asInt64())});
        RESP_MSG(cb, "操作成功");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        long userId = (*body)["userId"].asInt64();
        if (SecurityUtils::isAdmin(userId)) { RESP_ERR(cb, "不允许操作超级管理员用户"); return; }
        auto& db = DatabaseService::instance();
        std::string uid      = std::to_string(userId);
        std::string userName = (*body).get("userName","").asString();
        // 用户名唯一性
        if (!userName.empty()) {
            auto u = db.queryParams("SELECT user_id FROM sys_user WHERE user_name=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {userName, uid});
            if (u.ok() && u.rows() > 0) { RESP_ERR(cb, "修改用户'" + userName + "'失败，登录账号已存在"); return; }
        }
        // 手机号唯一性
        std::string editPhone = (*body).get("phonenumber","").asString();
        if (!editPhone.empty()) {
            auto p = db.queryParams("SELECT user_id FROM sys_user WHERE phonenumber=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {editPhone, uid});
            if (p.ok() && p.rows() > 0) { RESP_ERR(cb, "修改用户'" + userName + "'失败，手机号码已存在"); return; }
        }
        // 邮箱唯一性
        std::string editEmail = (*body).get("email","").asString();
        if (!editEmail.empty()) {
            auto e = db.queryParams("SELECT user_id FROM sys_user WHERE email=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {editEmail, uid});
            if (e.ok() && e.rows() > 0) { RESP_ERR(cb, "修改用户'" + userName + "'失败，邮箱账号已存在"); return; }
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
        LOG_OPER(req, "用户管理", BusinessType::UPDATE);
        RESP_MSG(cb, "操作成功");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:user:remove");
        long curUserId = GET_USER_ID(req);
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids)) {
            long uid = std::stol(idStr);
            if (uid == curUserId) { RESP_ERR(cb, "当前用户不能删除"); return; }
            if (SecurityUtils::isAdmin(uid)) { RESP_ERR(cb, "不允许删除超级管理员"); return; }
            std::string s = std::to_string(uid);
            db.execParams("DELETE FROM sys_user_role WHERE user_id=$1", {s});
            db.execParams("DELETE FROM sys_user_post WHERE user_id=$1", {s});
            db.execParams("UPDATE sys_user SET del_flag='2' WHERE user_id=$1", {s});
        }
        LOG_OPER_PARAM(req, "用户管理", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "操作成功");
    }

    void resetPwd(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:resetPwd");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        long userId = (*body)["userId"].asInt64();
        if (SecurityUtils::isAdmin(userId)) { RESP_ERR(cb, "不允许操作超级管理员用户"); return; }
        std::string pwd = SecurityUtils::encryptPassword((*body)["password"].asString());
        DatabaseService::instance().execParams("UPDATE sys_user SET password=$1,update_time=NOW() WHERE user_id=$2",
            {pwd, std::to_string(userId)});
        LOG_OPER(req, "用户管理", BusinessType::UPDATE);
        RESP_MSG(cb, "操作成功");
    }

    void changeStatus(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        long userId = (*body)["userId"].asInt64();
        if (SecurityUtils::isAdmin(userId)) { RESP_ERR(cb, "不允许操作超级管理员用户"); return; }
        DatabaseService::instance().execParams("UPDATE sys_user SET status=$1,update_time=NOW() WHERE user_id=$2",
            {(*body)["status"].asString(), std::to_string(userId)});
        LOG_OPER(req, "用户管理", BusinessType::UPDATE);
        RESP_MSG(cb, "操作成功");
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
        auto ids = splitIds(roleIdsStr);
        if (ids.size() > 1) { RESP_ERR(cb, "每个用户只能分配一个角色"); return; }
        auto& db = DatabaseService::instance();
        db.execParams("DELETE FROM sys_user_role WHERE user_id=$1", {uid});
        if (!ids.empty())
            db.execParams("INSERT INTO sys_user_role(user_id,role_id) VALUES($1,$2) ON CONFLICT DO NOTHING", {uid, ids[0]});
        // 刷新该用户的 token 权限缓存
        long userId = std::stol(uid);
        if (!SecurityUtils::isAdmin(userId)) {
            auto permsRes = db.queryParams(
                "SELECT DISTINCT m.perms FROM sys_menu m "
                "JOIN sys_role_menu rm ON m.menu_id=rm.menu_id "
                "JOIN sys_user_role ur ON rm.role_id=ur.role_id "
                "WHERE ur.user_id=$1 AND m.status='0' AND m.perms IS NOT NULL AND m.perms!=''",
                {uid});
            auto roleRes = db.queryParams(
                "SELECT r.role_key FROM sys_role r "
                "JOIN sys_user_role ur ON r.role_id=ur.role_id "
                "WHERE ur.user_id=$1 AND r.status='0' AND r.del_flag='0'", {uid});
            auto allTokens = db.query("SELECT token_key FROM sys_token");
            if (allTokens.ok()) {
                for (int t = 0; t < allTokens.rows(); ++t) {
                    std::string key = allTokens.str(t, 0);
                    auto cu = TokenCache::instance().get(key);
                    if (!cu || cu->userId != userId) continue;
                    cu->permissions.clear(); cu->roles.clear();
                    if (permsRes.ok()) for (int p = 0; p < permsRes.rows(); ++p) cu->permissions.push_back(permsRes.str(p, 0));
                    if (roleRes.ok())  for (int r = 0; r < roleRes.rows();  ++r) cu->roles.push_back(roleRes.str(r, 0));
                    TokenCache::instance().update(key, *cu);
                }
            }
            MemCache::instance().remove("routers:" + uid);
        }
        LOG_OPER(req, "用户管理", BusinessType::UPDATE);
        RESP_MSG(cb, "操作成功");
    }

    void deptTree(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:list");
        auto& db = DatabaseService::instance();
        auto res = db.query("SELECT dept_id,parent_id,dept_name,order_num,status FROM sys_dept WHERE del_flag='0' ORDER BY parent_id,order_num");
        Json::Value tree(Json::arrayValue);
        if (res.ok()) { std::set<long> vis; tree = buildDeptTree(res, 0, vis); }
        RESP_OK(cb, tree);
    }

    void profile(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        auto& db = DatabaseService::instance();
        std::string uid = std::to_string(userId);
        auto uRes = db.queryParams(
            std::string("SELECT ") + USER_LIST_COLS +
            " FROM sys_user u LEFT JOIN sys_dept d ON u.dept_id=d.dept_id WHERE u.user_id=$1", {uid});
        if (!uRes.ok() || uRes.rows() == 0) { RESP_ERR(cb, "用户不存在"); return; }

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
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        auto& db = DatabaseService::instance();
        std::string uid   = std::to_string(userId);
        std::string phone = (*body).get("phonenumber","").asString();
        std::string email = (*body).get("email","").asString();
        // 手机号唯一性校验（对应 C# CheckPhoneUniqueAsync）
        if (!phone.empty()) {
            auto p = db.queryParams("SELECT user_id FROM sys_user WHERE phonenumber=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {phone, uid});
            if (p.ok() && p.rows() > 0) { RESP_ERR(cb, "修改个人信息失败，手机号码已存在"); return; }
        }
        // 邮箱唯一性校验（对应 C# CheckEmailUniqueAsync）
        if (!email.empty()) {
            auto e = db.queryParams("SELECT user_id FROM sys_user WHERE email=$1 AND del_flag='0' AND user_id!=$2 LIMIT 1", {email, uid});
            if (e.ok() && e.rows() > 0) { RESP_ERR(cb, "修改个人信息失败，邮箱账号已存在"); return; }
        }
        db.execParams(
            "UPDATE sys_user SET nick_name=$1,email=$2,phonenumber=$3,sex=$4,update_time=NOW() WHERE user_id=$5",
            {(*body).get("nickName","").asString(), email, phone,
             (*body).get("sex","0").asString(), uid});
        RESP_MSG(cb, "操作成功");
    }

    void updatePwd(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        std::string oldPwd = req->getParameter("oldPassword");
        std::string newPwd = req->getParameter("newPassword");
        // 前端可能以 JSON body 发送（PUT + Content-Type: application/json）
        if (oldPwd.empty()) {
            auto j = req->getJsonObject();
            if (j) {
                if ((*j)["oldPassword"].isString()) oldPwd = (*j)["oldPassword"].asString();
                if ((*j)["newPassword"].isString()) newPwd = (*j)["newPassword"].asString();
            }
        }
        auto& db = DatabaseService::instance();
        auto res = db.queryParams("SELECT password FROM sys_user WHERE user_id=$1", {std::to_string(userId)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "用户不存在"); return; }
        std::string encoded = res.str(0, 0);
        if (!SecurityUtils::matchesPassword(oldPwd, encoded)) { RESP_ERR(cb, "修改密码失败，旧密码错误"); return; }
        if (SecurityUtils::matchesPassword(newPwd, encoded)) { RESP_ERR(cb, "新密码不能与旧密码相同"); return; }
        { auto err = SysLoginCtrl::checkPasswordComplexity(newPwd); if (!err.empty()) { RESP_ERR(cb, err); return; } }
        std::string newEncoded = SecurityUtils::encryptPassword(newPwd);
        db.execParams("UPDATE sys_user SET password=$1,update_time=NOW() WHERE user_id=$2",
            {newEncoded, std::to_string(userId)});
        // 更新 token 缓存中的密码（对应 C# SetLoginUser）
        auto userOpt = TokenService::instance().getLoginUser(req);
        if (userOpt) {
            userOpt->password = newEncoded;
            TokenService::instance().setLoginUser(*userOpt);
        }
        RESP_MSG(cb, "操作成功");
    }

private:
        // 读取当前在线 token 缓存并更新密码字段
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

    // POST /system/user/importTemplate — 下载导入用户 CSV 模板
    void importTemplate(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:import");
        std::string csv = "\xEF\xBB\xBF"; // UTF-8 BOM
        csv += "登录账号,用户昵称,部门编号,手机号码,邮箱,性别(0男1女2未知),状态(0正常1停用)\n";
        csv += "test01,测试用户,100,13800000001,test@example.com,0,0\n";
        cb(CsvUtils::makeCsvResponse(csv, "user_import_template.csv"));
    }

    // POST /system/user/importData — 导入用户 CSV（multipart file）
    void importData(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:user:import");
        bool updateSupport = req->getParameter("updateSupport") == "1"
                          || req->getParameter("updateSupport") == "true";
        drogon::MultiPartParser parser;
        if (parser.parse(req) != 0) { RESP_ERR(cb, "上传文件解析失败"); return; }
        auto &files = parser.getFiles();
        if (files.empty()) { RESP_ERR(cb, "请选择要导入的文件"); return; }

        // 读取文件内容
        const auto &file = files[0];
        std::string ext = std::filesystem::path(file.getFileName()).extension().string();
        for (auto &c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext != ".csv") { RESP_ERR(cb, "只支持 CSV 格式文件"); return; }

        std::string content(file.fileData(), file.fileLength());
        // 去除 UTF-8 BOM
        if (content.size() >= 3 &&
            (unsigned char)content[0] == 0xEF &&
            (unsigned char)content[1] == 0xBB &&
            (unsigned char)content[2] == 0xBF)
            content = content.substr(3);

        std::istringstream ss(content);
        std::string line;
        int success = 0, fail = 0;
        std::string errMsg;
        bool firstLine = true;
        auto& db = DatabaseService::instance();

        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if (firstLine) { firstLine = false; continue; } // 跳过表头

            // 按逗号分割（简单实现，不处理引号内逗号）
            std::vector<std::string> cols;
            std::stringstream ls(line);
            std::string col;
            while (std::getline(ls, col, ',')) cols.push_back(col);

            if (cols.size() < 2) { fail++; errMsg += "行格式错误；"; continue; }
            auto trim = [](std::string s) {
                while (!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin());
                while (!s.empty() && (s.back()==' '||s.back()=='\t')) s.pop_back();
                return s;
            };
            std::string userName    = trim(cols.size() > 0 ? cols[0] : "");
            std::string nickName    = trim(cols.size() > 1 ? cols[1] : userName);
            std::string deptId      = trim(cols.size() > 2 ? cols[2] : "");
            std::string phone       = trim(cols.size() > 3 ? cols[3] : "");
            std::string email       = trim(cols.size() > 4 ? cols[4] : "");
            std::string sex         = trim(cols.size() > 5 ? cols[5] : "0");
            std::string status      = trim(cols.size() > 6 ? cols[6] : "0");

            if (userName.empty()) { fail++; errMsg += "登录账号不能为空；"; continue; }

            // 检查用户是否已存在
            auto exist = db.queryParams(
                "SELECT user_id FROM sys_user WHERE user_name=$1 AND del_flag='0' LIMIT 1", {userName});
            if (exist.ok() && exist.rows() > 0) {
                if (!updateSupport) { fail++; errMsg += userName + " 已存在；"; continue; }
                // 更新已有用户
                std::string uid = std::to_string(exist.longVal(0, 0));
                db.execParams(
                    "UPDATE sys_user SET nick_name=$1,phonenumber=$2,email=$3,sex=$4,status=$5,update_time=NOW()"
                    " WHERE user_id=$6",
                    {nickName, phone, email, sex, status, uid});
                success++;
            } else {
                // 插入新用户，默认密码 123456
                std::string pwd = SecurityUtils::encryptPassword("123456");
                db.execParams(
                    "INSERT INTO sys_user(dept_id,user_name,nick_name,password,phonenumber,email,sex,status,del_flag,create_time)"
                    " VALUES($1,$2,$3,$4,$5,$6,$7,$8,'0',NOW())",
                    {deptId.empty() ? "0" : deptId, userName, nickName, pwd, phone, email, sex, status});
                success++;
            }
        }

        LOG_OPER(req, "用户管理", BusinessType::IMPORT);
        std::string msg = "恭喜您，数据已全部导入成功！共 " + std::to_string(success) + " 条。";
        if (fail > 0) msg = "导入完成，成功 " + std::to_string(success) + " 条，失败 " + std::to_string(fail) + " 条。" + errMsg;
        RESP_MSG(cb, msg);
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
        LOG_OPER(req, "用户管理", BusinessType::EXPORT);
        auto csv = CsvUtils::toCsv(rows, {
            {"用户编号","userId"},{"登录账号","userName"},{"用户昵称","nickName"},
            {"部门","deptName"},{"手机号码","phonenumber"},{"邮箱","email"},
            {"性别","sex"},{"状态","status"},{"最后登录IP","loginIp"},
            {"创建时间","createTime"}});
        cb(CsvUtils::makeCsvResponse(csv, "user.csv"));
    }

    // POST /system/user/profile/avatar — 上传头像（对应 C# SysProfileController.UploadAvatar）
    void uploadAvatar(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        drogon::MultiPartParser parser;
        if (parser.parse(req) != 0) { RESP_ERR(cb, "上传文件异常"); return; }
        auto &files = parser.getFiles();
        if (files.empty()) { RESP_ERR(cb, "请选择要上传的头像"); return; }
        auto &file = files[0];
        std::string ext = std::filesystem::path(file.getFileName()).extension().string();
        for (auto &c : ext) c = (char)std::tolower((unsigned char)c);
        static const std::set<std::string> allowed = {".jpg",".jpeg",".png",".gif",".bmp",".webp"};
        if (allowed.find(ext) == allowed.end()) { RESP_ERR(cb, "只允许上传图片文件"); return; }
        {
            size_t maxMb = 2;
            auto& cfg = drogon::app().getCustomConfig();
            if (cfg.isMember("upload")) maxMb = (size_t)cfg["upload"].get("max_avatar_mb", 2).asInt();
            if (file.fileLength() > maxMb * 1024 * 1024) {
                RESP_ERR(cb, "头像大小超过限制 (" + std::to_string(maxMb) + "MB)");
                return;
            }
        }
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

    void unlock(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                const std::string &userName) {
        CHECK_PERM(req, cb, "system:user:edit");
        SysPasswordService::instance().clearLoginRecordCache(userName);
        LOG_INFO << "[Unlock] " << GET_USER_NAME(req) << " 解锁账户: " << userName;
        LOG_OPER_PARAM(req, "用户管理", BusinessType::UPDATE, "解锁账户: " + userName);
        RESP_MSG(cb, "操作成功");
    }

    Json::Value buildDeptTree(const DatabaseService::QueryResult &res, long parentId,
                              std::set<long>& visited, int depth = 0) {
        Json::Value arr(Json::arrayValue);
        if (depth > 20 || visited.count(parentId)) return arr;
        visited.insert(parentId);
        for (int i = 0; i < res.rows(); ++i) {
            if (res.longVal(i, 1) == parentId) {
                long deptId = res.longVal(i, 0);
                if (visited.count(deptId)) continue;
                Json::Value node;
                node["id"]       = (Json::Int64)deptId;
                node["label"]    = res.str(i, 2);
                node["children"] = buildDeptTree(res, deptId, visited, depth + 1);
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
