/**
 * @file SysConfigService.h
 * @brief 系统配置服务 — 管理系统级配置参数
 * 
 * 功能概述：
 *   - 配置查询：按 key 查询系统配置值
 *   - 配置缓存：使用内存缓存加速配置读取
 *   - 功能开关：验证码、注册、忘记密码等功能的开关
 * 
 * 核心特性：
 *   - 双层存储：内存缓存（MemCache）+ 数据库持久化（sys_config）
 *   - 懒加载：首次查询时从数据库加载，后续从缓存读取
 *   - 配置键：统一使用 Constants::SYS_CONFIG_KEY 作为缓存键前缀
 * 
 * 常用配置项：
 *   - sys.account.captchaEnabled: 验证码是否启用（默认 true）
 *   - sys.account.registerUser: 用户注册是否启用（默认 false）
 *   - sys.account.forgotPwdEnabled: 忘记密码是否启用（默认 true）
 * 
 * 数据库表：
 *   - sys_config: 系统配置表（config_key, config_value）
 */

#pragma once
#include <json/json.h>
#include <string>
#include <optional>
#include "../../common/TokenCache.h"
#include "../../common/Constants.h"
#include "../../services/DatabaseService.h"

/**
 * @class SysConfigService
 * @brief 系统配置服务单例
 * 
 * 对应 RuoYi 中的 SysConfigService，管理系统级配置参数。
 * 采用单例模式，全局唯一实例。
 * 
 * 使用 libpq 直接查询 PostgreSQL 数据库，支持配置缓存和功能开关。
 */
class SysConfigService {
public:
    static SysConfigService &instance() {
        static SysConfigService inst;
        return inst;
    }

    /**
     * @brief 按 key 查询系统配置值
     * 
     * 先从内存缓存中查询，如果缓存未命中则从数据库查询，
     * 并将结果存入缓存以加速后续查询。
     * 
     * @param configKey 配置键（如 "sys.account.captchaEnabled"）
     * 
     * @return 配置值（字符串），如果不存在返回空字符串
     * 
     * @note 
     *   - 缓存键为 Constants::SYS_CONFIG_KEY + configKey
     *   - 缓存使用 MemCache 单例管理
     *   - 如果需要实时更新配置，需要手动清除缓存
     */
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
