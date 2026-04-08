#pragma once
#include <string>
#include <json/json.h>

// LDAP/Active Directory 认证
// config.json: { "ldap": { "enabled": false, "host": "192.168.1.100", "port": 389,
//   "base_dn": "DC=example,DC=com", "bind_dn": "CN=svc,OU=SA,DC=example,DC=com",
//   "bind_pass": "pass", "user_filter": "(&(objectClass=person)(sAMAccountName={username}))",
//   "fallback_local": true } }
struct LdapAuth {
    struct Config {
        bool        enabled       = false;
        std::string host          = "127.0.0.1";
        int         port          = 389;
        std::string baseDn;
        std::string bindDn;
        std::string bindPass;
        std::string userFilter    = "(&(objectClass=person)(sAMAccountName={username}))";
        bool        fallbackLocal = true;
    };

    static LdapAuth &instance() { static LdapAuth l; return l; }

    void init(const Json::Value &cfg) {
        cfg_.enabled       = cfg.get("enabled", false).asBool();
        cfg_.host          = cfg.get("host", "127.0.0.1").asString();
        cfg_.port          = cfg.get("port", 389).asInt();
        cfg_.baseDn        = cfg.get("base_dn", "").asString();
        cfg_.bindDn        = cfg.get("bind_dn", "").asString();
        cfg_.bindPass      = cfg.get("bind_pass", "").asString();
        cfg_.userFilter    = cfg.get("user_filter", cfg_.userFilter).asString();
        cfg_.fallbackLocal = cfg.get("fallback_local", true).asBool();
    }

    bool enabled()       const { return cfg_.enabled; }
    bool fallbackLocal() const { return cfg_.fallbackLocal; }

    // 尝试 LDAP 认证，成功返回 true
    // Windows 平台通过调用 ldapsearch CLI 实现（需要 MSYS2 openldap-client 包）
    // Linux 平台直接调用 ldapsearch
    bool authenticate(const std::string &username, const std::string &password) {
        if (!cfg_.enabled) return false;
        if (username.empty() || password.empty()) return false;

        // 构造 ldapsearch 命令验证用户绑定
        std::string filter = cfg_.userFilter;
        size_t pos = filter.find("{username}");
        if (pos != std::string::npos) filter.replace(pos, 10, escapeFilter(username));

        // 先以 bind_dn 查找用户 DN
        std::string userDn = findUserDn(username, filter);
        if (userDn.empty()) return false;

        // 再以用户 DN + 密码尝试 bind
        return tryBind(userDn, password);
    }

private:
    Config cfg_;

    static std::string escapeFilter(const std::string &s) {
        std::string r;
        for (char c : s) {
            switch (c) {
                case '\\': r += "\\5c"; break;
                case '*':  r += "\\2a"; break;
                case '(':  r += "\\28"; break;
                case ')':  r += "\\29"; break;
                case '\0': r += "\\00"; break;
                default:   r += c;
            }
        }
        return r;
    }

    std::string findUserDn(const std::string &/*user*/, const std::string &filter) {
        // ldapsearch -x -H ldap://host:port -D bind_dn -w bind_pass -b base_dn filter dn
        std::string cmd = "ldapsearch -x -LLL"
            " -H ldap://" + cfg_.host + ":" + std::to_string(cfg_.port) +
            " -D \"" + cfg_.bindDn + "\""
            " -w \"" + cfg_.bindPass + "\""
            " -b \"" + cfg_.baseDn + "\""
            " \"" + filter + "\" dn 2>/dev/null";
        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp) return "";
        char buf[1024]; std::string out;
        while (fgets(buf, sizeof(buf), fp)) out += buf;
        pclose(fp);
        // 解析 dn: xxx
        auto pos = out.find("dn: ");
        if (pos == std::string::npos) return "";
        auto end = out.find('\n', pos);
        return out.substr(pos + 4, end == std::string::npos ? std::string::npos : end - pos - 4);
    }

    bool tryBind(const std::string &userDn, const std::string &password) {
        std::string cmd = "ldapsearch -x -LLL"
            " -H ldap://" + cfg_.host + ":" + std::to_string(cfg_.port) +
            " -D \"" + userDn + "\""
            " -w \"" + password + "\""
            " -b \"" + userDn + "\" dn 2>/dev/null";
        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp) return false;
        char buf[256]; std::string out;
        while (fgets(buf, sizeof(buf), fp)) out += buf;
        int ret = pclose(fp);
        return ret == 0 && out.find("dn:") != std::string::npos;
    }
};
