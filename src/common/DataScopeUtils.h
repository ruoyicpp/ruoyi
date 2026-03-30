#pragma once
#include <string>
#include <vector>
#include <optional>
#include "LoginUser.h"
#include "../services/DatabaseService.h"

namespace DataScopeUtils {

    // 生成数据权限 SQL 附加条件
    // tableAlias: 主表别名（含 dept_id 字段，如 "u" 代表 sys_user u�?    // userAlias:  �?create_by 字段的表别名（可为空则不加仅本人过滤�?    // 返回 " AND (...)" 形式的字符串，若无需过滤则返�?""
    inline std::string getScopeFilter(const LoginUser &user,
                                      const std::string &tableAlias,
                                      const std::string &userAlias = "") {
        if (user.userId == 1) return "";

        auto &db = DatabaseService::instance();
        std::string uid = std::to_string(user.userId);

        auto roleRes = db.queryParams(
            "SELECT r.role_id, r.data_scope FROM sys_role r "
            "JOIN sys_user_role ur ON r.role_id=ur.role_id "
            "WHERE ur.user_id=$1 AND r.status='0' AND r.del_flag='0'",
            {uid});
        if (!roleRes.ok() || roleRes.rows() == 0) {
            if (!userAlias.empty())
                return " AND " + userAlias + ".create_by='" + user.userName + "'";
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
                // 自定义：�?sys_role_dept
                auto deptRes = db.queryParams(
                    "SELECT dept_id FROM sys_role_dept WHERE role_id=$1", {roleId});
                if (deptRes.ok())
                    for (int j = 0; j < deptRes.rows(); ++j)
                        customDeptIds.push_back(deptRes.str(j, 0));
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
                conditions.push_back(ta + "dept_id='" + dRow.str(0, 0) + "'");
        }

        // dept and sub-depts (ancestors LIKE)
        if (hasDeptSub) {
            auto dRow = db.queryParams(
                "SELECT dept_id FROM sys_user WHERE user_id=$1", {uid});
            if (dRow.ok() && dRow.rows() > 0) {
                std::string deptId = dRow.str(0, 0);
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
            conditions.push_back(ua + "create_by='" + user.userName + "'");
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

// 快捷宏：从请求取 LoginUser 后生成过滤条�?// 若无法取�?user，返回空串（不过滤）
#define DATA_SCOPE_FILTER(req, tableAlias, userAlias) \
    ([&]() -> std::string { \
        auto lu_ = TokenService::instance().getLoginUser(req); \
        if (!lu_) return std::string(""); \
        return DataScopeUtils::getScopeFilter(*lu_, (tableAlias), (userAlias)); \
    }())
