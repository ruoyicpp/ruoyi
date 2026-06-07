/**
 * @file SysNoticeCtrl.h
 * @brief 系统公告管理控制器
 * 
 * 功能概述：
 *   - 公告管理：CRUD 操作，支持分页、搜索
 *   - 公告类型：通知、公告等不同类型
 *   - 已读管理：记录用户对公告的已读状态
 *   - 未读计数：获取当前用户的未读公告数
 * 
 * API 端点：
 *   - GET    /system/notice/list          - 公告列表（分页）
 *   - GET    /system/notice/listTop       - 顶部公告列表
 *   - GET    /system/notice/{id}          - 获取公告详情
 *   - POST   /system/notice               - 新增公告
 *   - PUT    /system/notice               - 修改公告
 *   - DELETE /system/notice/{ids}         - 删除公告（支持批量）
 *   - POST   /system/notice/markRead      - 标记单条公告已读
 *   - POST   /system/notice/markReadAll   - 标记全部公告已读
 *   - GET    /system/notice/unreadCount   - 获取未读公告数
 * 
 * 权限要求：
 *   - system:notice:list   - 查看公告列表
 *   - system:notice:add    - 新增公告
 *   - system:notice:edit   - 修改公告
 *   - system:notice:remove - 删除公告
 * 
 * 核心特性：
 *   - 公告类型：支持通知、公告等多种类型
 *   - 已读追踪：记录每个用户对每条公告的已读状态
 *   - 未读计数：实时计算用户的未读公告数（用于顶栏徽标）
 *   - 操作日志：所有修改操作自动记录到 sys_oper_log
 * 
 * 使用场景：
 *   - 系统通知：发布系统维护、更新等通知
 *   - 公司公告：发布公司重要信息
 *   - 待办提醒：提醒用户完成待办任务
 */

#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

/**
 * @class SysNoticeCtrl
 * @brief 系统公告管理控制器
 * 
 * 对应 RuoYi-Vue 中的 SysNoticeController，提供公告管理的所有 API 端点。
 * 所有操作都需要 JWT 认证，并根据权限字符串进行权限检查。
 */
class SysNoticeCtrl : public drogon::HttpController<SysNoticeCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysNoticeCtrl::list,    "/system/notice/list",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::listTop, "/system/notice/listTop", drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::getById, "/system/notice/{id}",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::add,     "/system/notice",         drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::edit,    "/system/notice",         drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::remove,  "/system/notice/{ids}",   drogon::Delete, "JwtAuthFilter");
        // 顶栏通知：标记单条 / 批量已读（沿用 sys_notice_read(user_id, notice_id, read_at)）
        ADD_METHOD_TO(SysNoticeCtrl::markRead,     "/system/notice/markRead",     drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(SysNoticeCtrl::markReadAll,  "/system/notice/markReadAll",  drogon::Post, "JwtAuthFilter");
        // 顶栏红色徽标：当前登录用户的未读公告数
        ADD_METHOD_TO(SysNoticeCtrl::unreadCount,  "/system/notice/unreadCount",  drogon::Get,  "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notice:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT notice_id,notice_title,notice_type,status,create_by,create_time FROM sys_notice WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto title    = req->getParameter("noticeTitle");
        auto type     = req->getParameter("noticeType");
        auto createBy = req->getParameter("createBy");
        if (!title.empty())    { sql += " AND notice_title LIKE $" + std::to_string(idx++); params.push_back("%" + title + "%"); }
        if (!type.empty())     { sql += " AND notice_type=$" + std::to_string(idx++); params.push_back(type); }
        if (!createBy.empty()) { sql += " AND create_by LIKE $" + std::to_string(idx++); params.push_back("%" + createBy + "%"); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY notice_id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        // 列字段: notice_id(0),notice_title(1),notice_type(2),status(3),create_by(4),create_time(5)
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["noticeId"]    = res.intVal(i, 0);
            j["noticeTitle"] = res.str(i, 1);
            j["noticeType"]  = res.str(i, 2);
            j["status"]      = res.str(i, 3);
            j["createBy"]    = res.str(i, 4);
            j["createTime"]  = fmtTs(res.str(i, 5));
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    // 前端多通知菜单，显示最新5条公告 + 标识当前用户已读/未读
    void listTop(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        auto& db = DatabaseService::instance();
        // LEFT JOIN sys_notice_read 用 read_at IS NOT NULL 判定 isRead
        auto res = db.queryParams(
            "SELECT n.notice_id, n.notice_title, n.notice_type, n.status, n.create_by, n.create_time, "
            "       CASE WHEN r.read_at IS NULL THEN 0 ELSE 1 END AS is_read "
            "FROM sys_notice n "
            "LEFT JOIN sys_notice_read r "
            "       ON r.notice_id = n.notice_id AND r.user_id = $1 "
            "WHERE n.status='0' ORDER BY n.notice_id DESC LIMIT 5",
            {std::to_string(userId)});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["noticeId"]    = res.intVal(i, 0);
            j["noticeTitle"] = res.str(i, 1);
            j["noticeType"]  = res.str(i, 2);
            j["status"]      = res.str(i, 3);
            j["createBy"]    = res.str(i, 4);
            j["createTime"]  = fmtTs(res.str(i, 5));
            j["isRead"]      = res.intVal(i, 6) == 1;
            rows.append(j);
        }
        RESP_OK(cb, rows);
    }

    // 顶栏红色徽标：当前登录用户的未读公告数
    void unreadCount(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        if (userId <= 0) { RESP_401(cb); return; }
        // 启用的公告中，当前用户尚未读的数量
        auto res = DatabaseService::instance().queryParams(
            "SELECT COUNT(*) FROM sys_notice n "
            "LEFT JOIN sys_notice_read r "
            "       ON r.notice_id = n.notice_id AND r.user_id = $1 "
            "WHERE n.status='0' AND r.read_at IS NULL",
            {std::to_string(userId)});
        long count = (res.ok() && res.rows() > 0) ? res.longVal(0, 0) : 0;
        Json::Value j;
        j["count"] = (Json::Int64)count;
        RESP_OK(cb, j);
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int id) {
        CHECK_PERM(req, cb, "system:notice:query");
        // 列字段: notice_id(0),notice_title(1),notice_type(2),notice_content(3),status(4),remark(5)
        auto res = DatabaseService::instance().queryParams(
            "SELECT notice_id,notice_title,notice_type,notice_content,status,remark FROM sys_notice WHERE notice_id=$1",
            {std::to_string(id)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "公告不存在"); return; }
        Json::Value j;
        j["noticeId"]      = res.intVal(0, 0);
        j["noticeTitle"]   = res.str(0, 1);
        j["noticeType"]    = res.str(0, 2);
        j["noticeContent"] = res.str(0, 3);
        j["status"]        = res.str(0, 4);
        j["remark"]        = res.str(0, 5);
        RESP_OK(cb, j);
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notice:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        DatabaseService::instance().execParams(
            "INSERT INTO sys_notice(notice_title,notice_type,notice_content,status,remark,create_by,create_time) VALUES($1,$2,$3,$4,$5,$6,NOW())",
            {(*body)["noticeTitle"].asString(), (*body)["noticeType"].asString(),
             (*body).get("noticeContent","").asString(), (*body).get("status","0").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req)});
        LOG_OPER(req, "通知公告", BusinessType::INSERT);
        RESP_MSG(cb, "操作成功");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notice:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_notice SET notice_title=$1,notice_type=$2,notice_content=$3,status=$4,remark=$5,update_by=$6,update_time=NOW() WHERE notice_id=$7",
            {(*body)["noticeTitle"].asString(), (*body)["noticeType"].asString(),
             (*body).get("noticeContent","").asString(), (*body).get("status","0").asString(),
             (*body).get("remark","").asString(), GET_USER_NAME(req),
             std::to_string((*body)["noticeId"].asInt())});
        LOG_OPER(req, "通知公告", BusinessType::UPDATE);
        RESP_MSG(cb, "操作成功");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:notice:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids))
            db.execParams("DELETE FROM sys_notice WHERE notice_id=$1", {idStr});
        LOG_OPER_PARAM(req, "通知公告", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "操作成功");
    }

    // 标记单条通知已读：POST /system/notice/markRead?noticeId=123
    void markRead(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        if (userId <= 0) { RESP_401(cb); return; }
        std::string nid = req->getParameter("noticeId");
        if (nid.empty()) { RESP_ERR(cb, "noticeId 不能为空"); return; }
        // 复合主键 (user_id, notice_id) 已存在则忽略；read_at 默认 NOW()
        DatabaseService::instance().execParams(
            "INSERT INTO sys_notice_read(user_id, notice_id, read_at) VALUES($1,$2,NOW()) "
            "ON CONFLICT (user_id, notice_id) DO NOTHING",
            {std::to_string(userId), nid});
        RESP_MSG(cb, "已标记为已读");
    }

    // 批量标记已读：POST /system/notice/markReadAll?ids=1,2,3   (ids 为空表示标记当前所有有效公告)
    void markReadAll(const drogon::HttpRequestPtr &req,
                     std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        if (userId <= 0) { RESP_401(cb); return; }
        auto& db = DatabaseService::instance();
        std::string ids = req->getParameter("ids");
        std::vector<std::string> noticeIds;
        if (!ids.empty()) {
            noticeIds = splitIds(ids);
        } else {
            auto res = db.query("SELECT notice_id FROM sys_notice WHERE status='0'");
            if (res.ok()) for (int i = 0; i < res.rows(); ++i) noticeIds.push_back(res.str(i, 0));
        }
        for (auto& nid : noticeIds) {
            db.execParams(
                "INSERT INTO sys_notice_read(user_id, notice_id, read_at) VALUES($1,$2,NOW()) "
                "ON CONFLICT (user_id, notice_id) DO NOTHING",
                {std::to_string(userId), nid});
        }
        RESP_MSG(cb, "已全部标记为已读");
    }

private:
    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> r; std::stringstream ss(ids); std::string item;
        while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
        return r;
    }
};
