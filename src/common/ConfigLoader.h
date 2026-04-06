#pragma once
#include <string>
#include <json/json.h>
#include "VaultClient.h"

// 配置加载器：config.json 优先，字段为空时从 Vault 自动补全
// 约定：Vault 字段命名为 "section_key"，如 database_passwd、jwt_secret
//
// config.json 示例（留空则由 Vault 补全）：
//   "vault": { "enabled":true, "addr":"http://127.0.0.1:8200",
//              "token":"hvs.xxx", "secret_path":"secret/ruoyi-cpp" }
//   "database": { "passwd": "" }   ← Vault 字段名: database_passwd
//   "jwt":      { "secret": "" }   ← Vault 字段名: jwt_secret
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
    Json::Value          root_;
    VaultClient::Config  vault_;
    std::string          secretPath_;
};
