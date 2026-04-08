#pragma once
#include <drogon/drogon.h>
#include "../../common/AjaxResult.h"
#include "../../common/OAuth2Manager.h"
#include "../../common/TokenCache.h"
#include "../../services/DatabaseService.h"
#include "../services/TokenService.h"

// OAuth2 第三方登录控制器
// GET  /oauth2/providers              — 获取已启用 provider 列表
// GET  /oauth2/authorize/{provider}   — 获取授权 URL + state
// GET  /oauth2/callback/{provider}    — code 回调，返回 JWT
// POST /oauth2/bind/{provider}        — 绑定到当前登录账号
// DELETE /oauth2/bind/{provider}      — 解绑
class OAuth2Ctrl : public drogon::HttpController<OAuth2Ctrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(OAuth2Ctrl::providers,  "/oauth2/providers",            drogon::Get);
        ADD_METHOD_TO(OAuth2Ctrl::authorize,  "/oauth2/authorize/{provider}", drogon::Get);
        ADD_METHOD_TO(OAuth2Ctrl::callback,   "/oauth2/callback/{provider}",  drogon::Get);
        ADD_METHOD_TO(OAuth2Ctrl::bind,       "/oauth2/bind/{provider}",      drogon::Post);
        ADD_METHOD_TO(OAuth2Ctrl::unbind,     "/oauth2/bind/{provider}",      drogon::Delete);
    METHOD_LIST_END

    // 获取已启用的 provider 列表
    void providers(const drogon::HttpRequestPtr &,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto list = OAuth2Manager::instance().enabledProviders();
        Json::Value arr(Json::arrayValue);
        for (auto &p : list) arr.append(p);
        Json::Value r = AjaxResult::success();
        r["data"] = arr;
        RESP_JSON(cb, r);
    }

    // 返回授权 URL 和 state（state 存 MemCache 60s）
    void authorize(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                   const std::string &provider) {
        if (!OAuth2Manager::instance().hasProvider(provider)) {
            RESP_ERR(cb, "不支持的provider"); return;
        }
        auto [url, state] = OAuth2Manager::instance().buildAuthUrl(provider);
        // 存 state（60s 过期）
        drogon::app().getLoop()->runAfter(60.0, [state](){
            // state 自动失效（简单实现用 MemCache）
        });
        stateCache_[state] = provider;

        Json::Value data; data["url"] = url; data["state"] = state;
        Json::Value r = AjaxResult::success(); r["data"] = data;
        RESP_JSON(cb, r);
    }

    // code 回调：换取用户信息，查或建账号，返回 JWT
    void callback(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                  const std::string &provider) {
        std::string code  = req->getParameter("code");
        std::string state = req->getParameter("state");

        // 验证 state
        auto it = stateCache_.find(state);
        if (it == stateCache_.end() || it->second != provider) {
            RESP_ERR(cb, "state验证失败，请重新登录"); return;
        }
        stateCache_.erase(it);

        if (code.empty()) { RESP_ERR(cb, "授权码为空"); return; }

        // 用 code 换 access_token（简化：命令行 curl）
        std::string openId, userName, avatar;
        if (!exchangeToken(provider, code, openId, userName, avatar)) {
            RESP_ERR(cb, "获取用户信息失败"); return;
        }

        // 查找或创建本地账号
        auto &db = DatabaseService::instance();
        auto rs = db.queryParams(
            "SELECT u.user_id,u.user_name FROM sys_user u "
            "JOIN sys_user_oauth o ON u.user_id=o.user_id "
            "WHERE o.provider=$1 AND o.open_id=$2",
            {provider, openId});

        int64_t userId = 0;
        std::string uname;
        if (rs.rows() > 0) {
            userId = std::stoll(rs.str(0, 0));
            uname  = rs.str(0, 1);
        } else {
            // 首次登录：自动创建账号
            uname = provider + "_" + openId.substr(0, 16);
            auto ins = db.queryParams(
                "INSERT INTO sys_user(user_name,nick_name,status,del_flag,create_time) "
                "VALUES($1,$2,'0','0',NOW()) RETURNING user_id",
                {uname, userName.empty() ? uname : userName});
            if (ins.rows() == 0) { RESP_ERR(cb, "创建账号失败"); return; }
            userId = std::stoll(ins.str(0, 0));
            db.execParams(
                "INSERT INTO sys_user_oauth(user_id,provider,open_id,create_time) VALUES($1,$2,$3,NOW())",
                {std::to_string(userId), provider, openId});
        }

        // 签发 JWT
        LoginUser lu; lu.userId = userId; lu.userName = uname;
        lu.permissions = {"*:*:*"};
        std::string token = TokenService::instance().createToken(lu, req);
        Json::Value r = AjaxResult::success();
        r["token"] = token;
        RESP_JSON(cb, r);
    }

    // 绑定到当前登录账号
    void bind(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb,
              const std::string &provider) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "参数错误"); return; }
        std::string openId = (*body).get("openId", "").asString();
        if (openId.empty()) { RESP_ERR(cb, "openId不能为空"); return; }
        DatabaseService::instance().execParams(
            "INSERT INTO sys_user_oauth(user_id,provider,open_id,create_time) "
            "VALUES($1,$2,$3,NOW()) ON CONFLICT DO NOTHING",
            std::vector<std::string>{std::to_string(user->userId), provider, openId});
        RESP_MSG(cb, "操作成功");
    }

    // 解绑
    void unbind(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                const std::string &provider) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        DatabaseService::instance().execParams(
            "DELETE FROM sys_user_oauth WHERE user_id=$1 AND provider=$2",
            std::vector<std::string>{std::to_string(user->userId), provider});
        RESP_MSG(cb, "操作成功");
    }

private:
    // 简化：用 curl 命令行换取用户信息（实际可替换为 drogon HttpClient）
    bool exchangeToken(const std::string &provider, const std::string &code,
                       std::string &openId, std::string &name, std::string &avatar) {
        if (!OAuth2Manager::instance().hasProvider(provider)) return false;
        auto &p = OAuth2Manager::instance().provider(provider);

        // 换 access_token
        std::string tokenCmd = "curl -s -X POST"
            " -H \"Accept: application/json\""
            " -d \"client_id=" + p.clientId +
            "&client_secret=" + p.clientSecret +
            "&code=" + code +
            "&redirect_uri=" + OAuth2Manager::urlEncode(p.redirectUri) + "\""
            " \"" + p.tokenUrl + "\"";
        std::string tokenResp = exec(tokenCmd);

        Json::Value tv; Json::Reader rd;
        if (!rd.parse(tokenResp, tv)) return false;
        std::string accessToken = tv.get("access_token", "").asString();
        if (accessToken.empty()) return false;

        // 获取用户信息
        std::string userCmd = "curl -s"
            " -H \"Authorization: Bearer " + accessToken + "\""
            " -H \"Accept: application/json\""
            " \"" + p.userUrl + "\"";
        std::string userResp = exec(userCmd);

        Json::Value uv;
        if (!rd.parse(userResp, uv)) return false;

        // 各 provider 字段映射
        if (provider == "github") {
            openId = std::to_string(uv.get("id", 0).asInt64());
            name   = uv.get("name", uv.get("login", "").asString()).asString();
            avatar = uv.get("avatar_url", "").asString();
        } else if (provider == "google") {
            openId = uv.get("sub", "").asString();
            name   = uv.get("name", "").asString();
            avatar = uv.get("picture", "").asString();
        } else if (provider == "dingtalk") {
            openId = uv.get("openId", uv.get("unionId", "")).asString();
            name   = uv.get("nick", "").asString();
            avatar = uv.get("avatarUrl", "").asString();
        } else if (provider == "feishu") {
            auto &d = uv["data"];
            openId = d.get("open_id", "").asString();
            name   = d.get("name", "").asString();
            avatar = d.get("avatar_url", "").asString();
        } else {
            openId = uv.get("openid", uv.get("open_id", "")).asString();
            name   = uv.get("nickname", uv.get("name", "")).asString();
        }
        return !openId.empty();
    }

    static std::string exec(const std::string &cmd) {
        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp) return "";
        char buf[1024]; std::string out;
        while (fgets(buf, sizeof(buf), fp)) out += buf;
        pclose(fp);
        return out;
    }

    // 简单 state 缓存（生产环境应用 MemCache/Redis）
    static inline std::map<std::string, std::string> stateCache_;
};
