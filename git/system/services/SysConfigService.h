#pragma once
#include <json/json.h>
#include <string>
#include <optional>
#include "../../common/TokenCache.h"
#include "../../common/Constants.h"
#include "../../services/DatabaseService.h"

// 对应 RuoYi.Net SysConfigService，使用直接 libpq 查询
class SysConfigService {
public:
    static SysConfigService &instance() {
        static SysConfigService inst;
        return inst;
    }

    // 按 key 查询，先读缓存再查 DB
    std::string selectConfigByKey(const std::string &configKey) {
        auto cacheKey = Constants::SYS_CONFIG_KEY + configKey;
        auto cached = MemCache::instance().getString(cacheKey);
        if (cached) return *cached;

        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1",
            {configKey});
        if (res.ok() && res.rows() > 0) {
            std::string val = res.str(0, 0);
            MemCache::instance().setString(cacheKey, val);
            return val;
        }
        return "";
    }

    bool isCaptchaEnabled() {
        auto val = selectConfigByKey("sys.account.captchaEnabled");
        if (val.empty()) return true;
        return val == "true";
    }

    bool isRegisterEnabled() {
        return selectConfigByKey("sys.account.registerUser") == "true";
    }

    bool isForgotPwdEnabled() {
        auto val = selectConfigByKey("sys.account.forgotPwdEnabled");
        if (val.empty()) return true; // 默认开启
        return val == "true";
    }

    // 注册用户默认角色 ID，0 表示未配置
    long getInitRoleId() {
        auto val = selectConfigByKey("sys.account.initRoleId");
        if (val.empty()) return 0;
        try { return std::stol(val); } catch (...) { return 0; }
    }

    void resetConfigCache() {
        MemCache::instance().removeByPrefix(Constants::SYS_CONFIG_KEY);
        loadConfigCache();
    }

    void loadConfigCache() {
        auto& db = DatabaseService::instance();
        auto res = db.query("SELECT config_key, config_value FROM sys_config");
        if (res.ok()) {
            for (int i = 0; i < res.rows(); ++i) {
                auto k = res.str(i, 0);
                auto v = res.str(i, 1);
                MemCache::instance().setString(Constants::SYS_CONFIG_KEY + k, v);
            }
        }
    }

    bool checkConfigKeyUnique(const std::string &configKey, int configId = 0) {
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT config_id FROM sys_config WHERE config_key=$1 LIMIT 1",
            {configKey});
        if (!res.ok() || res.rows() == 0) return true;
        return res.intVal(0, 0) == configId;
    }
};
