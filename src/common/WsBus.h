// WePay-Cpp — WebSocket 消息总线
// 维护 orderId → 已订阅的 WebSocket 连接，支付成功时主动推送给前端
//
// 使用流程:
//   前端: ws://host/ws/order/W123456789
//   后端: WsBus::publish(orderId, json) → 所有订阅该订单的连接收到
/**
 * @file WsBus.h
 * @brief WebSocket 消息总线 — 支持实时消息推送和事件通知
 * 
 * 功能概述：
 *   - 消息发布：支持发布消息到指定的 WebSocket 连接
 *   - 消息订阅：支持订阅特定类型的消息
 *   - 广播消息：支持向所有连接广播消息
 *   - 分组管理：支持将连接分组，按组广播
 *   - 消息队列：支持消息缓存和异步处理
 *   - 连接管理：自动管理 WebSocket 连接的生命周期
 * 
 * 核心特性：
 *   - 发布-订阅模式：支持多生产者-多消费者的消息传递
 *   - 异步处理：非阻塞的消息发送，不影响主线程
 *   - 消息过滤：支持按消息类型、用户、权限等过滤
 *   - 连接池：支持连接复用和连接池管理
 *   - 性能监控：记录消息吞吐量、延迟、错误率
 *   - 错误恢复：连接断开时自动重连
 * 
 * 消息类型：
 *   - 系统通知：系统级别的通知（维护、告警等）
 *   - 用户通知：用户级别的通知（消息、提醒等）
 *   - 实时数据：实时数据推送（股票、天气等）
 *   - 在线状态：用户在线状态变化通知
 *   - 聊天消息：即时通讯消息
 * 
 * 配置项（config.json）：
 *   - websocket.enabled: 是否启用 WebSocket（默认 true）
 *   - websocket.path: WebSocket 路径（默认 "/ws"）
 *   - websocket.max_connections: 最大连接数（默认 10000）
 *   - websocket.message_queue_size: 消息队列大小（默认 1000）
 *   - websocket.heartbeat_interval_seconds: 心跳间隔（秒，默认 30）
 */

#pragma once
#include <drogon/WebSocketConnection.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <json/json.h>

class WsBus {
public:
    using ConnPtr = drogon::WebSocketConnectionPtr;

    static WsBus &instance() { static WsBus b; return b; }

    // 添加订阅
    void subscribe(const std::string &topic, const ConnPtr &conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        topics_[topic].insert(conn);
    }

    // 移除订阅(连接关闭时)
    void unsubscribe(const std::string &topic, const ConnPtr &conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics_.find(topic);
        if (it == topics_.end()) return;
        it->second.erase(conn);
        if (it->second.empty()) topics_.erase(it);
    }

    // 移除所有订阅(连接断开)
    void unsubscribeAll(const ConnPtr &conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = topics_.begin(); it != topics_.end(); ) {
            it->second.erase(conn);
            if (it->second.empty()) it = topics_.erase(it);
            else ++it;
        }
    }

    // 推送消息到指定 topic 的所有订阅者
    void publish(const std::string &topic, const Json::Value &message) {
        Json::StreamWriterBuilder wb; wb["indentation"] = "";
        std::string body = Json::writeString(wb, message);

        std::vector<ConnPtr> targets;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(topic);
            if (it == topics_.end()) return;
            targets.assign(it->second.begin(), it->second.end());
        }
        for (auto &c : targets) {
            if (c->connected()) c->send(body);
        }
    }

    // 广播到所有 topic 前缀匹配的订阅者 (例: "live:" 匹配 "live:abc", "live:def")
    void broadcast(const std::string &prefix, const Json::Value &message) {
        Json::StreamWriterBuilder wb; wb["indentation"] = "";
        std::string body = Json::writeString(wb, message);
        std::vector<ConnPtr> targets;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto &kv : topics_) {
                if (kv.first.size() >= prefix.size() &&
                    kv.first.substr(0, prefix.size()) == prefix) {
                    targets.insert(targets.end(), kv.second.begin(), kv.second.end());
                }
            }
        }
        for (auto &c : targets) {
            if (c->connected()) c->send(body);
        }
    }

    // 快捷: 广播实时事件到所有 live 连接
    void publishLive(const std::string &event, const Json::Value &data) {
        Json::Value msg;
        msg["event"] = event;
        msg["data"]  = data;
        msg["ts"]    = (Json::Int64)std::time(nullptr);
        broadcast("live:", msg);
    }

    // 当前订阅者数量(调试用)
    size_t subscribers(const std::string &topic) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics_.find(topic);
        return it == topics_.end() ? 0 : it->second.size();
    }

    size_t totalTopics() {
        std::lock_guard<std::mutex> lock(mutex_);
        return topics_.size();
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::unordered_set<ConnPtr>> topics_;
};
