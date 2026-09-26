/**
 * @file SsoCtrl.h
 * @brief SSO 控制器 — OAuth2.0/OIDC 端点 + 接入方管理
 *
 * 功能概述：
 *   - 公开端点：authorize/token/userinfo/introspect/revoke/discovery
 *   - 管理端点：clients 增删查（需 system:sso:* 权限）
 *   - 跨域支持：authorize/token/userinfo 走全局 CORS 配置
 *
 * API 端点：
 *   GET  /sso/authorize?response_type=code&client_id=&redirect_uri=&state=&scope=
 *        — 需已登录（Authorization 头或 ?token=），302 回调带 code
 *   POST /sso/token — grant_type=authorization_code|refresh_token
 *   GET  /sso/userinfo — Bearer token → 用户信息+角色+权限
 *   POST /sso/introspect — RFC 7662 令牌内省（异构后端校验用）
 *   POST /sso/revoke — 撤销刷新令牌
 *   GET  /.well-known/openid-configuration — OIDC 发现
 *   GET/POST/DELETE /sso/clients — 接入方管理（管理员）
 *
 * 平台说明：仅 Linux 版本可用（非 Linux 返回 501）
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include <cstdlib>
#include <memory>
#include "../common/AjaxResult.h"
#include "../common/OperLogUtils.h"
#include "../common/SecurityUtils.h"
#include "../filters/PermFilter.h"
#include "../system/services/TokenService.h"
#include "SsoServer.h"

/**
 * @class SsoCtrl
 * @brief SSO 单点登录控制器
 */
class SsoCtrl : public drogon::HttpController<SsoCtrl> {
public:
    /// 非 Linux 平台统一返回"不可用"
    static bool ssoUnavailable(std::function<void(const drogon::HttpResponsePtr&)>& cb) {
#ifndef __linux__
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            Json::Value("SSO 功能仅在 Linux 构建中可用"));
        resp->setStatusCode(drogon::k501NotImplemented);
        cb(resp);
        return true;
#else
        (void)cb;
        return false;
#endif
    }

    METHOD_LIST_BEGIN
        // 公开端点（authorize 内部校验登录态，token 校验 client_secret）
        ADD_METHOD_TO(SsoCtrl::authorize,  "/sso/authorize",   drogon::Get);
        ADD_METHOD_TO(SsoCtrl::token,      "/sso/token",       drogon::Post);
        ADD_METHOD_TO(SsoCtrl::userinfo,   "/sso/userinfo",    drogon::Get);
        ADD_METHOD_TO(SsoCtrl::introspect, "/sso/introspect",  drogon::Post);
        ADD_METHOD_TO(SsoCtrl::revoke,     "/sso/revoke",      drogon::Post);
        ADD_METHOD_TO(SsoCtrl::discovery,  "/.well-known/openid-configuration", drogon::Get);
        // 管理端点（需登录 + 权限）
        ADD_METHOD_TO(SsoCtrl::clients,    "/sso/clients",     drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SsoCtrl::addClient,  "/sso/clients",     drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SsoCtrl::delClient,  "/sso/clients/{id}",drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    /// GET /sso/authorize — 签发授权码并 302 回调
    void authorize(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        if (ssoUnavailable(cb)) return;
        if (!SsoServer::instance().isEnabled()) {
            RESP_ERR(cb, "SSO 未启用"); return;
        }
        std::string clientId    = req->getParameter("client_id");
        std::string redirectUri = req->getParameter("redirect_uri");
        std::string state       = req->getParameter("state");
        std::string respType    = req->getParameter("response_type");

        if (respType != "code") {
            RESP_ERR(cb, "仅支持 response_type=code"); return;
        }
        if (!SsoServer::instance().validateClient(clientId, redirectUri)) {
            RESP_ERR(cb, "client_id 或 redirect_uri 未注册"); return;
        }
        // 要求已登录（JWT 经 Authorization 头或 ?token= 传入）
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) {
            RESP_ERR(cb, "请先登录（未携带有效 token）"); return;
        }

        std::string code = SsoServer::instance().issueCode(
            *user, clientId, redirectUri);

        // 302 回调：redirect_uri?code=xx&state=yy
        std::string sep = redirectUri.find('?') == std::string::npos ? "?" : "&";
        std::string loc = redirectUri + sep + "code=" + code;
        if (!state.empty()) loc += "&state=" + state;

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k302Found);
        resp->addHeader("Location", loc);
        cb(resp);
    }

    /// POST /sso/token — 授权码/刷新令牌换 access_token
    void token(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        if (ssoUnavailable(cb)) return;
        if (!SsoServer::instance().isEnabled()) {
            RESP_ERR(cb, "SSO 未启用"); return;
        }
        // 支持 form 和 JSON 两种提交
        auto getP = [&](const char* k) -> std::string {
            auto v = req->getParameter(k);
            if (!v.empty()) return v;
            auto j = req->getJsonObject();
            return (j && (*j).isMember(k)) ? (*j)[k].asString() : "";
        };
        std::string grantType = getP("grant_type");
        std::string clientId  = getP("client_id");
        std::string secret    = getP("client_secret");

        if (!SsoServer::instance().validateSecret(clientId, secret)) {
            RESP_ERR(cb, "client 认证失败"); return;
        }

        std::optional<Json::Value> result;
        if (grantType == "authorization_code") {
            result = SsoServer::instance().exchangeCode(
                getP("code"), clientId, getP("redirect_uri"));
        } else if (grantType == "refresh_token") {
            result = SsoServer::instance().refreshToken(getP("refresh_token"));
        } else {
            RESP_ERR(cb, "不支持的 grant_type: " + grantType); return;
        }

        if (!result) { RESP_ERR(cb, "票据无效或已过期"); return; }
        // OAuth2 标准响应：直接返回 token JSON（不包 AjaxResult）
        cb(drogon::HttpResponse::newHttpJsonResponse(*result));
    }

    /// GET /sso/userinfo — Bearer token → 用户信息 + 角色 + 权限
    void userinfo(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        if (ssoUnavailable(cb)) return;
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse(
                Json::Value("invalid token"));
            resp->setStatusCode(drogon::k401Unauthorized);
            cb(resp); return;
        }
        Json::Value j;
        j["sub"]      = std::to_string(user->userId);   // OIDC subject
        j["name"]     = user->userName;
        j["dept_id"]  = (Json::Int64)user->deptId;
        j["dept_name"]= user->deptName;
        for (auto& r : user->roles)       j["roles"].append(r);
        for (auto& p : user->permissions) j["permissions"].append(p);
        cb(drogon::HttpResponse::newHttpJsonResponse(j));
    }

    /// POST /sso/introspect — RFC 7662 令牌内省（异构后端校验）
    void introspect(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        if (ssoUnavailable(cb)) return;
        std::string token = req->getParameter("token");
        if (token.empty()) {
            auto j = req->getJsonObject();
            if (j && (*j).isMember("token")) token = (*j)["token"].asString();
        }
        Json::Value j;
        if (token.empty()) { j["active"] = false; cb(drogon::HttpResponse::newHttpJsonResponse(j)); return; }
        try {
            auto uuid = JwtUtils::parseUuid(token);
            auto user = TokenCache::instance().get(uuid);
            if (user) {
                j["active"]   = true;
                j["sub"]      = std::to_string(user->userId);
                j["username"] = user->userName;
                j["token_type"] = "Bearer";
            } else {
                j["active"] = false;   // JWT 有效但会话已注销
            }
        } catch (...) {
            j["active"] = false;
        }
        cb(drogon::HttpResponse::newHttpJsonResponse(j));
    }

    /// POST /sso/revoke — 撤销刷新令牌
    void revoke(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        if (ssoUnavailable(cb)) return;
        std::string rt = req->getParameter("token");
        if (rt.empty()) {
            auto j = req->getJsonObject();
            if (j && (*j).isMember("token")) rt = (*j)["token"].asString();
        }
        if (!rt.empty()) SsoServer::instance().revokeRefresh(rt);
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);   // RFC 7009: 无论是否存在都 200
        cb(resp);
    }

    /// GET /.well-known/openid-configuration — OIDC 发现
    void discovery(const drogon::HttpRequestPtr&,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        if (ssoUnavailable(cb)) return;
        cb(drogon::HttpResponse::newHttpJsonResponse(
            SsoServer::instance().discoveryDoc()));
    }

    // ── 接入方管理（管理员）────────────────────────────────────────────

    /// GET /sso/clients — 接入方列表（secret 脱敏）
    void clients(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:sso:list");
        if (ssoUnavailable(cb)) return;
        Json::Value arr(Json::arrayValue);
        for (auto& c : SsoServer::instance().listClients()) {
            Json::Value j;
            j["client_id"] = c.clientId;
            j["name"]      = c.name;
            j["enabled"]   = c.enabled;
            j["public"]    = c.clientSecret.empty();
            for (auto& u : c.redirectUris) j["redirect_uris"].append(u);
            for (auto& s : c.scopes)       j["scopes"].append(s);
            arr.append(j);
        }
        RESP_OK(cb, arr);
    }

    /// POST /sso/clients — 注册接入方 {client_id, client_secret?, name, redirect_uris[], scopes[]}
    void addClient(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:sso:add");
        if (ssoUnavailable(cb)) return;
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("client_id")) {
            RESP_ERR(cb, "缺少 client_id"); return;
        }
        SsoServer::ClientInfo ci;
        ci.clientId     = (*body)["client_id"].asString();
        ci.clientSecret = body->get("client_secret", "").asString();
        ci.name         = body->get("name", ci.clientId).asString();
        for (auto& u : (*body)["redirect_uris"]) ci.redirectUris.push_back(u.asString());
        for (auto& s : (*body)["scopes"])        ci.scopes.push_back(s.asString());
        if (ci.clientId.empty()) { RESP_ERR(cb, "client_id 不能为空"); return; }
        SsoServer::instance().upsertClient(ci);
        LOG_OPER_PARAM(req, "SSO接入方", BusinessType::INSERT, ci.clientId);
        RESP_MSG(cb, "注册成功（运行时生效，重启后以 config.json 为准）");
    }

    /// DELETE /sso/clients/{id} — 移除接入方
    void delClient(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                   const std::string& id) {
        CHECK_PERM(req, cb, "system:sso:remove");
        if (ssoUnavailable(cb)) return;
        if (!SsoServer::instance().removeClient(id)) {
            RESP_ERR(cb, "接入方不存在: " + id); return;
        }
        LOG_OPER_PARAM(req, "SSO接入方", BusinessType::REMOVE, id);
        RESP_MSG(cb, "删除成功");
    }
};
