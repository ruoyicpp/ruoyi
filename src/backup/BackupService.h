/**
 * @file BackupService.h
 * @brief 备份管理 — PG 定时备份（pg_dump）+ 备份文件管理 + 一键恢复
 *
 * 功能概述：
 *   - 定时备份：每日 schedule_hour 触发 pg_dump -Fc 自定义格式
 *   - 文件管理：backups/ 目录扫描、大小/时间列表、按 keep_count 滚动清理
 *   - 一键恢复：pg_restore -c 恢复（危险操作，需 confirm=true）
 *   - 备份日志：sys_backup_log 表记录每次备份结果
 *   - 依赖：系统需安装 postgresql-client（pg_dump/pg_restore 在 PATH）
 *
 * 配置项（config.json → backup）：
 *   - enabled: 总开关（默认 false）
 *   - dir: 备份目录（默认 backups）
 *   - keep_count: 保留份数（默认 7，超出删最旧）
 *   - schedule_hour: 每日备份小时 0-23（默认 3，-1=禁用定时）
 *   - 数据库连接：复用 config.json → database 段（host/port/dbname/user/passwd）
 */

#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "../services/DatabaseService.h"

namespace fs = std::filesystem;

/**
 * @class BackupService
 * @brief 备份服务单例
 */
class BackupService {
public:
    struct Config {
        bool        enabled      = false;
        std::string dir          = "backups";
        int         keepCount    = 7;
        int         scheduleHour = 3;    ///< -1 禁用定时
        // 数据库连接（从 config.json database 段读取）
        std::string dbHost = "127.0.0.1";
        int         dbPort = 5432;
        std::string dbName;
        std::string dbUser;
        std::string dbPass;
    };

    struct BackupFile {
        std::string name;
        int64_t     size;
        int64_t     mtime;
    };

    static BackupService& instance() {
        static BackupService inst;
        return inst;
    }

    void init(const Config& cfg) {
        cfg_ = cfg;
        if (!cfg_.enabled) { LOG_INFO << "[Backup] disabled"; return; }
        std::error_code ec;
        fs::create_directories(cfg_.dir, ec);
        LOG_INFO << "[Backup] enabled dir=" << cfg_.dir
                 << " keep=" << cfg_.keepCount
                 << " schedule=" << cfg_.scheduleHour << ":00"
                 << " db=" << cfg_.dbName << "@" << cfg_.dbHost;
    }

    bool isEnabled() const { return cfg_.enabled; }

    /**
     * @brief 执行一次备份（pg_dump -Fc）
     * @return 备份文件名，失败返回空串
     */
    std::string backupNow() {
        if (!cfg_.enabled || cfg_.dbName.empty()) return "";
        char ts[32];
        std::time_t now = std::time(nullptr);
        std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", std::localtime(&now));
        std::string fname = "ruoyi_" + cfg_.dbName + "_" + ts + ".dump";
        std::string fpath = cfg_.dir + "/" + fname;

        // PGPASSWORD 环境变量传密码（避免命令行明文）
        setEnv("PGPASSWORD", cfg_.dbPass);
        std::string cmd = "pg_dump -Fc -h " + cfg_.dbHost
                        + " -p " + std::to_string(cfg_.dbPort)
                        + " -U " + cfg_.dbUser
                        + " -d " + cfg_.dbName
                        + " -f \"" + fpath + "\"";
        int rc = std::system(cmd.c_str());
        bool ok = (rc == 0) && fs::exists(fpath) && fs::file_size(fpath) > 0;
        int64_t size = ok ? (int64_t)fs::file_size(fpath) : 0;

        logBackup(fname, ok, size, ok ? "" : "pg_dump exit=" + std::to_string(rc));
        if (ok) {
            LOG_INFO << "[Backup] " << fname << " " << size << " bytes";
            enforceRetention();
        } else {
            LOG_ERROR << "[Backup] failed: " << fname;
            std::error_code ec; fs::remove(fpath, ec);   // 清理残文件
        }
        return ok ? fname : "";
    }

    /// 备份文件列表（按时间倒序）
    std::vector<BackupFile> listFiles() {
        std::vector<BackupFile> out;
        std::error_code ec;
        if (!fs::exists(cfg_.dir)) return out;
        for (auto& e : fs::directory_iterator(cfg_.dir, ec)) {
            if (ec || !e.is_regular_file()) continue;
            if (e.path().extension() != ".dump") continue;
            BackupFile f;
            f.name  = e.path().filename().string();
            f.size  = (int64_t)e.file_size(ec);
            f.mtime = decltype(f.mtime)(
                std::chrono::duration_cast<std::chrono::seconds>(
                    e.last_write_time(ec).time_since_epoch()).count());
            out.push_back(f);
        }
        std::sort(out.begin(), out.end(),
                  [](const BackupFile& a, const BackupFile& b){ return a.mtime > b.mtime; });
        return out;
    }

    /**
     * @brief 一键恢复（pg_restore -c 清库恢复，危险操作）
     * @param fname 备份文件名（仅限 backups/ 目录内，防路径穿越）
     * @param confirm 必须传 true 才执行
     */
    bool restore(const std::string& fname, bool confirm, std::string& err) {
        if (!confirm) { err = "需 confirm=true 确认恢复（会清空现有数据）"; return false; }
        // 防路径穿越
        if (fname.find("..") != std::string::npos ||
            fname.find('/') != std::string::npos ||
            fname.find('\\') != std::string::npos) {
            err = "非法文件名"; return false;
        }
        std::string fpath = cfg_.dir + "/" + fname;
        if (!fs::exists(fpath)) { err = "备份文件不存在"; return false; }

        setEnv("PGPASSWORD", cfg_.dbPass);
        std::string cmd = "pg_restore -c -h " + cfg_.dbHost
                        + " -p " + std::to_string(cfg_.dbPort)
                        + " -U " + cfg_.dbUser
                        + " -d " + cfg_.dbName
                        + " \"" + fpath + "\"";
        int rc = std::system(cmd.c_str());
        // pg_restore -c 对不存在的对象会报 warning 但返回非零，视为部分成功
        bool ok = (rc == 0);
        if (!ok) err = "pg_restore exit=" + std::to_string(rc) +
                       "（-c 模式下非零可能是对象不存在的 warning，请检查数据）";
        LOG_WARN << "[Backup] restore " << fname << " rc=" << rc;
        return ok;
    }

    /// 删除备份文件
    bool remove(const std::string& fname, std::string& err) {
        if (fname.find("..") != std::string::npos ||
            fname.find('/') != std::string::npos ||
            fname.find('\\') != std::string::npos) {
            err = "非法文件名"; return false;
        }
        std::error_code ec;
        return fs::remove(cfg_.dir + "/" + fname, ec) && !ec;
    }

    /// 定时检查：到点执行备份（main.cc 每小时调用）
    void tickHourly() {
        if (!cfg_.enabled || cfg_.scheduleHour < 0) return;
        std::time_t now = std::time(nullptr);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &now);
#else
        localtime_r(&now, &tm);
#endif
        if (tm.tm_hour != cfg_.scheduleHour) return;
        // 同一小时只跑一次
        int today = tm.tm_yday;
        if (lastRunDay_ == today) return;
        lastRunDay_ = today;
        backupNow();
    }

private:
    BackupService() = default;

    /// 滚动清理：超出 keep_count 删最旧
    void enforceRetention() {
        auto files = listFiles();
        if ((int)files.size() <= cfg_.keepCount) return;
        for (size_t i = cfg_.keepCount; i < files.size(); ++i) {
            std::error_code ec;
            fs::remove(cfg_.dir + "/" + files[i].name, ec);
            LOG_INFO << "[Backup] retention removed " << files[i].name;
        }
    }

    /// 备份日志落库
    void logBackup(const std::string& fname, bool ok, int64_t size,
                   const std::string& err) {
        DatabaseService::instance().execParams(
            "INSERT INTO sys_backup_log(filename,size_bytes,status,error_msg) "
            "VALUES($1,$2,$3,$4)",
            {fname, std::to_string(size), ok ? "success" : "fail", err});
    }

    /// 跨平台 setenv
    static void setEnv(const std::string& k, const std::string& v) {
#ifdef _WIN32
        _putenv((k + "=" + v).c_str());
#else
        setenv(k.c_str(), v.c_str(), 1);
#endif
    }

    Config cfg_;
    int    lastRunDay_ = -1;
};
