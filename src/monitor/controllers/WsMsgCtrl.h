/**
 * @file WsMsgCtrl.h
 * @brief WebSocket 消息管理 — 在线用户查询 + 全员广播 + 定向推送
 *
 * 功能概述：
 *   - 在线用户：查询当前 WS 连接的用户列表和连接数
 *   - 全员广播：向所有在线连接推送消息（瞬时，不持久化）
 *   - 定向推送：向指定用户推送（瞬时；持久化站内信用 NotifyService::sendInbox）
 *
 * 说明：
 *   - 站内信持久化 + 推送已由 NotifyService::sendInbox 实现
 *     （写 sys_message + WsBus::publish("user:<id>")）
 *   - 本控制器补齐管理面：在线状态查询、全员广播、免持久化推送
 *
 * API 端点：
 *   - GET  /monitor/ws/online     在线用户列表（monitor:online:list）
 *   - POST /system/message/broadcast  全员广播（system:message:send）
 *   - POST /system/message/push       定向推送（system:message:send）
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include <vector>
#include <ctime>
#include "../../common/AjaxResult.h"
#include "../../common/OperLogUtils.h"
#include "../../common/ClusterSession.h"
#include "../../filters/PermFilter.h"
#include "WsNotifyCtrl.h"

/**
 * @class WsMsgCtrl
 * @brief WS 消息管理控制器
 */
class WsMsgCtrl : public drogon::HttpController<WsMsgCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(WsMsgCtrl::online,    "/monitor/ws/online",        drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(WsMsgCtrl::cluster,   "/monitor/ws/cluster",       drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(WsMsgCtrl::broadcast, "/system/message/broadcast", drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(WsMsgCtrl::push,      "/system/message/push",      drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    /// GET /monitor/ws/online — 在线用户列表
    void online(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:online:list");
        auto users = WsNotifyCtrl::onlineUsers();
        Json::Value arr(Json::arrayValue);
        for (long uid : users) arr.append((Json::Int64)uid);
        Json::Value data;
        data["count"] = (Json::Int64)users.size();
        data["userIds"] = arr;
        RESP_OK(cb, data);
    }

    /// GET /monitor/ws/cluster — 集群在线用户（跨节点合并）
    void cluster(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:online:list");
        auto users = ClusterSession::instance().clusterOnlineUsers();
        auto nodes = ClusterSession::instance().clusterNodes();
        Json::Value uarr(Json::arrayValue), narr(Json::arrayValue);
        for (long uid : users) uarr.append((Json::Int64)uid);
        for (auto& n : nodes) narr.append(n);
        Json::Value data;
        data["node_id"]  = ClusterSession::instance().nodeId();
        data["count"]    = (Json::Int64)users.size();
        data["userIds"]  = uarr;
        data["nodes"]    = narr;
        RESP_OK(cb, data);
    }

    /// POST /system/message/broadcast {title, content, level?} — 全员广播
    void broadcast(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:message:send");
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("title")) {
            RESP_ERR(cb, "缺少 title"); return;
        }
        Json::Value msg;
        msg["type"]    = "broadcast";
        msg["title"]   = (*body)["title"];
        msg["content"] = body->get("content", "");
        msg["level"]   = body->get("level", "info");
        msg["ts"]      = (Json::Int64)std::time(nullptr);
        WsNotifyCtrl::broadcastAll(msg);
        LOG_OPER(req, "站内消息", BusinessType::OTHER);
        RESP_MSG(cb, "广播已推送");
    }

    /// POST /system/message/push {user_id, title, content, level?} — 定向推送
    void push(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:message:send");
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("user_id")) {
            RESP_ERR(cb, "缺少 user_id"); return;
        }
        long uid = (*body)["user_id"].asInt64();
        Json::Value msg;
        msg["type"]    = "message";
        msg["title"]   = body->get("title", "");
        msg["content"] = body->get("content", "");
        msg["level"]   = body->get("level", "info");
        msg["ts"]      = (Json::Int64)std::time(nullptr);
        int sent = WsNotifyCtrl::pushToUser(uid, msg);
        if (sent == 0) { RESP_ERR(cb, "用户不在线"); return; }
        RESP_MSG(cb, "已推送到 " + std::to_string(sent) + " 个连接");
    }
};
