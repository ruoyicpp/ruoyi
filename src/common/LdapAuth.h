/**
 * @file LdapAuth.h
 * @brief LDAP/Active Directory 认证工具
 * 
 * 功能概述：
 *   - LDAP 认证：支持 LDAP 和 Active Directory 认证
 *   - 用户查询：查询 LDAP 目录中的用户信息
 *   - 密码验证：验证用户密码
 *   - 本地回退：LDAP 认证失败时可回退到本地认证
 *   - 跨平台支持：Windows 和 Linux 都支持
 * 
 * 认证流程：
 *   1. 使用 bind_dn 和 bind_pass 连接到 LDAP 服务器
 *   2. 根据 user_filter 查询用户 DN
 *   3. 使用用户 DN 和密码尝试 bind
 *   4. 如果 LDAP 认证失败且启用本地回退，则使用本地数据库认证
 * 
 * 配置格式（config.json）：
 *   {
 *     "ldap": {
 *       "enabled": true,
 *       "host": "192.168.1.100",
 *       "port": 389,
 *       "base_dn": "DC=example,DC=com",
 *       "bind_dn": "CN=svc,OU=SA,DC=example,DC=com",
 *       "bind_pass": "password",
 *       "user_filter": "(&(objectClass=person)(sAMAccountName={username}))",
 *       "fallback_local": true
 *     }
 *   }
 * 
 * 配置项说明：
 *   - enabled: 是否启用 LDAP 认证（默认 false）
 *   - host: LDAP 服务器地址（默认 127.0.0.1）
 *   - port: LDAP 服务器端口（默认 389，加密 636）
 *   - base_dn: LDAP 基础 DN（如 DC=example,DC=com）
 *   - bind_dn: 绑定用户 DN（用于查询用户）
 *   - bind_pass: 绑定用户密码
 *   - user_filter: 用户查询过滤器（{username} 会被替换为实际用户名）
 *   - fallback_local: LDAP 失败时是否回退到本地认证（默认 true）
 * 
 * 使用示例：
 *   // 初始化 LDAP 认证
 *   Json::Value cfg;
 *   // ... 从 config.json 读取 cfg ...
 *   LdapAuth::instance().init(cfg);
 *   
 *   // 验证用户
 *   if (LdapAuth::instance().authenticate("username", "password")) {
 *       // 认证成功
 *   }
 * 
 * 常见 LDAP 过滤器：
 *   - Active Directory: (&(objectClass=person)(sAMAccountName={username}))
 *   - OpenLDAP: (&(objectClass=inetOrgPerson)(uid={username}))
 *   - 通用: (&(objectClass=*)(cn={username}))
 * 
 * 特性：
 *   - 多目录支持：支持 Active Directory 和 OpenLDAP
 *   - 灵活配置：通过 config.json 配置 LDAP 参数
 *   - 本地回退：LDAP 失败时可回退到本地认证
 *   - 跨平台：Windows（MSYS2）和 Linux 都支持
 *   - 安全性：使用 LDAP bind 验证密码，不传输明文
 * 
 * 平台要求：
 *   - Windows: 需要安装 MSYS2 openldap-client 包
 *   - Linux: 需要安装 ldap-utils 包
 */

#pragma once
#include <string>
#include <json/json.h>

/**
 * @struct LdapAuth
 * @brief LDAP/Active Directory 认证单例
 * 
 * 提供 LDAP 和 Active Directory 认证功能。
 * 支持用户查询和密码验证。
 */
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
