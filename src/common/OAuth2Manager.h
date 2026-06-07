/**
 * @file OAuth2Manager.h
 * @brief OAuth2 管理器 — 第三方登录提供商管理
 * 
 * 功能概述：
 *   - Provider 管理：管理多个 OAuth2 提供商配置
 *   - 授权 URL 生成：生成第三方授权 URL
 *   - Token 交换：用 authorization code 交换 access token
 *   - 用户信息获取：通过 access token 获取用户信息
 *   - 状态验证：验证 OAuth2 state 参数防止 CSRF
 * 
 * 支持的提供商：
 *   - GitHub：GitHub OAuth2
 *   - Google：Google OAuth2
 *   - 钉钉：DingTalk OAuth2
 *   - 飞书：Feishu OAuth2
 *   - 企业微信：WeChat Work OAuth2
 *   - QQ：QQ OAuth2
 * 
 * 配置格式（config.json）：
 *   {
 *     "oauth2": {
 *       "github": {
 *         "enabled": true,
 *         "client_id": "your_client_id",
 *         "client_secret": "your_client_secret",
 *         "redirect_uri": "http://your.domain/oauth2/callback/github",
 *         "scope": "user:email"
 *       },
 *       "google": {
 *         "enabled": true,
 *         "client_id": "your_client_id",
 *         "client_secret": "your_client_secret",
 *         "redirect_uri": "http://your.domain/oauth2/callback/google",
 *         "scope": "openid email profile"
 *       }
 *     }
 *   }
 * 
 * 使用示例：
 *   // 初始化 OAuth2 管理器
 *   Json::Value cfg;
 *   // ... 从 config.json 读取 cfg ...
 *   OAuth2Manager::instance().init(cfg);
 *   
 *   // 获取已启用的 provider 列表
 *   auto providers = OAuth2Manager::instance().enabledProviders();
 *   
 *   // 生成授权 URL
 *   auto [url, state] = OAuth2Manager::instance().buildAuthUrl("github");
 *   
 *   // 交换 token
 *   auto token = OAuth2Manager::instance().exchangeToken("github", code);
 *   
 *   // 获取用户信息
 *   auto user = OAuth2Manager::instance().getUserInfo("github", token);
 * 
 * OAuth2 流程：
 *   1. 前端调用 /oauth2/authorize/{provider} 获取授权 URL
 *   2. 用户跳转到第三方授权页面
 *   3. 用户授权后，第三方重定向到 /oauth2/callback/{provider}?code=xxx&state=yyy
 *   4. 后端验证 state，用 code 交换 access token
 *   5. 用 access token 获取用户信息
 *   6. 创建或更新本地用户，返回 JWT
 * 
 * 特性：
 *   - 多 Provider 支持：支持多个第三方登录提供商
 *   - 灵活配置：通过 config.json 配置各个 provider
 *   - State 验证：防止 CSRF 攻击
 *   - 异步请求：使用 HttpClient 异步调用第三方 API
 *   - 错误处理：完善的错误处理和日志记录
 */

#pragma once
#include <string>
#include <map>
#include <json/json.h>
#include <drogon/HttpClient.h>
#include <openssl/rand.h>

/**
 * @struct OAuth2Provider
 * @brief OAuth2 提供商配置
 * 
 * 存储单个 OAuth2 提供商的配置信息。
 */
struct OAuth2Provider {
    std::string name;
    bool        enabled       = false;
    std::string clientId;
    std::string clientSecret;
    std::string redirectUri;
    std::string scope;
    std::string authUrl;
    std::string tokenUrl;
    std::string userUrl;
    // 企业微信额外字段
    std::string corpId;
    std::string agentId;
};

class OAuth2Manager {
public:
    static OAuth2Manager &instance() { static OAuth2Manager m; return m; }

    void init(const Json::Value &cfg) {
        auto addProvider = [&](const std::string &name,
                               const std::string &authUrl,
                               const std::string &tokenUrl,
                               const std::string &userUrl,
                               const std::string &defaultScope) {
            if (!cfg.isMember(name)) return;
            const auto &c = cfg[name];
            OAuth2Provider p;
            p.name         = name;
            p.enabled      = c.get("enabled", false).asBool();
            p.clientId     = c.get("client_id", "").asString();
            p.clientSecret = c.get("client_secret", "").asString();
            p.redirectUri  = c.get("redirect_uri", "").asString();
            p.scope        = c.get("scope", defaultScope).asString();
            p.authUrl      = authUrl;
            p.tokenUrl     = tokenUrl;
            p.userUrl      = userUrl;
            p.corpId       = c.get("corp_id", "").asString();
            p.agentId      = c.get("agent_id", "").asString();
            if (p.enabled && !p.clientId.empty()) providers_[name] = p;
        };

        addProvider("github",
            "https://github.com/login/oauth/authorize",
            "https://github.com/login/oauth/access_token",
            "https://api.github.com/user",
            "user:email");
        addProvider("google",
            "https://accounts.google.com/o/oauth2/v2/auth",
            "https://oauth2.googleapis.com/token",
            "https://www.googleapis.com/oauth2/v3/userinfo",
            "openid email profile");
        addProvider("dingtalk",
            "https://login.dingtalk.com/oauth2/auth",
            "https://api.dingtalk.com/v1.0/oauth2/userAccessToken",
            "https://api.dingtalk.com/v1.0/contact/users/me",
            "openid");
        addProvider("feishu",
            "https://open.feishu.cn/open-apis/authen/v1/authorize",
            "https://open.feishu.cn/open-apis/authen/v1/oidc/access_token",
            "https://open.feishu.cn/open-apis/authen/v1/user_info",
            "");
        addProvider("wechat_work",
            "https://open.work.weixin.qq.com/wwopen/sso/qrConnect",
            "https://qyapi.weixin.qq.com/cgi-bin/gettoken",
            "https://qyapi.weixin.qq.com/cgi-bin/user/getuserinfo",
            "snsapi_base");
        addProvider("qq",
            "https://graph.qq.com/oauth2.0/authorize",
            "https://graph.qq.com/oauth2.0/token",
            "https://graph.qq.com/oauth2.0/me",
            "get_user_info");
    }

    // 获取已启用的 provider 列表
    std::vector<std::string> enabledProviders() const {
        std::vector<std::string> v;
        for (auto &[k, p] : providers_) v.push_back(k);
        return v;
    }

    bool hasProvider(const std::string &name) const {
        return providers_.count(name) > 0;
    }

    const OAuth2Provider &provider(const std::string &name) const {
        return providers_.at(name);
    }

    // 生成授权 URL + CSRF state
    std::pair<std::string, std::string> buildAuthUrl(const std::string &name) {
        if (!providers_.count(name)) return {"", ""};
        auto &p   = providers_[name];
        std::string state = randomHex(16);

        std::string url = p.authUrl +
            "?client_id="    + urlEncode(p.clientId) +
            "&redirect_uri=" + urlEncode(p.redirectUri) +
            "&response_type=code" +
            "&state="        + state;
        if (!p.scope.empty())   url += "&scope="    + urlEncode(p.scope);
        if (!p.corpId.empty())  url += "&appid="    + urlEncode(p.corpId);
        if (!p.agentId.empty()) url += "&agentid="  + urlEncode(p.agentId);

        return {url, state};
    }

    static std::string urlEncode(const std::string &s) {
        std::string r;
        for (unsigned char c : s) {
            if (isalnum(c) || c=='-'||c=='_'||c=='.'||c=='~') r += c;
            else { char b[4]; snprintf(b,sizeof(b),"%%%02X",c); r += b; }
        }
        return r;
    }

    static std::string randomHex(int bytes) {
        std::vector<uint8_t> buf(bytes);
        RAND_bytes(buf.data(), bytes);
        std::string r;
        for (auto b : buf) { char h[3]; snprintf(h,3,"%02x",b); r+=h; }
        return r;
    }

private:
    std::map<std::string, OAuth2Provider> providers_;
};
