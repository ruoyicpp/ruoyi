#pragma once
#include <drogon/WebSocketController.h>
#include <drogon/HttpRequest.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>
#include "../../common/JwtUtils.h"
#include "../../common/TokenCache.h"
#include "../../common/SecurityUtils.h"

/**
 * WsNotifyCtrl — 服务端推送通知 WebSocket
 * 路由：GET /ws/notify?token=<JWT>
 *
 * 功能：
 *   - 客户端登录后连接，token 鉴权
 *   - 被强制踢出时，服务端推送 {"type":"kick","msg":"您已被管理员强制下线"}
 *     并主动关闭连接
 */
class WsNotifyCtrl : public drogon::WebSocketController<WsNotifyCtrl> {
public:
    WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/ws/notify");
    WS_PATH_LIST_END

    // ── 新连接建立 ──────────────────────────────────────────────────────────
    void handleNewConnection(const drogon::HttpRequestPtr &req,
                             const drogon::WebSocketConnectionPtr &conn) override {
        std::string jwtToken = req->getParameter("token");
        if (jwtToken.empty()) {
            conn->send("{\"type\":\"error\",\"msg\":\"缺少 token 参数\"}");
            conn->shutdown();
            return;
        }
        try {
            auto uuid   = JwtUtils::parseUuid(jwtToken);
            auto cached = TokenCache::instance().get(SecurityUtils::getTokenKey(uuid));
            if (!cached) {
                conn->send("{\"type\":\"error\",\"msg\":\"token 无效或已过期\"}");
                conn->shutdown();
                return;
            }
            long userId = cached->userId;
            conn->setContext(std::make_shared<ConnCtx>(uuid, userId));
            addConn(userId, conn);
            LOG_INFO << "[WsNotify] userId=" << userId << " 已连接";
        } catch (...) {
            conn->send("{\"type\":\"error\",\"msg\":\"token 解析失败\"}");
            conn->shutdown();
        }
    }

    // ── 连接断开 ────────────────────────────────────────────────────────────
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr &conn) override {
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
