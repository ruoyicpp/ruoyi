#pragma once
/**
 * @file WsTicketCtrl.h
 * @brief WebSocket 票据签发控制器 — 为 WebSocket 连接生成一次性认证票据
 * 
 * 功能概述：
 *   - 票据生成：生成一次性的 WebSocket 认证票据
 *   - 票据验证：WebSocket 连接时使用票据进行身份验证
 *   - 防重放：票据一次性使用，防止重放攻击
 *   - 自动过期：票据 60 秒后自动过期
 * 
 * 核心特性：
 *   - 一次性使用：票据使用后立即删除，防止重放
 *   - 随机生成：使用 OpenSSL RAND_bytes 生成 32 字节随机数
 *   - 短期有效：票据有效期仅 60 秒，降低被盗用风险
 *   - 用户绑定：票据与用户 ID 和用户名绑定
 * 
 * 工作流程：
 *   1. 前端使用 JWT Token 调用 GET /ws/ticket 获取票据
 *   2. 后端验证 JWT Token（JwtAuthFilter）
 *   3. 后端生成 32 字节随机数，转换为 64 字符十六进制字符串
 *   4. 将票据存入 MemCache，60 秒过期，值为 "userId:userName"
 *   5. 返回票据给前端
 *   6. 前端使用 ?ticket=xxx 升级 WebSocket 连接
 *   7. WebSocket 连接时，验证票据有效性
 *   8. 验证成功后，从 MemCache 删除票据（一次性）
 * 
 * 为什么需要票据？
 *   - 浏览器 WebSocket 升级请求无法附加 Authorization 头
 *   - 无法直接在 WebSocket 握手时传递 JWT Token
 *   - 需要一个临时的、一次性的认证凭证
 * 
 * API 端点：
 *   - GET /ws/ticket - 获取 WebSocket 票据
 * 
 * 请求示例：
 *   ```
 *   GET /ws/ticket HTTP/1.1
 *   Authorization: Bearer <JWT_TOKEN>
 *   ```
 * 
 * 响应示例：
 *   ```json
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": {
 *       "ticket": "a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6"
 *     }
 *   }
 *   ```
 * 
 * 权限要求：
 *   - 需要有效的 JWT Token（任何已登录用户）
 * 
 * @see WsNotifyCtrl - WebSocket 通知控制器
 * @see WsBus - WebSocket 消息总线
 * @see JwtAuthFilter - JWT 认证过滤器
 */
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/TokenCache.h"
#include "../../filters/PermFilter.h"
#include <openssl/rand.h>
#include <json/json.h>
#include <string>
#include <cstdio>

class WsTicketCtrl : public drogon::HttpController<WsTicketCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(WsTicketCtrl::issue, "/ws/ticket", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    void issue(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        if (userId <= 0) { RESP_401(cb); return; }
        std::string userName = GET_USER_NAME(req);

        // 32 字节随机 → 64 hex
        unsigned char raw[32];
        if (RAND_bytes(raw, sizeof(raw)) != 1) {
            RESP_ERR(cb, "票据生成失败"); return;
        }
        char hex[65] = {};
        for (int i = 0; i < 32; ++i) std::snprintf(hex + i*2, 3, "%02x", raw[i]);

        std::string cacheKey = "ws:ticket:" + std::string(hex);
        std::string val = std::to_string(userId) + ":" + userName;
        MemCache::instance().setString(cacheKey, val, 60); // 60s

        Json::Value j;
        j["ticket"] = std::string(hex);
        j["expiresIn"] = 60;
        RESP_OK(cb, j);
    }

    // 工具函数：给 WsNotifyCtrl 等地方校验 ticket（一次性，命中后 remove）
    // 调用方：auto info = WsTicketCtrl::consume("xxxx"); if (info) { ... }
    struct TicketInfo { long userId; std::string userName; };
    static std::optional<TicketInfo> consume(const std::string& ticket) {
        if (ticket.empty()) return std::nullopt;
        std::string key = "ws:ticket:" + ticket;
        auto val = MemCache::instance().getString(key);
        if (!val) return std::nullopt;
        MemCache::instance().remove(key);
        // 解析 "userId:userName"
        auto colon = val->find(':');
        if (colon == std::string::npos) return std::nullopt;
        TicketInfo info;
        try { info.userId = std::stol(val->substr(0, colon)); }
        catch (...) { return std::nullopt; }
        info.userName = val->substr(colon + 1);
        return info;
    }
};
