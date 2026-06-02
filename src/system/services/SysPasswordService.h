/**
 * @file SysPasswordService.h
 * @brief 系统密码服务 — 处理密码验证、错误次数限制、账号锁定等
 * 
 * 功能概述：
 *   - 密码验证：使用 bcrypt 验证用户密码
 *   - 错误限制：限制密码错误次数，防止暴力破解
 *   - 账号锁定：密码错误超过限制次数后锁定账号
 *   - 日志记录：记录密码验证失败的日志
 * 
 * 核心特性：
 *   - 密码哈希：使用 bcrypt 算法，不存储明文密码
 *   - 缓存管理：使用 MemCache 存储密码错误计数
 *   - 自动解锁：锁定时间到期后自动解锁（缓存过期）
 *   - 日志追踪：记录所有密码验证尝试
 * 
 * 配置项（config.json）：
 *   - user.max_retry_count: 最大重试次数（默认 5）
 *   - user.lock_time_minutes: 锁定时间（分钟，默认 15）
 */

#pragma once
#include <string>
#include <stdexcept>
#include <drogon/drogon.h>
#include "../../common/SecurityUtils.h"
#include "../../common/TokenCache.h"
#include "../../common/Constants.h"
#include "../../services/DatabaseService.h"
#include "../../common/IpUtils.h"

/**
 * @class SysPasswordService
 * @brief 系统密码服务单例
 * 
 * 对应 RuoYi.Net 中的 SysPasswordService，处理密码验证和安全防护。
 * 采用单例模式，全局唯一实例。
 * 
 * 支持密码错误次数限制、账号锁定、日志记录等功能。
 */
class SysPasswordService {
public:
    static SysPasswordService &instance() {
        static SysPasswordService inst;
        return inst;
    }

    /**
     * @brief 验证用户密码
     * 
     * 验证流程：
     *   1. 从缓存中获取该用户的密码错误次数
     *   2. 如果错误次数超过限制，抛出异常（账号已锁定）
     *   3. 使用 bcrypt 比对密码
     *   4. 如果密码错误，增加错误计数并设置缓存过期时间
     *   5. 如果密码正确，清除错误计数
     * 
     * @param username 用户名
     * @param rawPassword 明文密码（从前端获取）
     * @param encodedPassword bcrypt 哈希密码（从数据库获取）
     * @param ipAddr IP 地址（可选，用于日志记录）
     * @param ua User-Agent（可选，用于日志记录）
     * 
     * @throw std::runtime_error 密码验证失败或账号已锁定时抛出异常
     * 
     * @note 
     *   - 错误计数使用 MemCache 存储，键为 Constants::PWD_ERR_CNT_KEY + username
     *   - 缓存过期时间为 lock_time_minutes * 60 秒
     *   - 密码正确时自动清除错误计数
     *   - 每次密码错误都会记录到登录日志
     */
    void validate(const std::string &username, const std::string &rawPassword,
                  const std::string &encodedPassword,
                  const std::string &ipAddr = "", const std::string &ua = "") {
        auto &app = drogon::app();
        auto userCfg = app.getCustomConfig()["user"];
        int maxRetry = userCfg.get("max_retry_count", 5).asInt();
        int lockTime = userCfg.get("lock_time_minutes", 15).asInt();

        auto cacheKey = Constants::PWD_ERR_CNT_KEY + username;
        int retryCount = 0;
        auto cached = MemCache::instance().getString(cacheKey);
        if (cached && !cached->empty()) {
            try { retryCount = std::stoi(*cached); } catch (...) { retryCount = 0; }
        }

        if (retryCount >= maxRetry) {
            std::string msg = "密码输入错误" + std::to_string(maxRetry) +
                              "次，帐户锁定" + std::to_string(lockTime) + "分钟";
            writeLog(username, "1", msg, ipAddr, ua);
            throw std::runtime_error(msg);
        }

        if (!SecurityUtils::matchesPassword(rawPassword, encodedPassword)) {
            retryCount++;
            MemCache::instance().setString(cacheKey, std::to_string(retryCount),
                                           lockTime * 60);
        // 对应  同名日志：已尝试N次 + 密码不匹配
            writeLog(username, "1",
                     "密码输入错误" + std::to_string(retryCount) + "次",
                     ipAddr, ua);
            throw std::runtime_error("用户不存在/密码错误");
        }

        // 验证成功，清除错误计数
        MemCache::instance().remove(cacheKey);
    }

    void clearLoginRecordCache(const std::string &username) {
        MemCache::instance().remove(Constants::PWD_ERR_CNT_KEY + username);
    }

private:
    void writeLog(const std::string &username, const std::string &status,
                  const std::string &msg, const std::string &ip, const std::string &ua) {
        std::string location = IpUtils::getIpLocation(ip);
        auto& db = DatabaseService::instance();
        auto insRes = db.queryParams(
            "INSERT INTO sys_logininfor(user_name,ipaddr,login_location,browser,os,status,msg,login_time) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,NOW()) RETURNING info_id",
            {username, ip, location, ua, "", status, msg});
        if (!IpUtils::isIntranetIp(ip) && insRes.ok() && insRes.rows() > 0) {
            std::string infoId = insRes.str(0, 0);
            IpUtils::getIpLocationAsync(ip, [infoId](std::string loc) {
                DatabaseService::instance().execParams(
                    "UPDATE sys_logininfor SET login_location=$1 WHERE info_id=$2",
                    {loc, infoId});
            });
        }
    }
};
