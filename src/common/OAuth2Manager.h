#pragma once
#include <string>
#include <map>
#include <json/json.h>
#include <drogon/HttpClient.h>
#include <openssl/rand.h>

// OAuth2 第三方登录管理器
// 支持：github / google / wechat_work / dingtalk / feishu / qq
// config.json: { "oauth2": { "github": { "enabled": true, "client_id": "", "client_secret": "",
//   "redirect_uri": "http://your.domain/oauth2/callback/github", "scope": "user:email" }, ... } }
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
