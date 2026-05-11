#pragma once
#include <drogon/drogon.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../common/OperLogUtils.h"
#include "../../common/NotifyService.h"
#include "../../filters/PermFilter.h"
#include "../../system/services/TokenService.h"

// f15 通知渠道 CRUD + 测试发送
//   POST   /system/notify/channel           创建渠道
//   GET    /system/notify/channel/list      列表
//   PUT    /system/notify/channel/{id}      更新
//   DELETE /system/notify/channel/{id}      删除
//   POST   /system/notify/channel/{id}/test 测试发送 (body: {title, content})
//   POST   /system/notify/send              业务调用：发送到指定渠道 (body: {channelId, title, content})
class NotifyChannelCtrl : public drogon::HttpController<NotifyChannelCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(NotifyChannelCtrl::create, "/system/notify/channel",            drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::list,   "/system/notify/channel/list",       drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::update, "/system/notify/channel/{id}",       drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::remove, "/system/notify/channel/{id}",       drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::test,   "/system/notify/channel/{id}/test",  drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(NotifyChannelCtrl::send,   "/system/notify/send",               drogon::Post,   "JwtAuthFilter");
    METHOD_LIST_END

    void create(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notify:add");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }
        std::string name = (*json).get("name", "").asString();
        std::string type = (*json).get("channelType", "webhook").asString();
        std::string url  = (*json).get("webhookUrl", "").asString();
        std::string sec  = (*json).get("secret", "").asString();
        std::string remark = (*json).get("remark", "").asString();
        if (name.empty() || url.empty()) { RESP_ERR(cb, "name/webhookUrl 必填"); return; }
        if (type != "dingtalk" && type != "feishu" && type != "wxwork" && type != "webhook") {
            RESP_ERR(cb, "channelType 仅支持 dingtalk/feishu/wxwork/webhook"); return;
        }
        auto curUser = TokenService::instance().getLoginUser(req);
        std::string creator = curUser ? curUser->userName : "system";

        bool ok = DatabaseService::instance().execParams(
            "INSERT INTO sys_notify_channel(name,channel_type,webhook_url,secret,enabled,create_by,remark) "
            "VALUES($1,$2,$3,$4,1,$5,$6)",
            {name, type, url, sec, creator, remark});
        if (!ok) { RESP_ERR(cb, "创建失败"); return; }
        LOG_OPER_PARAM(req, "创建通知渠道: " + name, BusinessType::INSERT, "type=" + type);
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void list(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notify:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto name = req->getParameter("name");
        std::string sql = "SELECT id,name,channel_type,webhook_url,enabled,create_by,create_time,remark "
                          "FROM sys_notify_channel WHERE 1=1";
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
            j["channelType"] = res.str(i, 2);
            // 不暴露完整 webhookUrl，前端只看到 host 部分
            std::string url = res.str(i, 3);
            auto pos = url.find('/', url.find("://") + 3);
            j["webhookUrl"] = (pos == std::string::npos) ? url
                                                         : (url.substr(0, pos) + "/...");
            j["enabled"]     = res.intVal(i, 4);
            j["createBy"]    = res.str(i, 5);
            j["createTime"]  = res.str(i, 6);
            j["remark"]      = res.str(i, 7);
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void update(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        CHECK_PERM(req, cb, "system:notify:edit");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }
        std::string name = (*json).get("name", "").asString();
        std::string type = (*json).get("channelType", "webhook").asString();
        std::string url  = (*json).get("webhookUrl", "").asString();
        std::string sec  = (*json).get("secret", "").asString();
        std::string remark = (*json).get("remark", "").asString();
        int enabled = (*json).get("enabled", 1).asInt();
        bool ok = DatabaseService::instance().execParams(
            "UPDATE sys_notify_channel SET name=$1, channel_type=$2, webhook_url=$3, "
            "secret=$4, enabled=$5, remark=$6 WHERE id=$7",
            {name, type, url, sec, std::to_string(enabled), remark, std::to_string(id)});
        if (!ok) { RESP_ERR(cb, "更新失败"); return; }
        LOG_OPER(req, "更新通知渠道 id=" + std::to_string(id), BusinessType::UPDATE);
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void remove(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        CHECK_PERM(req, cb, "system:notify:remove");
        bool ok = DatabaseService::instance().execParams(
            "DELETE FROM sys_notify_channel WHERE id=$1", {std::to_string(id)});
        if (!ok) { RESP_ERR(cb, "删除失败"); return; }
        LOG_OPER(req, "删除通知渠道 id=" + std::to_string(id), BusinessType::REMOVE);
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void test(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb,
              long id) {
        CHECK_PERM(req, cb, "system:notify:edit");
        auto json = req->getJsonObject();
        std::string title   = json ? (*json).get("title",   "RuoYi-Cpp 测试通知").asString() : "测试通知";
        std::string content = json ? (*json).get("content", "这是一条测试消息。").asString() : "test";
        bool sent = NotifyService::sendToChannel(id, title, content);
        if (!sent) { RESP_ERR(cb, "渠道不存在或被禁用"); return; }
        Json::Value r = AjaxResult::success();
        r["msg"] = "已发送（异步），请到目标群查看";
        RESP_JSON(cb, r);
    }

    void send(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:notify:send");
        auto json = req->getJsonObject();
        if (!json) { RESP_ERR(cb, "请求体为空"); return; }
        long channelId      = (*json).get("channelId", 0).asInt64();
        std::string title   = (*json).get("title", "").asString();
        std::string content = (*json).get("content", "").asString();
        if (channelId <= 0 || title.empty() || content.empty()) {
            RESP_ERR(cb, "channelId/title/content 必填"); return;
        }
        bool sent = NotifyService::sendToChannel(channelId, title, content);
        if (!sent) { RESP_ERR(cb, "渠道不存在或被禁用"); return; }
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }
};

// f15 站内消息 CRUD
//   GET    /system/message/inbox             收件箱（按 user_id 过滤）
//   GET    /system/message/unread            未读条数
//   POST   /system/message/{id}/read         标记单条已读
//   POST   /system/message/readAll           全部已读
//   DELETE /system/message/{id}              删除单条
class MessageCtrl : public drogon::HttpController<MessageCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MessageCtrl::inbox,   "/system/message/inbox",     drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(MessageCtrl::unread,  "/system/message/unread",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(MessageCtrl::read,    "/system/message/{id}/read", drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(MessageCtrl::readAll, "/system/message/readAll",   drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(MessageCtrl::remove,  "/system/message/{id}",      drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void inbox(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto countRes = db.queryParams(
            "SELECT COUNT(*) FROM sys_message WHERE user_id=$1",
            {std::to_string(user->userId)});
        long total = (countRes.ok() && countRes.rows() > 0) ? countRes.longVal(0, 0) : 0;
        auto res = db.queryParams(
            "SELECT id,title,content,level,is_read,create_time,read_time "
            "FROM sys_message WHERE user_id=$1 ORDER BY id DESC LIMIT $2 OFFSET $3",
            {std::to_string(user->userId),
             std::to_string(page.pageSize),
             std::to_string(page.offset())});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]         = (Json::Int64)res.longVal(i, 0);
            j["title"]      = res.str(i, 1);
            j["content"]    = res.str(i, 2);
            j["level"]      = res.str(i, 3);
            j["isRead"]     = res.intVal(i, 4) == 1;
            j["createTime"] = res.str(i, 5);
            j["readTime"]   = res.str(i, 6);
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void unread(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto res = DatabaseService::instance().queryParams(
            "SELECT COUNT(*) FROM sys_message WHERE user_id=$1 AND is_read=0",
            {std::to_string(user->userId)});
        long n = (res.ok() && res.rows() > 0) ? res.longVal(0, 0) : 0;
        Json::Value r = AjaxResult::success();
        r["count"] = (Json::Int64)n;
        RESP_JSON(cb, r);
    }

    void read(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb,
              long id) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_message SET is_read=1, read_time=CURRENT_TIMESTAMP "
            "WHERE id=$1 AND user_id=$2 AND is_read=0",
            {std::to_string(id), std::to_string(user->userId)});
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void readAll(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_message SET is_read=1, read_time=CURRENT_TIMESTAMP "
            "WHERE user_id=$1 AND is_read=0",
            {std::to_string(user->userId)});
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }

    void remove(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                long id) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        DatabaseService::instance().execParams(
            "DELETE FROM sys_message WHERE id=$1 AND user_id=$2",
            {std::to_string(id), std::to_string(user->userId)});
        Json::Value r = AjaxResult::success(); RESP_JSON(cb, r);
    }
};
