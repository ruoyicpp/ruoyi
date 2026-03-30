#pragma once
#include <string>
#include <stdexcept>
#include <drogon/drogon.h>
#include "../../common/SecurityUtils.h"
#include "../../common/TokenCache.h"
#include "../../common/Constants.h"
#include "../../services/DatabaseService.h"

// ��Ӧ RuoYi.Net SysPasswordService
class SysPasswordService {
public:
    static SysPasswordService &instance() {
        static SysPasswordService inst;
        return inst;
    }

    // ��֤���룬ʧ�ܴ�������������
    // ipAddr / ua ����д����ĵ�¼��־���� C# ����һ�£�����ʱд������
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
            std::string msg = "�����������" + std::to_string(maxRetry) +
                              "�Σ��ʻ�����" + std::to_string(lockTime) + "����";
            writeLog(username, "1", msg, ipAddr, ua);
            throw std::runtime_error(msg);
        }

        if (!SecurityUtils::matchesPassword(rawPassword, encodedPassword)) {
            retryCount++;
            MemCache::instance().setString(cacheKey, std::to_string(retryCount),
                                           lockTime * 60);
            // ��Ӧ C# ��������־���Ѵ���N�� + ���벻ƥ��
            writeLog(username, "1",
                     "�����������" + std::to_string(retryCount) + "��",
                     ipAddr, ua);
            throw std::runtime_error("�û�������/�������");
        }

        // ��֤�ɹ�������������
        MemCache::instance().remove(cacheKey);
    }

    void clearLoginRecordCache(const std::string &username) {
        MemCache::instance().remove(Constants::PWD_ERR_CNT_KEY + username);
    }

private:
    void writeLog(const std::string &username, const std::string &status,
                  const std::string &msg, const std::string &ip, const std::string &ua) {
        DatabaseService::instance().execParams(
            "INSERT INTO sys_logininfor(user_name,ipaddr,login_location,browser,os,status,msg,login_time) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,NOW())",
            {username, ip, "", ua, "", status, msg});
    }
};
