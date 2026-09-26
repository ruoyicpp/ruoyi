/**
 * @file LogRetention.h
 * @brief 日志标准化留存 — PG 日志表 + 本地日志文件 + 审计日志统一清理
 *
 * 功能概述：
 *   - PG 日志表：sys_oper_log / sys_logininfor 按天数截断（DELETE WHERE time < cutoff）
 *   - 本地文件：logs/ 目录递归扫描，按 last_write_time 删除过期 .log/.ndjson
 *   - 审计联动：调用 AuditQueue::cleanupExpired() 清理 Manticore 过期文档
 *   - 单一定时器：main.cc 每日执行一次 runOnce()，替代分散的清理逻辑
 *   - 归档预留：archive_dir 非空时先移动再删除（归档到指定目录）
 *
 * 清理范围：
 *   - sys_oper_log.oper_time     — 操作日志（默认 90 天）
 *   - sys_logininfor.login_time  — 登录日志（默认 90 天）
 *   - logs/**\/*.{log,ndjson}    — 本地日志文件（默认 30 天）
 *   - Manticore waf_logs         — 审计文档（沿用 audit.retention_days）
 *
 * 配置项（config.json → logging.retention）：
 *   - enabled: 总开关（默认 true）
 *   - oper_log_days: 操作日志留存（默认 90）
 *   - login_log_days: 登录日志留存（默认 90）
 *   - local_log_days: 本地文件留存（默认 30）
 *   - log_dir: 日志根目录（默认 logs）
 *   - archive_dir: 归档目录（默认空=直接删除；非空则先 mv 再删）
 */

#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <trantor/utils/Logger.h>
#include "../services/DatabaseService.h"
#include "../audit/AuditQueue.h"

namespace fs = std::filesystem;

/**
 * @class LogRetention
 * @brief 日志留存清理单例
 */
class LogRetention {
public:
    struct Config {
        bool        enabled      = true;
        int         operLogDays  = 90;
        int         loginLogDays = 90;
        int         localLogDays = 30;
        int         maxFiles     = 5000;  ///< 目录内日志文件硬上限（防爆量，超了按 mtime 从旧到新删）
        int64_t     maxTotalMb   = 2048;  ///< 日志文件总大小硬上限 MB
        std::string logDir       = "logs";
        std::string archiveDir;   ///< 空=直接删除
    };

    static LogRetention& instance() {
        static LogRetention inst;
        return inst;
    }

    void init(const Config& cfg) {
        cfg_ = cfg;
        if (!cfg_.enabled) { LOG_INFO << "[LogRetention] disabled"; return; }
        LOG_INFO << "[LogRetention] oper=" << cfg_.operLogDays << "d"
                 << " login=" << cfg_.loginLogDays << "d"
                 << " local=" << cfg_.localLogDays << "d"
                 << " maxFiles=" << cfg_.maxFiles
                 << " maxTotalMb=" << cfg_.maxTotalMb
                 << " dir=" << cfg_.logDir
                 << (cfg_.archiveDir.empty() ? "" : " archive=" + cfg_.archiveDir);
    }

    /**
     * @brief 执行一次全量清理（每日定时器调用）
     * @return 清理统计摘要
     */
    std::string runOnce() {
        if (!cfg_.enabled) return "disabled";
        int pgRows = cleanPgTables();
        int files  = cleanLocalFiles();
        AuditQueue::instance().cleanupExpired();   // Manticore 联动
        std::string summary = "pg_rows~" + std::to_string(pgRows) +
                              " files=" + std::to_string(files);
        LOG_INFO << "[LogRetention] cleanup done: " << summary;
        return summary;
    }

    bool isEnabled() const { return cfg_.enabled; }

    /// 硬上限兜底：文件数/总大小超限 → 按 mtime 从旧到新删（防爆量场景）
    /// 可在 runOnce 外独立每小时调用（目录健康时开销可忽略）
    int enforceCaps() {
        if (!cfg_.enabled) return 0;
        if (!fs::exists(cfg_.logDir)) return 0;
        struct Ent { fs::path p; fs::file_time_type mt; uintmax_t sz; };
        std::vector<Ent> all;
        std::error_code ec;
        for (auto& e : fs::recursive_directory_iterator(cfg_.logDir, ec)) {
            if (ec || !e.is_regular_file()) continue;
            if (!isLogExt(e.path().extension().string())) continue;
            Ent en{e.path(), e.last_write_time(ec), e.file_size(ec)};
            if (ec) { ec.clear(); continue; }
            all.push_back(std::move(en));
        }
        int64_t totalMb = 0;
        for (auto& en : all) totalMb += (int64_t)(en.sz >> 20);
        if ((int)all.size() <= cfg_.maxFiles && totalMb <= cfg_.maxTotalMb) return 0;

        // 最旧的在前面，边删边计数
        std::sort(all.begin(), all.end(),
                  [](const Ent& a, const Ent& b){ return a.mt < b.mt; });
        int removed = 0;
        size_t count = all.size();
        for (auto& en : all) {
            if (count <= (size_t)cfg_.maxFiles && totalMb <= cfg_.maxTotalMb) break;
            if (fs::remove(en.p, ec)) { removed++; --count; totalMb -= (int64_t)(en.sz >> 20); }
            ec.clear();
        }
        if (removed) LOG_INFO << "[LogRetention] enforceCaps removed " << removed
                              << " files (cap files=" << cfg_.maxFiles
                              << " mb=" << cfg_.maxTotalMb << ")";
        return removed;
    }

private:
    LogRetention() = default;

    /// 清理 PG 日志表（oper_time/login_time 为 timestamp，传 ISO 截止串）
    int cleanPgTables() {
        auto cutoff = [&](int days) {
            std::time_t t = std::time(nullptr) - (std::time_t)days * 86400;
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
                          std::localtime(&t));
            return std::string(buf);
        };
        int n = 0;
        // execParams 自动路由 PG/SQLite，失败静默（表可能不存在）
        if (DatabaseService::instance().execParams(
                "DELETE FROM sys_oper_log WHERE oper_time < $1",
                {cutoff(cfg_.operLogDays)})) n++;
        if (DatabaseService::instance().execParams(
                "DELETE FROM sys_logininfor WHERE login_time < $1",
                {cutoff(cfg_.loginLogDays)})) n++;
        return n;
    }

    /// 清理本地日志文件（递归 logs/，按 mtime 删除过期日志文件）
    int cleanLocalFiles() {
        int removed = 0;
        if (!fs::exists(cfg_.logDir)) return 0;
        auto cutoff = fs::file_time_type::clock::now() -
                      std::chrono::hours(24 * cfg_.localLogDays);
        std::error_code ec;
        for (auto& e : fs::recursive_directory_iterator(cfg_.logDir, ec)) {
            if (ec || !e.is_regular_file()) continue;
            if (!isLogExt(e.path().extension().string())) continue;
            auto mt = e.last_write_time(ec);
            if (ec || mt >= cutoff) continue;
            if (!cfg_.archiveDir.empty()) {
                // 归档：保留相对路径结构
                auto rel = fs::relative(e.path(), cfg_.logDir, ec);
                auto dst = fs::path(cfg_.archiveDir) / rel;
                fs::create_directories(dst.parent_path(), ec);
                fs::rename(e.path(), dst, ec);
                if (!ec) { removed++; continue; }
                ec.clear();   // 归档失败则直接删
            }
            if (fs::remove(e.path(), ec)) removed++;
            ec.clear();
        }
        removed += enforceCaps();   // 天数之外再卡数量/容量上限
        return removed;
    }

    static bool isLogExt(const std::string& ext) {
        return ext == ".log" || ext == ".ndjson" || ext == ".jsonl" || ext == ".txt";
    }

    Config cfg_;
};
