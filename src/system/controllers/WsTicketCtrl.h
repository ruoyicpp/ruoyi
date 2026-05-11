#pragma once
/**
 * WsTicketCtrl —— 一次性 WebSocket 票据签发
 *
 * 端点：GET /ws/ticket
 *
 * 流程：
 *   1) JwtAuthFilter 已校验 Bearer token，能取到当前 userId / username
 *   2) 生成 32 字节随机 ticket（hex 字符串）
 *   3) 写入 MemCache，60 秒过期，value = userId:userName
 *   4) 前端用 ?ticket=xxx 升级 ws，WsAuthFilter（如有）取 ticket → 反查 cache → 通过
 *
 * 设计要点：
 *   - 不复用 JWT：浏览器 ws 升级请求无法附 Authorization 头
 *   - 一次性：getString 命中后 cache 立刻 remove（防止重放）
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
