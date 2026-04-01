#pragma once
#include <json/json.h>
#include <string>
#include <vector>
#include <set>
#include "../../common/Constants.h"
#include "../../common/SecurityUtils.h"
#include "../../services/DatabaseService.h"


class SysMenuService {
public:
    static SysMenuService &instance() {
        static SysMenuService inst;
        return inst;
    }

    // 根据用户ID查询权限（供前端 getRouters 使用）
    Json::Value buildMenusForUser(long userId) {
        auto& db = DatabaseService::instance();
        DatabaseService::QueryResult res{};
        if (SecurityUtils::isAdmin(userId)) {
            res = db.query(
                "SELECT menu_id,menu_name,parent_id,order_num,path,component,query,"
                "is_frame,is_cache,menu_type,visible,status,perms,icon "
                "FROM sys_menu WHERE menu_type IN ('M','C') AND status='0' "
                "ORDER BY parent_id,order_num");
        } else {
            res = db.queryParams(
                "SELECT DISTINCT m.menu_id,m.menu_name,m.parent_id,m.order_num,"
                "m.path,m.component,m.query,m.is_frame,m.is_cache,m.menu_type,"
                "m.visible,m.status,m.perms,m.icon "
                "FROM sys_menu m "
                "INNER JOIN sys_role_menu rm ON m.menu_id=rm.menu_id "
                "INNER JOIN sys_user_role ur ON rm.role_id=ur.role_id "
                "WHERE ur.user_id=$1 AND m.menu_type IN ('M','C') AND m.status='0' "
                "ORDER BY m.parent_id,m.order_num",
                {std::to_string(userId)});
        }
        if (!res.ok()) return Json::Value(Json::arrayValue);

        std::set<long> vis; auto menus = buildTree(res, 0, vis);
        return buildRouters(menus);
    }

    // 构建路由树（供前端 getRouters 使用）
    Json::Value selectMenuList(long userId, const std::string &menuName = "",
                               const std::string &status = "") {
        auto& db = DatabaseService::instance();
        std::string sql =
            "SELECT menu_id,menu_name,parent_id,order_num,path,component,query,"
            "is_frame,is_cache,menu_type,visible,status,perms,icon,"
            "create_time,update_time "
            "FROM sys_menu WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        if (!menuName.empty()) {
            sql += " AND menu_name LIKE $" + std::to_string(idx++);
            params.push_back("%" + menuName + "%");
        }
        if (!status.empty()) {
            sql += " AND status=$" + std::to_string(idx++);
            params.push_back(status);
        }
        if (!SecurityUtils::isAdmin(userId)) {
            sql += " AND menu_id IN (SELECT DISTINCT m.menu_id FROM sys_menu m "
                   "INNER JOIN sys_role_menu rm ON m.menu_id=rm.menu_id "
                   "INNER JOIN sys_user_role ur ON rm.role_id=ur.role_id "
                   "WHERE ur.user_id=$" + std::to_string(idx++) + ")";
            params.push_back(std::to_string(userId));
        }
        sql += " ORDER BY parent_id,order_num";

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value arr(Json::arrayValue);
        if (res.ok()) {
            for (int i = 0; i < res.rows(); ++i)
                arr.append(rowToJson(res, i));
        }
        return arr;
    }

    // 构建菜单树（按 parent_id 分组）
    Json::Value selectMenuListByRoleId(long roleId, bool menuCheckStrictly) {
        auto& db = DatabaseService::instance();
        std::string sql =
            "SELECT m.menu_id FROM sys_menu m "
            "INNER JOIN sys_role_menu rm ON m.menu_id=rm.menu_id "
            "WHERE rm.role_id=$1";
        if (menuCheckStrictly)
            sql += " AND m.menu_id NOT IN (SELECT m2.parent_id FROM sys_menu m2 "
                   "INNER JOIN sys_role_menu rm2 ON m2.menu_id=rm2.menu_id "
                   "WHERE rm2.role_id=$1)";
        auto res = db.queryParams(sql, {std::to_string(roleId)});
        Json::Value arr(Json::arrayValue);
        if (res.ok()) {
            for (int i = 0; i < res.rows(); ++i)
                arr.append((Json::Int64)res.longVal(i, 0));
        }
        return arr;
    }

    // 构建 TreeSelect（供前端 treeselect 使用）
    Json::Value buildMenuTreeSelect(long userId) {
        auto& db = DatabaseService::instance();
        DatabaseService::QueryResult res{};
        if (SecurityUtils::isAdmin(userId)) {
            res = db.query(
                "SELECT menu_id,menu_name,parent_id FROM sys_menu "
                "WHERE status='0' ORDER BY parent_id,order_num");
        } else {
            res = db.queryParams(
                "SELECT DISTINCT m.menu_id,m.menu_name,m.parent_id FROM sys_menu m "
                "INNER JOIN sys_role_menu rm ON m.menu_id=rm.menu_id "
                "INNER JOIN sys_user_role ur ON rm.role_id=ur.role_id "
                "WHERE ur.user_id=$1 AND m.status='0' ORDER BY m.parent_id,m.order_num",
                {std::to_string(userId)});
        }
        if (!res.ok()) return Json::Value(Json::arrayValue);
        std::set<long> vis; auto menus = buildTreeSimple(res, 0, vis);
        return toTreeSelect(menus);
    }

    bool hasChildByMenuId(long menuId) {
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT COUNT(*) FROM sys_menu WHERE parent_id=$1",
            {std::to_string(menuId)});
        return res.ok() && res.rows() > 0 && res.intVal(0, 0) > 0;
    }

    bool checkMenuExistRole(long menuId) {
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT COUNT(*) FROM sys_role_menu WHERE menu_id=$1",
            {std::to_string(menuId)});
        return res.ok() && res.rows() > 0 && res.intVal(0, 0) > 0;
    }

    bool checkMenuNameUnique(const std::string &menuName, long parentId, long menuId = 0) {
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT menu_id FROM sys_menu WHERE menu_name=$1 AND parent_id=$2 LIMIT 1",
            {menuName, std::to_string(parentId)});
        if (!res.ok() || res.rows() == 0) return true;
        return res.longVal(0, 0) == menuId;
    }

private:
    // 查询完整菜单数据（对应 JSON 14字段：menu_id,menu_name,parent_id,...）
    Json::Value rowToJson(const DatabaseService::QueryResult &res, int row) {
        Json::Value j;
        j["menuId"]    = (Json::Int64)res.longVal(row, 0);
        j["menuName"]  = res.str(row, 1);
        j["parentId"]  = (Json::Int64)res.longVal(row, 2);
        j["orderNum"]  = res.intVal(row, 3);
        j["path"]      = res.str(row, 4);
        j["component"] = res.str(row, 5);
        j["query"]     = res.str(row, 6);
        j["isFrame"]   = res.str(row, 7);
        j["isCache"]   = res.str(row, 8);
        j["menuType"]  = res.str(row, 9);
        j["visible"]   = res.str(row, 10);
        j["status"]    = res.str(row, 11);
        j["perms"]     = res.str(row, 12);
        j["icon"]      = res.str(row, 13);
        j["children"]  = Json::Value(Json::arrayValue);
        return j;
    }

    // 构建树结构
    Json::Value buildTree(const DatabaseService::QueryResult &res, long parentId,
                          std::set<long>& visited, int depth = 0) {
        Json::Value arr(Json::arrayValue);
        if (depth > 20 || visited.count(parentId)) return arr;
        visited.insert(parentId);
        for (int i = 0; i < res.rows(); ++i) {
            long pid = res.longVal(i, 2);  // parent_id
            if (pid == parentId) {
                long mid = res.longVal(i, 0);  // menu_id
                if (visited.count(mid)) continue;
                auto node = rowToJson(res, i);
                node["children"] = buildTree(res, mid, visited, depth + 1);
                arr.append(node);
            }
        }
        return arr;
    }

    // 查询完整菜单，含 menu_id,menu_name,parent_id 用于展示
    Json::Value buildTreeSimple(const DatabaseService::QueryResult &res, long parentId,
                                std::set<long>& visited, int depth = 0) {
        Json::Value arr(Json::arrayValue);
        if (depth > 20 || visited.count(parentId)) return arr;
        visited.insert(parentId);
        for (int i = 0; i < res.rows(); ++i) {
            long pid = res.longVal(i, 2);
            if (pid == parentId) {
                long mid = res.longVal(i, 0);
                if (visited.count(mid)) continue;
                Json::Value node;
                node["menuId"]   = (Json::Int64)mid;
                node["menuName"] = res.str(i, 1);
                node["children"] = buildTreeSimple(res, mid, visited, depth + 1);
                arr.append(node);
            }
        }
        return arr;
    }

    // 构建路由层级（递归）
    Json::Value buildRouters(const Json::Value &menus) {
        Json::Value routers(Json::arrayValue);
        for (auto &menu : menus) {
            Json::Value router;
            std::string menuType = menu["menuType"].asString();
            std::string path     = menu["path"].asString();
            std::string isFrame  = menu["isFrame"].asString();

            router["hidden"]    = menu["visible"].asString() == "1";
            router["name"]      = toUpperCamelCase(path);
            router["path"]      = getRouterPath(menu);
            router["component"] = getComponent(menu);
            router["query"]     = menu["query"];

            Json::Value meta;
            meta["title"]   = menu["menuName"];
            meta["icon"]    = menu["icon"];
            meta["noCache"] = menu["isCache"].asString() == "1";
            meta["link"]    = isFrame == "0" ? path : "";
            router["meta"]  = meta;

            auto &children = menu["children"];
            if (!children.empty() && menuType == "M") {
                router["alwaysShow"] = true;
                router["redirect"]   = "noRedirect";
                router["children"]   = buildRouters(children);
            } else if (isMenuFrame(menu)) {
                router["meta"] = Json::Value();
                Json::Value childRouter;
                childRouter["path"]      = path;
                childRouter["component"] = menu["component"];
                childRouter["name"]      = toUpperCamelCase(path);
                childRouter["meta"]      = meta;
                childRouter["query"]     = menu["query"];
                Json::Value childArr(Json::arrayValue);
                childArr.append(childRouter);
                router["children"] = childArr;
            } else if (isInnerLinkMenu(menu)) {
                // 顶层内嵌链接：外层用 Layout，内层用 InnerLink
                // meta.link 已设为 HTTP 路径，前端 IframeToggle 用它作 iframe src
                router["meta"] = Json::Value();
                std::string lp = innerLinkPath(path);
                Json::Value childRouter;
                childRouter["path"]      = lp;
                childRouter["component"] = "InnerLink";
                childRouter["name"]      = toUpperCamelCase(lp);
                childRouter["meta"]      = meta;
                childRouter["query"]     = menu["query"];
                Json::Value childArr(Json::arrayValue);
                childArr.append(childRouter);
                router["children"] = childArr;
            }
            routers.append(router);
        }
        return routers;
    }

    bool isMenuFrame(const Json::Value &menu) {
        return menu["parentId"].asInt64() == 0
            && menu["menuType"].asString() == "C"
            && menu["isFrame"].asString() == "1";
    }

    // 顶层内嵌链接（component='InnerLink' + is_frame='0' + parentId=0）
    bool isInnerLinkMenu(const Json::Value &menu) {
        return menu["parentId"].asInt64() == 0
            && menu["isFrame"].asString() == "0"
            && menu["component"].asString() == "InnerLink";
    }

    // http://localhost:18080/ai → localhost/18080/ai, /ai → ai
    std::string innerLinkPath(const std::string& url) {
        std::string r = url;
        if (r.size() >= 8 && r.substr(0,8) == "https://") r = r.substr(8);
        else if (r.size() >= 7 && r.substr(0,7) == "http://") r = r.substr(7);
        for (char& c : r) if (c == '.' || c == ':') c = '/';
        // strip leading slashes for use as router path segment
        size_t s = r.find_first_not_of('/');
        return (s != std::string::npos) ? r.substr(s) : r;
    }

    std::string getRouterPath(const Json::Value &menu) {
        std::string path     = menu["path"].asString();
        long parentId        = menu["parentId"].asInt64();
        std::string menuType = menu["menuType"].asString();
        std::string isFrame  = menu["isFrame"].asString();
        if (parentId == 0 && menuType == "M" && isFrame == "1")
            return "/" + path;
        if (isMenuFrame(menu)) return "/";
        if (isInnerLinkMenu(menu)) return "/";
        return path;
    }

    std::string getComponent(const Json::Value &menu) {
        std::string comp = menu["component"].asString();
        if (!comp.empty() && !isMenuFrame(menu) && !isInnerLinkMenu(menu)) return comp;
        if (menu["parentId"].asInt64() != 0 && menu["menuType"].asString() == "M")
            return Constants::PARENT_VIEW;
        return Constants::LAYOUT;
    }

    std::string toUpperCamelCase(const std::string &s) {
        if (s.empty()) return s;
        std::string r = s;
        r[0] = std::toupper(r[0]);
        return r;
    }

    Json::Value toTreeSelect(const Json::Value &menus) {
        Json::Value arr(Json::arrayValue);
        for (auto &m : menus) {
            Json::Value node;
            node["id"]       = m["menuId"];
            node["label"]    = m["menuName"];
            node["children"] = toTreeSelect(m["children"]);
            arr.append(node);
        }
        return arr;
    }
};
