#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../common/TokenCache.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../common/OperLogUtils.h"
#include "../../system/services/TokenService.h"
#include "WsNotifyCtrl.h"

/**
 * @file SysOnlineCtrl.h
 * @brief 在线用户管理控制器 — 实时监控和管理在线用户会话
 * 
 * 功能概述：
 *   - 在线用户列表：查看当前所有在线用户
 *   - 用户信息：获取用户登录时间、IP 地址、设备信息
 *   - 强制退出：管理员可强制用户退出登录
 *   - 会话管理：管理用户会话和 Token
 *   - 实时监控：实时显示用户在线状态
 *   - 登录日志：记录用户登录和退出事件
 * 
 * 核心特性：
 *   - 实时更新：基于 TokenCache 的实时在线用户数据
 *   - 多条件查询：支持按用户名、IP 地址、登录时间查询
 *   - 强制退出：支持管理员强制用户退出
 *   - 会话追踪：记录用户会话的完整生命周期
 *   - 安全防护：只有管理员可以查看和管理在线用户
 *   - 事件通知：用户被强制退出时实时通知
 * 
 * API 端点：
 *   - GET /monitor/online/list - 获取在线用户列表
 *   - DELETE /monitor/online/{tokenId} - 强制用户退出
 * 
 * 请求/响应示例：
 *   ```
 *   GET /monitor/online/list?userName=admin&ipaddr=192.168
 *   Authorization: Bearer <JWT>
 *   
 *   响应：
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": [
 *       {
 *         "tokenId": "abc123def456",
 *         "userName": "admin",
 *         "ipAddr": "192.168.1.100",
 *         "loginTime": "2026-06-10 10:30:00",
 *         "deviceName": "Windows 10",
 *         "browser": "Chrome 120"
 *       }
 *     ]
 *   }
 *   ```
 * 
 * 权限要求：
 *   - monitor:online:list - 查看在线用户列表
 *   - monitor:online:forceLogout - 强制用户退出
 * 
 * 配置项（config.json）：
 *   - online.session_timeout: 会话超时时间（分钟，默认 30）
 *   - online.max_sessions_per_user: 每个用户最大会话数（默认 1）
 *   - online.track_login_logout: 是否记录登录退出事件（默认 true）
 * 
 * 用户信息包含：
 *   - tokenId：会话令牌 ID
 *   - userName：用户名
 *   - ipAddr：登录 IP 地址
 *   - loginTime：登录时间
 *   - deviceName：设备名称
 *   - browser：浏览器信息
 *   - lastActivity：最后活动时间
 * 
 * 强制退出流程：
 *   1. 管理员发送强制退出请求
 *   2. 系统从 TokenCache 中删除用户 Token
 *   3. 用户下次请求时认证失败
 *   4. 系统记录强制退出事件
 *   5. 通知用户会话已被终止
 * 
 * @see TokenCache - Token 缓存管理
 * @see TokenService - Token 服务
 * @see WsNotifyCtrl - WebSocket 通知控制器
 */
class SysOnlineCtrl : public drogon::HttpController<SysOnlineCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysOnlineCtrl::list,       "/monitor/online/list",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysOnlineCtrl::forceLogout,"/monitor/online/{tokenId}",   drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    // 在线用户列表：遍历 TokenCache 枚举所有当前在线用户
    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:online:list");
        auto ipaddr   = req->getParameter("ipaddr");
        auto userName = req->getParameter("userName");
        auto allUsers = TokenCache::instance().getAll();
        Json::Value arr(Json::arrayValue);
        for (auto &u : allUsers) {
            if (!ipaddr.empty() && u.ipAddr.find(ipaddr) == std::string::npos) continue;
            if (!userName.empty() && u.userName.find(userName) == std::string::npos) continue;
            Json::Value j;
            j["tokenId"]       = u.token;
            j["userName"]      = u.userName;
            j["ipaddr"]        = u.ipAddr;
            j["loginLocation"] = u.loginLocation;
            j["browser"]       = u.browser;
            j["os"]            = u.os;
            j["loginTime"]     = (Json::Int64)u.loginTime;
            j["deptName"]      = u.deptName;
            arr.append(j);
        }
        PageResult pr; pr.total = (long)arr.size(); pr.rows = arr;
        RESP_JSON(cb, pr.toJson());
    }

    // 强制退出（踢人）
    void forceLogout(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &tokenId) {
        CHECK_PERM(req, cb, "monitor:online:forceLogout");
        auto key    = SecurityUtils::getTokenKey(tokenId);
        // 先取 userId，用于 WS 推送（取完再移除）
        auto cached = TokenCache::instance().get(key);
        long userId = cached ? cached->userId : -1;
        // 移除缓存（JWT 失效）并同步删除持久化记录
        TokenService::instance().delLoginUser(tokenId);
        // 推送 WS 踢人通知（若用户已建立 WS 连接）
        if (userId > 0) WsNotifyCtrl::sendKick(userId);
        LOG_OPER_PARAM(req, "在线用户", BusinessType::FORCE,
                       "强制退出: " + (cached ? cached->userName : tokenId));
        RESP_MSG(cb, "操作成功");
    }
};
