/**
 * @file BackupCtrl.h
 * @brief 备份管理控制器 — 备份文件列表/手动备份/一键恢复/删除/日志
 *
 * 功能概述：
 *   - 备份文件：backups/ 目录扫描列表（文件名/大小/时间）
 *   - 手动备份：立即触发 pg_dump
 *   - 一键恢复：pg_restore -c（危险操作，需 confirm=true）
 *   - 备份日志：sys_backup_log 分页查询
 *
 * API 端点（均需登录 + monitor:backup:* 权限）：
 *   - GET    /monitor/backup/list            备份文件列表
 *   - POST   /monitor/backup/now             立即备份
 *   - POST   /monitor/backup/restore         恢复 {filename, confirm:true}
 *   - DELETE /monitor/backup/{filename}      删除备份文件
 *   - GET    /monitor/backup/log             备份日志分页
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include "../common/AjaxResult.h"
#include "../common/OperLogUtils.h"
#include "../common/PageUtils.h"
#include "../filters/PermFilter.h"
#include "../services/DatabaseService.h"
#include "BackupService.h"

/**
 * @class BackupCtrl
 * @brief 备份管理控制器
 */
class BackupCtrl : public drogon::HttpController<BackupCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(BackupCtrl::list,    "/monitor/backup/list",       drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(BackupCtrl::now,     "/monitor/backup/now",        drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(BackupCtrl::restore, "/monitor/backup/restore",    drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(BackupCtrl::remove,  "/monitor/backup/{filename}", drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(BackupCtrl::log,     "/monitor/backup/log",        drogon::Get,    "JwtAuthFilter");
    METHOD_LIST_END

    /// GET /monitor/backup/list — 备份文件列表
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:backup:list");
        Json::Value arr(Json::arrayValue);
        for (auto& f : BackupService::instance().listFiles()) {
            Json::Value j;
            j["name"]  = f.name;
            j["size"]  = (Json::Int64)f.size;
            j["mtime"] = (Json::Int64)f.mtime;
            arr.append(j);
        }
        RESP_OK(cb, arr);
    }

    /// POST /monitor/backup/now — 立即备份
    void now(const drogon::HttpRequestPtr& req,
             std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:backup:add");
        std::string fname = BackupService::instance().backupNow();
        if (fname.empty()) { RESP_ERR(cb, "备份失败（检查 pg_dump 是否在 PATH）"); return; }
        LOG_OPER_PARAM(req, "数据库备份", BusinessType::INSERT, fname);
        Json::Value r = AjaxResult::success();
        r["filename"] = fname;
        RESP_JSON(cb, r);
    }

    /// POST /monitor/backup/restore {filename, confirm:true}
    void restore(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:backup:restore");
        auto b = req->getJsonObject();
        if (!b || !(*b).isMember("filename")) { RESP_ERR(cb, "缺少 filename"); return; }
        bool confirm = b->get("confirm", false).asBool();
        std::string err;
        if (!BackupService::instance().restore((*b)["filename"].asString(), confirm, err)) {
            RESP_ERR(cb, err); return;
        }
        LOG_OPER_PARAM(req, "数据库恢复", BusinessType::UPDATE, (*b)["filename"].asString());
        RESP_MSG(cb, "恢复完成");
    }

    /// DELETE /monitor/backup/{filename}
    void remove(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                const std::string& filename) {
        CHECK_PERM(req, cb, "monitor:backup:remove");
        std::string err;
        if (!BackupService::instance().remove(filename, err)) {
            RESP_ERR(cb, err.empty() ? "删除失败" : err); return;
        }
        LOG_OPER_PARAM(req, "备份文件", BusinessType::REMOVE, filename);
        RESP_MSG(cb, "删除成功");
    }

    /// GET /monitor/backup/log — 备份日志分页
    void log(const drogon::HttpRequestPtr& req,
             std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:backup:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto cnt = db.query("SELECT COUNT(*) FROM sys_backup_log");
        long total = (cnt.ok() && cnt.rows() > 0) ? cnt.longVal(0, 0) : 0;
        auto res = db.queryParams(
            "SELECT id,filename,size_bytes,status,error_msg,create_time "
            "FROM sys_backup_log ORDER BY id DESC LIMIT $1 OFFSET $2",
            {std::to_string(page.pageSize), std::to_string(page.offset())});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]          = (Json::Int64)res.longVal(i, 0);
            j["filename"]    = res.str(i, 1);
            j["size_bytes"]  = (Json::Int64)res.longVal(i, 2);
            j["status"]      = res.str(i, 3);
            j["error_msg"]   = res.str(i, 4);
            j["create_time"] = res.str(i, 5);
            rows.append(j);
        }
        Json::Value data; data["total"] = (Json::Int64)total; data["rows"] = rows;
        RESP_OK(cb, data);
    }
};
