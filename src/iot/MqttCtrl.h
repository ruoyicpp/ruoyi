/**
 * @file MqttCtrl.h
 * @brief MQTT 设备管理控制器 — 设备在线状态/下行指令/消息记录
 *
 * 功能概述：
 *   - 在线设备：iot_device 表 status/last_seen 查询（MqttClient 自动更新）
 *   - 下行指令：POST /iot/mqtt/publish → MqttClient::publish 发指令到设备
 *   - 消息记录：iot_message 表分页查询（mqtt.persist=true 时启用）
 *   - 连接状态：MqttClient::isConnected 查询 broker 连接
 *
 * API 端点（均需登录 + iot:mqtt:* 权限）：
 *   - GET  /iot/mqtt/status        broker 连接状态
 *   - GET  /iot/mqtt/devices       设备在线列表（iot_device）
 *   - POST /iot/mqtt/publish       下行指令 {topic, payload}
 *   - GET  /iot/mqtt/messages      消息记录分页
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include "../common/AjaxResult.h"
#include "../common/OperLogUtils.h"
#include "../common/PageUtils.h"
#include "../filters/PermFilter.h"
#include "../services/DatabaseService.h"
#include "MqttClient.h"

/**
 * @class MqttCtrl
 * @brief MQTT 设备管理控制器
 */
class MqttCtrl : public drogon::HttpController<MqttCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MqttCtrl::status,   "/iot/mqtt/status",   drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(MqttCtrl::devices,  "/iot/mqtt/devices",  drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(MqttCtrl::publish,  "/iot/mqtt/publish",  drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(MqttCtrl::messages, "/iot/mqtt/messages", drogon::Get,  "JwtAuthFilter");
    METHOD_LIST_END

    /// GET /iot/mqtt/status — broker 连接状态
    void status(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "iot:mqtt:list");
        Json::Value j;
        j["connected"] = MqttClient::instance().isConnected();
        RESP_OK(cb, j);
    }

    /// GET /iot/mqtt/devices — 设备在线列表
    void devices(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "iot:mqtt:list");
        auto res = DatabaseService::instance().query(
            "SELECT id,name,status,last_seen FROM iot_device "
            "WHERE status IS NOT NULL ORDER BY status DESC, last_seen DESC");
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]        = res.str(i, 0);
            j["name"]      = res.str(i, 1);
            j["status"]    = res.str(i, 2);
            j["last_seen"] = res.str(i, 3);
            rows.append(j);
        }
        RESP_OK(cb, rows);
    }

    /// POST /iot/mqtt/publish {topic, payload} — 下行指令
    void publish(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "iot:mqtt:send");
        auto b = req->getJsonObject();
        if (!b || !(*b).isMember("topic") || !(*b).isMember("payload")) {
            RESP_ERR(cb, "缺少 topic/payload"); return;
        }
        if (!MqttClient::instance().publish((*b)["topic"].asString(),
                                            (*b)["payload"].asString())) {
            RESP_ERR(cb, "MQTT 未连接"); return;
        }
        LOG_OPER_PARAM(req, "MQTT指令", BusinessType::OTHER, (*b)["topic"].asString());
        RESP_MSG(cb, "已发送");
    }

    /// GET /iot/mqtt/messages — 消息记录分页
    void messages(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "iot:mqtt:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto cnt = db.query("SELECT COUNT(*) FROM iot_message");
        long total = (cnt.ok() && cnt.rows() > 0) ? cnt.longVal(0, 0) : 0;
        auto res = db.queryParams(
            "SELECT id,topic,payload,create_time FROM iot_message "
            "ORDER BY id DESC LIMIT $1 OFFSET $2",
            {std::to_string(page.pageSize), std::to_string(page.offset())});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]          = (Json::Int64)res.longVal(i, 0);
            j["topic"]       = res.str(i, 1);
            j["payload"]     = res.str(i, 2);
            j["create_time"] = res.str(i, 3);
            rows.append(j);
        }
        Json::Value data; data["total"] = (Json::Int64)total; data["rows"] = rows;
        RESP_OK(cb, data);
    }
};
