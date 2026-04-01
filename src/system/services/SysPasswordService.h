#pragma once
#include <string>
#include <stdexcept>
#include <drogon/drogon.h>
#include "../../common/SecurityUtils.h"
#include "../../common/TokenCache.h"
#include "../../common/Constants.h"
#include "../../services/DatabaseService.h"
#include "../../common/IpUtils.h"

// 对应 RuoYi.Net SysPasswordService
class SysPasswordService {
public:
    static SysPasswordService &instance() {
        static SysPasswordService inst;
        return inst;
    }

    // 验证密码，失败则抛异常并记录错误次数
    // ipAddr / ua 用于写登录日志，，暂时写死传空
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
        // 对应 C# 同名日志：已尝试N次 + 密码不匹配
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
