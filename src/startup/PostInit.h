#pragma once
#include "AppIncludes.h"

inline void runPostInit() {
    // ── 启动时从 sys_token 恢复在线会话（Token 持久化）────────────────────
    {
        auto& db = DatabaseService::instance();
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        db.execParams("DELETE FROM sys_token WHERE expire_time<$1",
                      {std::to_string(nowMs)});
        auto res = db.query("SELECT token_key,token_value FROM sys_token");
        int recovered = 0;
        if (res.ok()) {
            auto& cfg = JwtUtils::config();
            for (int i = 0; i < res.rows(); ++i) {
                std::string key = res.str(i, 0);
                std::string val = res.str(i, 1);
                Json::Value j; Json::Reader r;
                if (!r.parse(val, j)) continue;
                auto user = LoginUser::fromJson(j);
                TokenCache::instance().set(key, user, cfg.expireMinutes);
                ++recovered;
            }
        }
        LOG_INFO << "[TokenRestore] 已恢复 " << recovered << " 个在线会话";
        std::cout << "[TokenRestore] 恢复 " << recovered << " 个在线会话" << std::endl;
    }

    // ── 启动时归档过期日志（防止 sys_oper_log / sys_logininfor 表无限膨胀）
    {
        auto& db = DatabaseService::instance();
        int operDays  = 90;
        int loginDays = 180;
        try {
            auto& cfg = drogon::app().getCustomConfig();
            if (cfg.isMember("retention")) {
                operDays  = cfg["retention"].get("oper_log_days",  90).asInt();
                loginDays = cfg["retention"].get("login_log_days", 180).asInt();
            }
        } catch (...) {}
        if (operDays > 0) {
            std::string d = std::to_string(operDays);
            db.execParams(
                "DELETE FROM sys_oper_log WHERE oper_time < NOW() - ($1 || ' days')::INTERVAL",
                {d});
        }
        if (loginDays > 0) {
            std::string d = std::to_string(loginDays);
            db.execParams(
                "DELETE FROM sys_logininfor WHERE login_time < NOW() - ($1 || ' days')::INTERVAL",
                {d});
        }
        LOG_INFO << "[Retention] sys_oper_log >" << operDays << "d, sys_logininfor >" << loginDays << "d cleaned";
    }
}
