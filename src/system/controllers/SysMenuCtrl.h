#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../services/SysMenuService.h"

class SysMenuCtrl : public drogon::HttpController<SysMenuCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysMenuCtrl::list,               "/system/menu/list",                    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysMenuCtrl::getById,            "/system/menu/{menuId}",                drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysMenuCtrl::treeselect,         "/system/menu/treeselect",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysMenuCtrl::roleMenuTreeselect, "/system/menu/roleMenuTreeselect/{roleId}", drogon::Get, "JwtAuthFilter");
        ADD_METHOD_TO(SysMenuCtrl::add,                "/system/menu",                         drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysMenuCtrl::edit,               "/system/menu",                         drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysMenuCtrl::remove,             "/system/menu/{menuId}",                drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:menu:list");
        long userId = GET_USER_ID(req);
        auto menus = SysMenuService::instance().selectMenuList(userId,
            req->getParameter("menuName"), req->getParameter("status"));
        RESP_OK(cb, menus);
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long menuId) {
        CHECK_PERM(req, cb, "system:menu:query");
        // list字段: menu_id(0),menu_name(1),parent_id(2),order_num(3),path(4),component(5)
        //         is_frame(7),is_cache(8),menu_type(9),visible(10),status(11),perms(12),icon(13),remark(14)
        auto res = DatabaseService::instance().queryParams(
            "SELECT menu_id,menu_name,parent_id,order_num,path,component,query,"
            "is_frame,is_cache,menu_type,visible,status,perms,icon,remark "
            "FROM sys_menu WHERE menu_id=$1", {std::to_string(menuId)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "菜单不存在"); return; }
        Json::Value j;
        j["menuId"]    = (Json::Int64)res.longVal(0, 0);
        j["menuName"]  = res.str(0, 1);
        j["parentId"]  = (Json::Int64)res.longVal(0, 2);
        j["orderNum"]  = res.intVal(0, 3);
        j["path"]      = res.str(0, 4);
        j["component"] = res.str(0, 5);
        j["query"]     = res.str(0, 6);
        j["isFrame"]   = res.str(0, 7);
        j["isCache"]   = res.str(0, 8);
        j["menuType"]  = res.str(0, 9);
        j["visible"]   = res.str(0, 10);
        j["status"]    = res.str(0, 11);
        j["perms"]     = res.str(0, 12);
        j["icon"]      = res.str(0, 13);
        j["remark"]    = res.str(0, 14);
        RESP_OK(cb, j);
    }

    void treeselect(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        auto tree = SysMenuService::instance().buildMenuTreeSelect(userId);
        RESP_OK(cb, tree);
    }

    void roleMenuTreeselect(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long roleId) {
        long userId = GET_USER_ID(req);
        auto roleRow = DatabaseService::instance().queryParams(
            "SELECT menu_check_strictly FROM sys_role WHERE role_id=$1", {std::to_string(roleId)});
        bool strict = (!roleRow.ok() || roleRow.rows() == 0) ? true : (roleRow.str(0, 0) == "true" || roleRow.str(0, 0) == "t");
        auto checkedKeys = SysMenuService::instance().selectMenuListByRoleId(roleId, strict);
        auto menus = SysMenuService::instance().buildMenuTreeSelect(userId);
        auto result = AjaxResult::successMap();
        result["checkedKeys"] = checkedKeys;
        result["menus"]       = menus;
        RESP_JSON(cb, result);
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:menu:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        std::string menuName = (*body)["menuName"].asString();
        long parentId = (*body).get("parentId", 0).asInt64();
        if (!SysMenuService::instance().checkMenuNameUnique(menuName, parentId))
            { RESP_ERR(cb, "新增菜单'" + menuName + "'失败，菜单名称已存在"); return; }
        // isFrame=0(外链) 时 path 必须以 http(s):// 开头（对应 C# UserConstants.YES_FRAME）
        std::string isFrame = (*body).get("isFrame","1").asString();
        std::string path    = (*body).get("path","").asString();
        if (isFrame == "0" && path.substr(0, 7) != "http://" && path.substr(0, 8) != "https://")
            { RESP_ERR(cb, "新增菜单'" + menuName + "'失败，地址必须以http(s)://开头"); return; }
        DatabaseService::instance().execParams(
            "INSERT INTO sys_menu(menu_name,parent_id,order_num,path,component,query,"
            "is_frame,is_cache,menu_type,visible,status,perms,icon,remark,create_by,create_time) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,NOW())",
            {menuName, std::to_string(parentId),
             std::to_string((*body).get("orderNum",0).asInt()),
             (*body).get("path","").asString(), (*body).get("component","").asString(),
             (*body).get("query","").asString(), (*body).get("isFrame","1").asString(),
             (*body).get("isCache","0").asString(), (*body).get("menuType","M").asString(),
             (*body).get("visible","0").asString(), (*body).get("status","0").asString(),
             (*body).get("perms","").asString(), (*body).get("icon","#").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req)});
        LOG_OPER(req, "菜单管理", BusinessType::INSERT);
        RESP_MSG(cb, "操作成功");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:menu:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        long menuId   = (*body)["menuId"].asInt64();
        long parentId = (*body).get("parentId", 0).asInt64();
        if (menuId == parentId) { RESP_ERR(cb, "修改菜单失败，上级菜单不能选择自己"); return; }
        std::string menuName = (*body).get("menuName","").asString();
        // 菜单名唯一性（排除自身，对应 C# CheckMenuNameUnique）
        auto nm = DatabaseService::instance().queryParams(
            "SELECT menu_id FROM sys_menu WHERE menu_name=$1 AND parent_id=$2 AND menu_id!=$3 LIMIT 1",
            {menuName, std::to_string(parentId), std::to_string(menuId)});
        if (nm.ok() && nm.rows() > 0) { RESP_ERR(cb, "修改菜单'"+menuName+"'失败，菜单名称已存在"); return; }
        // isFrame=0(外链) 时 path 必须以 http(s):// 开头
        std::string isFrame = (*body).get("isFrame","1").asString();
        std::string path    = (*body).get("path","").asString();
        if (isFrame == "0" && path.substr(0, 7) != "http://" && path.substr(0, 8) != "https://")
            { RESP_ERR(cb, "修改菜单'" + menuName + "'失败，地址必须以http(s)://开头"); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_menu SET menu_name=$1,parent_id=$2,order_num=$3,path=$4,component=$5,"
            "query=$6,is_frame=$7,is_cache=$8,menu_type=$9,visible=$10,status=$11,perms=$12,"
            "icon=$13,remark=$14,update_by=$15,update_time=NOW() WHERE menu_id=$16",
            {(*body).get("menuName","").asString(), std::to_string(parentId),
             std::to_string((*body).get("orderNum",0).asInt()),
             (*body).get("path","").asString(), (*body).get("component","").asString(),
             (*body).get("query","").asString(), (*body).get("isFrame","1").asString(),
             (*body).get("isCache","0").asString(), (*body).get("menuType","M").asString(),
             (*body).get("visible","0").asString(), (*body).get("status","0").asString(),
             (*body).get("perms","").asString(), (*body).get("icon","#").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req), std::to_string(menuId)});
        LOG_OPER(req, "菜单管理", BusinessType::UPDATE);
        RESP_MSG(cb, "操作成功");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long menuId) {
        CHECK_PERM(req, cb, "system:menu:remove");
        if (SysMenuService::instance().hasChildByMenuId(menuId)) { RESP_ERR(cb, "存在子菜单,不允许删除"); return; }
        if (SysMenuService::instance().checkMenuExistRole(menuId)) { RESP_ERR(cb, "菜单已分配给角色,不允许删除"); return; }
        DatabaseService::instance().execParams("DELETE FROM sys_menu WHERE menu_id=$1", {std::to_string(menuId)});
        LOG_OPER_PARAM(req, "菜单管理", BusinessType::REMOVE, std::to_string(menuId));
        RESP_MSG(cb, "操作成功");
    }
};
