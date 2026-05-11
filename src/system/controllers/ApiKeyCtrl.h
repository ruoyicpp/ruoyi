#pragma once
#include <drogon/drogon.h>
#include "../../common/AjaxResult.h"
#include "../../common/ApiKeyService.h"
#include "../../common/PageUtils.h"
#include "../../common/OperLogUtils.h"
#include "../../filters/PermFilter.h"
#include "../../system/services/TokenService.h"

// f16 API Key 管理
//   POST   /system/apikey         创建（仅当时返回明文 key，DB 仅存 sha256）
//   GET    /system/apikey/list    列表（不含 key 明文，仅显示 prefix）
//   PUT    /system/apikey/{id}    更新（启用/禁用 / name / expire）
//   DELETE /system/apikey/{id}    删除（一并失效缓存）
class ApiKeyCtrl : public drogon::HttpController<ApiKeyCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ApiKeyCtrl::create, "/system/apikey",       drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(ApiKeyCtrl::list,   "/system/apikey/list",  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(ApiKeyCtrl::update, "/system/apikey/{id}",  drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(ApiKeyCtrl::remove, "/system/apikey/{id}",  drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    // ── POST /system/apikey ───────────────────────────────────────
    void create(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:apikey:add");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }

        std::string name    = (*json).get("name", "").asString();
        long        userId  = (*json).get("userId", 0).asInt64();
        std::string expire  = (*json).get("expireTime", "").asString();   // YYYY-MM-DD HH:MM:SS
        std::string remark  = (*json).get("remark", "").asString();

        if (name.empty()) { RESP_ERR(cb, "name 不能为空"); return; }
        if (userId <= 0)  { RESP_ERR(cb, "userId 必须 > 0"); return; }

        // 校验关联用户存在
        auto& db = DatabaseService::instance();
        auto uRes = db.queryParams(
            "SELECT user_id FROM sys_user WHERE user_id=$1",
            {std::to_string(userId)});
        if (!uRes.ok() || uRes.rows() == 0) { RESP_ERR(cb, "关联用户不存在"); return; }

        std::string key     = ApiKeyService::generateKey();
        std::string keyHash = ApiKeyService::hashKey(key);
        std::string prefix  = ApiKeyService::keyPrefix(key);

        auto curUser = TokenService::instance().getLoginUser(req);
        std::string creator = curUser ? curUser->userName : "system";

        bool ok;
        if (expire.empty()) {
            ok = db.execParams(
                "INSERT INTO sys_apikey(name,key_hash,key_prefix,user_id,enabled,create_by,remark) "
                "VALUES($1,$2,$3,$4,1,$5,$6)",
                {name, keyHash, prefix, std::to_string(userId), creator, remark});
        } else {
            ok = db.execParams(
                "INSERT INTO sys_apikey(name,key_hash,key_prefix,user_id,expire_time,enabled,create_by,remark) "
                "VALUES($1,$2,$3,$4,$5,1,$6,$7)",
                {name, keyHash, prefix, std::to_string(userId), expire, creator, remark});
        }
        if (!ok) { RESP_ERR(cb, "创建失败"); return; }

        LOG_OPER_PARAM(req, "创建 API Key: " + name, BusinessType::INSERT, "userId=" + std::to_string(userId));

        Json::Value r = AjaxResult::success();
        r["data"] = Json::Value(Json::objectValue);
        r["data"]["key"]       = key;        // ★ 仅此一次返回明文 ★
        r["data"]["keyPrefix"] = prefix;
        r["data"]["name"]      = name;
        r["msg"] = "请妥善保管，此 key 仅本次返回，无法找回";
        RESP_JSON(cb, r);
    }

    // ── GET /system/apikey/list ───────────────────────────────────
    void list(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:apikey:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto name = req->getParameter("name");

        std::string sql = "SELECT id,name,key_prefix,user_id,expire_time,enabled,last_used_at,create_by,create_time,remark "
                          "FROM sys_apikey WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        if (!name.empty()) { sql += " AND name LIKE $" + std::to_string(idx++); params.push_back("%" + name + "%"); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY id DESC LIMIT " + std::to_string(page.pageSize)
             + " OFFSET " + std::to_string(page.offset());
        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);

        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]          = (Json::Int64)res.longVal(i, 0);
            j["name"]        = res.str(i, 1);
            j["keyPrefix"]   = res.str(i, 2);    // 如 "ak_a1b2c3d4"
            j["userId"]      = (Json::Int64)res.longVal(i, 3);
            j["expireTime"]  = res.str(i, 4);
            j["enabled"]     = res.intVal(i, 5);
            j["lastUsedAt"]  = res.str(i, 6);
            j["createBy"]    = res.str(i, 7);
            j["createTime"]  = res.str(i, 8);
            j["remark"]      = res.str(i, 9);
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    // ── PUT /system/apikey/{id} ───────────────────────────────────
    void update(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        CHECK_PERM(req, cb, "system:apikey:edit");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }

        std::string name    = (*json).get("name", "").asString();
        std::string remark  = (*json).get("remark", "").asString();
        int         enabled = (*json).get("enabled", 1).asInt();
        std::string expire  = (*json).get("expireTime", "").asString();

        auto& db = DatabaseService::instance();
        bool ok;
        if (expire.empty()) {
            ok = db.execParams(
                "UPDATE sys_apikey SET name=$1, remark=$2, enabled=$3 WHERE id=$4",
                {name, remark, std::to_string(enabled), std::to_string(id)});
        } else {
            ok = db.execParams(
                "UPDATE sys_apikey SET name=$1, remark=$2, enabled=$3, expire_time=$4 WHERE id=$5",
                {name, remark, std::to_string(enabled), expire, std::to_string(id)});
        }
        if (!ok) { RESP_ERR(cb, "更新失败"); return; }

        // 失效全部缓存（不知道哪个 key 对应此 id，保守清空）
        ApiKeyService::instance().invalidateAll();
        LOG_OPER(req, "更新 API Key id=" + std::to_string(id), BusinessType::UPDATE);
        Json::Value r = AjaxResult::success();
        RESP_JSON(cb, r);
    }

    // ── DELETE /system/apikey/{id} ────────────────────────────────
    void remove(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        CHECK_PERM(req, cb, "system:apikey:remove");
        auto& db = DatabaseService::instance();
        bool ok = db.execParams("DELETE FROM sys_apikey WHERE id=$1", {std::to_string(id)});
        if (!ok) { RESP_ERR(cb, "删除失败"); return; }

        ApiKeyService::instance().invalidateAll();
        LOG_OPER(req, "删除 API Key id=" + std::to_string(id), BusinessType::REMOVE);
        Json::Value r = AjaxResult::success();
        RESP_JSON(cb, r);
    }
};
