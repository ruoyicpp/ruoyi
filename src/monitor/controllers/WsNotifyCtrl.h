#pragma once
#include <drogon/WebSocketController.h>
#include <drogon/HttpRequest.h>
#include <algorithm>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>
#include "../../common/JwtUtils.h"
#include "../../common/TokenCache.h"
#include "../../common/SecurityUtils.h"
#include "../../common/WsBus.h"
#include "../../system/controllers/WsTicketCtrl.h"

/**
 * @file WsNotifyCtrl.h
 * @brief WebSocket 通知控制器 — 实时推送系统通知和事件
 * 
 * 功能概述：
 *   - 实时通知：推送系统通知、告警、消息等
 *   - 用户踢出：强制踢出用户，推送踢出通知
 *   - 在线状态：推送用户在线状态变化
 *   - 系统事件：推送系统事件（维护、更新等）
 *   - 心跳检测：定期发送心跳，检测连接状态
 * 
 * 核心特性：
 *   - JWT 认证：使用 JWT Token 进行身份验证
 *   - 连接管理：自动管理 WebSocket 连接的生命周期
 *   - 消息队列：支持消息缓存和异步推送
 *   - 错误恢复：连接断开时自动清理资源
 *   - 性能监控：记录连接数、消息吞吐量等指标
 * 
 * WebSocket 路由：
 *   - GET /ws/notify?token=<JWT> - 连接通知 WebSocket
 * 
 * 消息格式：
 *   ```json
 *   {
 *     "type": "kick|notify|online|system",
 *     "msg": "消息内容",
 *     "timestamp": 1623456789000,
 *     "data": { ... }
 *   }
 *   ```
 * 
 * 消息类型：
 *   - kick：用户被强制踢出（管理员操作）
 *   - notify：系统通知（告警、消息等）
 *   - online：在线状态变化
 *   - system：系统事件（维护、更新等）
 * 
 * 使用示例：
 *   ```javascript
 *   const ws = new WebSocket('ws://localhost:18080/ws/notify?token=' + token);
 *   ws.onmessage = (event) => {
 *     const msg = JSON.parse(event.data);
 *     if (msg.type === 'kick') {
 *       // 处理被踢出
 *       alert(msg.msg);
 *       location.href = '/login';
 *     }
 *   };
 *   ```
 * 
 * @see WsBus - WebSocket 消息总线
 * @see TokenService - Token 管理服务
 * @see SecurityUtils - 安全工具
 */
class WsNotifyCtrl : public drogon::WebSocketController<WsNotifyCtrl> {
public:
    WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/ws/notify");
    WS_PATH_LIST_END

    // ── 新连接建立 ──────────────────────────────────────────────────────────
    void handleNewConnection(const drogon::HttpRequestPtr &req,
                             const drogon::WebSocketConnectionPtr &conn) override {
        // ── rate limit：同一 IP 每分钟最多 10 次 WS 握手 ──────────────────
        {
            std::string ip = req->peerAddr().toIp();
            std::lock_guard<std::mutex> lk(rateMu());
            auto &rec = wsRate()[ip];
            auto now = std::chrono::steady_clock::now();
            // 清除超过 60s 的记录
            while (!rec.empty() && (now - rec.front()) > std::chrono::seconds(60))
                rec.pop_front();
            if (rec.size() >= 10) {
                conn->send("{\"type\":\"error\",\"msg\":\"连接过于频繁，请稍后再试\"}");
                conn->shutdown();
                return;
            }
            rec.push_back(now);
        }

        long userId = 0;
        std::string uuid;

        std::string ticket   = req->getParameter("ticket");
        std::string jwtToken = req->getParameter("token");

        if (!ticket.empty()) {
            // ── 优先走 ticket 认证（安全：一次性，避免 JWT 出现在 URL）──────
            auto info = WsTicketCtrl::consume(ticket);
            if (!info) {
                conn->send("{\"type\":\"error\",\"msg\":\"ticket 无效或已过期\"}");
                conn->shutdown();
                return;
            }
            userId = info->userId;
            uuid   = "ticket:" + std::to_string(userId);
        } else if (!jwtToken.empty()) {
            // ── fallback：直接 JWT token 认证 ─────────────────────────────
            try {
                uuid = JwtUtils::parseUuid(jwtToken);
                auto cached = TokenCache::instance().get(SecurityUtils::getTokenKey(uuid));
                if (!cached) {
                    conn->send("{\"type\":\"error\",\"msg\":\"token 无效或已过期\"}");
                    conn->shutdown();
                    return;
                }
                userId = cached->userId;
            } catch (...) {
                conn->send("{\"type\":\"error\",\"msg\":\"token 解析失败\"}");
                conn->shutdown();
                return;
            }
        } else {
            conn->send("{\"type\":\"error\",\"msg\":\"缺少 ticket 或 token 参数\"}");
            conn->shutdown();
            return;
        }

        conn->setContext(std::make_shared<ConnCtx>(uuid, userId));
        addConn(userId, conn);
        WsBus::instance().subscribe("user:" + std::to_string(userId), conn);
        LOG_INFO << "[WsNotify] userId=" << userId << " 已连接 + 订阅 user:" << userId;
    }

    // ── 连接断开 ────────────────────────────────────────────────────────────
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr &conn) override {
        // f15: 取消全部 WsBus 订阅（包含 user:<id> 及任何 broadcast 前缀）
        WsBus::instance().unsubscribeAll(conn);
        auto ctx = conn->getContext<ConnCtx>();
        if (ctx) {
            removeConn(ctx->userId, conn);
            LOG_INFO << "[WsNotify] userId=" << ctx->userId << " 已断开";
        }
    }

    // ── 收到客户端消息（心跳 ping）──────────────────────────────────────────
    void handleNewMessage(const drogon::WebSocketConnectionPtr &conn,
                          std::string &&msg,
                          const drogon::WebSocketMessageType &type) override {
        if (type == drogon::WebSocketMessageType::Ping) {
            conn->send("", drogon::WebSocketMessageType::Pong);
        }
    }

    // ── 静态踢人接口（供其他 Controller 调用）──────────────────────────────
    static void sendKick(long userId) {
        std::lock_guard<std::mutex> lk(mu());
        auto it = conns().find(userId);
        if (it == conns().end()) return;
        const std::string msg = R"({"type":"kick","msg":"您已被管理员强制下线，请重新登录"})";
        for (auto &wp : it->second) {
            auto sp = wp.lock();
            if (sp && sp->connected()) {
                sp->send(msg);
                sp->shutdown();
            }
        }
        conns().erase(it);
        LOG_INFO << "[WsNotify] 已推送踢人消息 userId=" << userId;
    }

private:
    struct ConnCtx {
        std::string tokenUuid;
        long        userId;
        ConnCtx(std::string u, long i) : tokenUuid(std::move(u)), userId(i) {}
    };

    // ── WS 握手 rate limit（per-IP 滑动窗口）────────────────────────────────
    using RateMap = std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>>;
    static RateMap& wsRate()   { static RateMap m; return m; }
    static std::mutex& rateMu() { static std::mutex m; return m; }

    // ── 连接池（userId → weak_ptr 列表，支持多端登录）──────────────────────
    using ConnMap = std::unordered_map<long, std::vector<std::weak_ptr<drogon::WebSocketConnection>>>;

    static ConnMap& conns() { static ConnMap m; return m; }
    static std::mutex& mu()  { static std::mutex m; return m; }

    static void addConn(long userId, const drogon::WebSocketConnectionPtr &conn) {
        std::lock_guard<std::mutex> lk(mu());
        conns()[userId].push_back(conn);
    }

    static void removeConn(long userId, const drogon::WebSocketConnectionPtr &conn) {
        std::lock_guard<std::mutex> lk(mu());
        auto it = conns().find(userId);
        if (it == conns().end()) return;
        auto &v = it->second;
        v.erase(std::remove_if(v.begin(), v.end(),
            [&](const std::weak_ptr<drogon::WebSocketConnection>& wp){
                auto sp = wp.lock();
                return !sp || sp.get() == conn.get();
            }), v.end());
        if (v.empty()) conns().erase(it);
    }
};
