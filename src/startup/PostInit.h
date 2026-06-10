/**
 * @file PostInit.h
 * @brief 应用启动后初始化 — 会话恢复和日志清理
 * 
 * 功能概述：
 *   - 会话恢复：从数据库恢复上次关闭前的在线会话
 *   - 过期会话清理：删除已过期的会话记录
 *   - 日志归档：定期清理过期的操作日志和登录日志
 *   - 表空间管理：防止日志表无限膨胀
 * 
 * 执行时机：
 *   - 应用启动完成后，在 main 函数中调用
 *   - 在所有服务初始化完成后执行
 * 
 * 会话恢复流程：
 *   1. 从 sys_token 表查询所有有效的会话
 *   2. 删除已过期的会话记录
 *   3. 将有效会话加载到 TokenCache 中
 *   4. 记录恢复的会话数量
 * 
 * 日志清理规则：
 *   - sys_oper_log：默认保留 90 天
 *   - sys_logininfor：默认保留 180 天
 *   - 可通过 config.json 中的 retention 配置修改
 * 
 * 配置示例（config.json）：
 *   {
 *     "retention": {
 *       "oper_log_days": 90,
 *       "login_log_days": 180
 *     }
 *   }
 * 
 * @see TokenCache - 令牌缓存
 * @see DatabaseService - 数据库服务
 * @see JwtUtils - JWT 工具
 */

#pragma once
#include "AppIncludes.h"

/**
 * @brief 运行启动后初始化
 * 
 * 执行以下任务：
 *   1. 从数据库恢复在线会话
 *   2. 清理过期的日志记录
 * 
 * 此函数应在应用启动完成后调用，确保所有服务都已初始化。
 */
inline void runPostInit() {
    // ── 启动时从 sys_token 恢复在线会话（Token 持久化）────────────────────
    {
        auto& db = DatabaseService::instance();
        
        // 获取当前时间戳（毫秒）
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // 删除已过期的会话记录
        db.execParams("DELETE FROM sys_token WHERE expire_time<$1",
                      {std::to_string(nowMs)});
        
        // 查询所有有效的会话
        auto res = db.query("SELECT token_key,token_value FROM sys_token");
        int recovered = 0;
        
        if (res.ok()) {
            auto& cfg = JwtUtils::config();
            
            // 遍历每个会话记录
            for (int i = 0; i < res.rows(); ++i) {
                std::string key = res.str(i, 0);  // token_key
                std::string val = res.str(i, 1);  // token_value (JSON)
                
                // 解析 JSON 格式的用户信息
                Json::Value j;
                Json::Reader r;
                if (!r.parse(val, j)) continue;
                
                // 从 JSON 恢复用户信息
                auto user = LoginUser::fromJson(j);
                
                // 将会话加载到缓存中
                TokenCache::instance().set(key, user, cfg.expireMinutes);
                ++recovered;
            }
        }
        
        // 记录恢复的会话数量
        LOG_INFO << "[TokenRestore] 已恢复 " << recovered << " 个在线会话";
        std::cout << "[TokenRestore] 恢复 " << recovered << " 个在线会话" << std::endl;
    }

    // ── 启动时归档过期日志（防止 sys_oper_log / sys_logininfor 表无限膨胀）
    {
        auto& db = DatabaseService::instance();
        
        // 默认保留天数
        int operDays  = 90;    // 操作日志保留 90 天
        int loginDays = 180;   // 登录日志保留 180 天
        
        // 从配置文件读取保留天数
        try {
            auto& cfg = drogon::app().getCustomConfig();
            if (cfg.isMember("retention")) {
                operDays  = cfg["retention"].get("oper_log_days",  90).asInt();
                loginDays = cfg["retention"].get("login_log_days", 180).asInt();
            }
        } catch (...) {}
        
        // 清理操作日志
        if (operDays > 0) {
            std::string d = std::to_string(operDays);
            db.execParams(
                "DELETE FROM sys_oper_log WHERE oper_time < NOW() - ($1 || ' days')::INTERVAL",
                {d});
        }
        
        // 清理登录日志
        if (loginDays > 0) {
            std::string d = std::to_string(loginDays);
            db.execParams(
                "DELETE FROM sys_logininfor WHERE login_time < NOW() - ($1 || ' days')::INTERVAL",
                {d});
        }
        
        // 记录清理结果
        LOG_INFO << "[Retention] sys_oper_log >" << operDays << "d, sys_logininfor >" << loginDays << "d cleaned";
    }
}
