/**
 * @file DataScopeUtils.h
 * @brief 数据权限工具 — 基于部门和角色的数据访问控制
 * 
 * 功能概述：
 *   - 数据权限过滤：根据用户角色生成 SQL WHERE 条件
 *   - 权限范围：全部、本部门、本部门及下级、自定义部门、仅本人
 *   - SQL 注入防御：字符串转义、整数防御
 *   - 管理员豁免：管理员（userId=1）无数据权限限制
 * 
 * 权限范围定义：
 *   - 1 = 全部：可访问所有数据
 *   - 2 = 自定义：可访问指定部门的数据（sys_role_dept）
 *   - 3 = 本部门：仅可访问本部门的数据
 *   - 4 = 本部门及下级：可访问本部门及下级部门的数据
 *   - 5 = 仅本人：仅可访问本人创建的数据
 * 
 * 使用示例：
 *   // 在查询中添加数据权限过滤
 *   std::string sql = "SELECT * FROM sys_user WHERE del_flag='0'";
 *   sql += DATA_SCOPE_FILTER(req, "u", "u");
 *   
 *   // 或手动调用
 *   auto user = GET_LOGIN_USER(req);
 *   std::string filter = DataScopeUtils::getScopeFilter(user, "u", "u");
 *   sql += filter;
 * 
 * 配置项（config.json）：
 *   - datascope.enabled: 是否启用数据权限（默认 true）
 */

#pragma once
#include <string>
#include <vector>
#include <optional>
#include "LoginUser.h"
#include "../services/DatabaseService.h"

/**
 * @namespace DataScopeUtils
 * @brief 数据权限工具命名空间
 * 
 * 提供数据权限过滤和 SQL 注入防御功能。
 * 所有函数都是内联的，无需编译链接。
 */
namespace DataScopeUtils {

    // SQL 字符串字面量转义：双写单引号；用于 userName 等不便参数化的拼接场景
    inline std::string sqlEscape(const std::string &s) {
        std::string out;
        out.reserve(s.size() + 4);
        for (char c : s) {
            if (c == '\'') out += "''";
            else out += c;
        }
        return out;
    }

    // 整数防御：仅允许 0-9，过滤可能的注入字符；不合法返回 "0"
    inline std::string sanitizeId(const std::string &s) {
        if (s.empty()) return "0";
        for (char c : s) if (c < '0' || c > '9') return "0";
        return s;
    }

    // 生成数据权限 SQL 附加条件
    // tableAlias: 主表别名（含 dept_id 字段，如 "u" 代表 sys_user u）
    // userAlias:  创建者字段别名（如 "u" 代表 u.create_by）
    inline std::string getScopeFilter(const LoginUser &user,
                                      const std::string &tableAlias,
                                      const std::string &userAlias = "") {
        if (user.userId == 1) return "";

        auto &db = DatabaseService::instance();
        std::string uid = std::to_string(user.userId);
        std::string escUser = sqlEscape(user.userName);

        auto roleRes = db.queryParams(
            "SELECT r.role_id, r.data_scope FROM sys_role r "
            "JOIN sys_user_role ur ON r.role_id=ur.role_id "
            "WHERE ur.user_id=$1 AND r.status='0' AND r.del_flag='0'",
            {uid});
        if (!roleRes.ok() || roleRes.rows() == 0) {
            if (!userAlias.empty())
                return " AND " + userAlias + ".create_by='" + escUser + "'";
            return "";
        }

        bool hasAll    = false;
        bool hasSelf   = false;
        bool hasDept   = false;
        bool hasDeptSub= false;
        std::vector<std::string> customDeptIds;

        for (int i = 0; i < roleRes.rows(); ++i) {
            std::string scope   = roleRes.str(i, 1);
            std::string roleId  = roleRes.str(i, 0);
            if (scope == "1") { hasAll = true; break; }
            else if (scope == "2") {
    // 自定义：筛选 sys_role_dept 关联的部门
                auto deptRes = db.queryParams(
                    "SELECT dept_id FROM sys_role_dept WHERE role_id=$1", {roleId});
                if (deptRes.ok())
                    for (int j = 0; j < deptRes.rows(); ++j)
                        customDeptIds.push_back(sanitizeId(deptRes.str(j, 0)));
            }
            else if (scope == "3") hasDept    = true;
            else if (scope == "4") hasDeptSub = true;
            else if (scope == "5") hasSelf    = true;
        }

        if (hasAll) return "";

        std::string ta = tableAlias.empty() ? "" : tableAlias + ".";
        std::vector<std::string> conditions;

        // custom dept list
        if (!customDeptIds.empty()) {
            std::string inList;
            for (size_t i = 0; i < customDeptIds.size(); ++i) {
                if (i) inList += ",";
                inList += customDeptIds[i];
            }
            conditions.push_back(ta + "dept_id IN (" + inList + ")");
        }

        // exact dept match
        if (hasDept) {
            auto dRow = db.queryParams(
                "SELECT dept_id FROM sys_user WHERE user_id=$1", {uid});
            if (dRow.ok() && dRow.rows() > 0)
                conditions.push_back(ta + "dept_id='" + sanitizeId(dRow.str(0, 0)) + "'");
        }

        // dept and sub-depts (ancestors LIKE)
        if (hasDeptSub) {
            auto dRow = db.queryParams(
                "SELECT dept_id FROM sys_user WHERE user_id=$1", {uid});
            if (dRow.ok() && dRow.rows() > 0) {
                std::string deptId = sanitizeId(dRow.str(0, 0));
                conditions.push_back(
                    ta + "dept_id IN (SELECT dept_id FROM sys_dept "
                    "WHERE del_flag='0' AND (dept_id=" + deptId +
                    " OR ancestors LIKE '%," + deptId + ",%'"
                    " OR ancestors LIKE '%," + deptId + "'"
                    " OR ancestors LIKE '" + deptId + ",%'))");
            }
        }

        // self only
        if (hasSelf && !userAlias.empty()) {
            std::string ua = userAlias.empty() ? "" : userAlias + ".";
            conditions.push_back(ua + "create_by='" + escUser + "'");
        }

        if (conditions.empty()) return "";

        std::string filter = " AND (";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i) filter += " OR ";
            filter += conditions[i];
        }
        filter += ")";
        return filter;
    }
}

    // 快捷宏：从请求取 LoginUser 后生成过滤条件
    // 若无法取到 user，返回空串（不过滤）
#define DATA_SCOPE_FILTER(req, tableAlias, userAlias) \
    ([&]() -> std::string { \
        auto lu_ = TokenService::instance().getLoginUser(req); \
        if (!lu_) return std::string(""); \
        return DataScopeUtils::getScopeFilter(*lu_, (tableAlias), (userAlias)); \
    }())
