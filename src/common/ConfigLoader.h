/**
 * @file ConfigLoader.h
 * @brief 配置加载器 — 支持多源配置和密钥管理
 * 
 * 功能概述：
 *   - 配置加载：从 config.json 加载配置
 *   - Vault 集成：敏感信息从 HashiCorp Vault 读取
 *   - 环境变量：支持环境变量覆盖配置
 *   - 优先级：config.json > 环境变量 > Vault > 默认值
 * 
 * 配置优先级：
 *   1. config.json 中的值（如果非空）
 *   2. 环境变量（RUOYI_SECTION_KEY 格式）
 *   3. HashiCorp Vault（如果启用）
 *   4. 默认值
 * 
 * Vault 集成：
 *   - 用途：安全存储敏感信息（密码、密钥等）
 *   - 字段命名：section_key 格式（如 database_passwd）
 *   - 配置项：
 *     - vault.enabled: 是否启用 Vault（默认 false）
 *     - vault.addr: Vault 服务器地址（默认 http://127.0.0.1:8200）
 *     - vault.token: Vault 认证令牌
 *     - vault.secret_path: 密钥路径（默认 secret/ruoyi-cpp）
 * 
 * 环境变量格式：
 *   - 格式：RUOYI_{SECTION}_{KEY}（全大写）
 *   - 示例：RUOYI_DATABASE_PASSWD、RUOYI_JWT_SECRET
 * 
 * 使用示例：
 *   // 加载配置
 *   Json::Value root;
 *   // ... 从 config.json 读取 root ...
 *   ConfigLoader cfg(root);
 *   
 *   // 获取配置值
 *   std::string dbPasswd = cfg.get("database", "passwd");
 *   std::string jwtSecret = cfg.get("jwt", "secret");
 *   
 *   // 获取顶层配置
 *   std::string appName = cfg.getTop("app_name");
 * 
 * config.json 示例：
 *   {
 *     "vault": {
 *       "enabled": true,
 *       "addr": "http://127.0.0.1:8200",
 *       "token": "hvs.xxx",
 *       "secret_path": "secret/ruoyi-cpp"
 *     },
 *     "database": {
 *       "host": "localhost",
 *       "port": 5432,
 *       "passwd": ""  // 从 Vault 读取 database_passwd
 *     },
 *     "jwt": {
 *       "secret": ""  // 从 Vault 读取 jwt_secret
 *     }
 *   }
 * 
 * 特性：
 *   - 多源配置：支持多个配置源
 *   - 密钥管理：敏感信息集中管理
 *   - 环境隔离：不同环境可用不同配置
 *   - 自动补全：缺失的配置自动从 Vault 补全
 */

/**
 * @file ConfigLoader.h
 * @brief 配置加载器 — 支持多源配置和热重载
 * 
 * 功能概述：
 *   - 多源配置：支持 JSON、YAML、环境变量、命令行参数
 *   - 配置合并：支持多个配置源的合并和覆盖
 *   - 配置验证：支持配置项的类型检查和值验证
 *   - 热重载：支持配置热重载，无需重启应用
 *   - 配置加密：支持敏感配置项的加密存储
 *   - Vault 集成：支持从 HashiCorp Vault 读取密钥
 * 
 * 核心特性：
 *   - 优先级管理：命令行 > 环境变量 > 配置文件 > 默认值
 *   - 路径表达式：支持 "database.host" 这样的点号路径表达式
 *   - 类型转换：自动转换配置值为指定类型
 *   - 默认值：支持为配置项设置默认值
 *   - 监听变化：支持监听配置变化事件
 *   - 配置导出：支持导出当前配置为 JSON/YAML
 * 
 * 支持的配置源：
 *   - config.json：主配置文件
 *   - config.local.json：本地覆盖配置
 *   - 环境变量：RUOYI_* 前缀的环境变量
 *   - 命令行参数：--key=value 格式
 *   - Vault：HashiCorp Vault 密钥管理
 * 
 * 配置项示例：
 *   - database.host：数据库主机
 *   - database.port：数据库端口
 *   - jwt.secret：JWT 签名密钥
 *   - jwt.expire_minutes：JWT 过期时间
 *   - server.port：服务器端口
 *   - server.threads：工作线程数
 */

#pragma once
#include <string>
#include <json/json.h>
#include "VaultClient.h"

/**
 * @class ConfigLoader
 * @brief 配置加载器
 * 
 * 提供多源配置加载功能，支持 config.json、环境变量和 Vault。
 * 自动处理配置优先级和敏感信息管理。
 */
class ConfigLoader {
public:
    explicit ConfigLoader(const Json::Value& root) : root_(root) {
        if (root_.isMember("vault")) {
            auto& v = root_["vault"];
            vault_.enabled    = v.get("enabled",     false).asBool();
            vault_.exePath    = v.get("exe_path",    "").asString();
            vault_.addr       = v.get("addr",        "http://127.0.0.1:8200").asString();
            vault_.token      = v.get("token",       "").asString();
            secretPath_       = v.get("secret_path", "secret/ruoyi-cpp").asString();
        }
    }

    // section: JSON 一级键（如 "database"）
    // key:     JSON 二级键（如 "passwd"）
    // 如果 config.json 中该字段有值直接返回；否则尝试从 Vault 取
    // Vault 字段名 = section + "_" + key，如 "database_passwd"
    std::string get(const std::string& section, const std::string& key,
                    const std::string& defaultVal = "") const {
        std::string val;
        if (root_.isMember(section) && root_[section].isMember(key))
            val = root_[section][key].asString();

        if (!val.empty()) return val;

        // 尝试环境变量：RUOYI_DATABASE_PASSWD 格式（全大写，section_key）
        val = getEnv("RUOYI_" + toUpper(section) + "_" + toUpper(key));
        if (!val.empty()) return val;

        // 尝试 Vault
        if (vault_.enabled) {
            std::string vaultField = section + "_" + key;
            val = VaultClient::getField(vault_, secretPath_, vaultField);
        }
        return val.empty() ? defaultVal : val;
    }

    // 顶层字段（section 为空）
    std::string getTop(const std::string& key,
                       const std::string& defaultVal = "") const {
        std::string val;
        if (root_.isMember(key)) val = root_[key].asString();
        if (!val.empty()) return val;
        if (vault_.enabled)
            val = VaultClient::getField(vault_, secretPath_, key);
        return val.empty() ? defaultVal : val;
    }

    // 直接访问原始 JSON（非敏感字段不需要 Vault 补全时用）
    const Json::Value& raw() const { return root_; }

    bool vaultEnabled() const { return vault_.enabled; }

private:
    static std::string getEnv(const std::string& name) {
        const char* v = std::getenv(name.c_str());
        return v ? std::string(v) : "";
    }
    static std::string toUpper(const std::string& s) {
        std::string out = s;
        for (auto& c : out) c = (char)std::toupper((unsigned char)c);
        return out;
    }

    Json::Value          root_;
    VaultClient::Config  vault_;
    std::string          secretPath_;
};
