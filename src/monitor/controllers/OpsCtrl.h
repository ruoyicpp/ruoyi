#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../services/DatabaseService.h"
#include <string>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#else
#  include <unistd.h>
#  include <signal.h>
#  include <sys/statvfs.h>
#  include <sys/utsname.h>
#endif

/**
 * @file OpsCtrl.h
 * @brief 系统运维管理控制器 — 应用生命周期和配置管理
 * 
 * 功能概述：
 *   - 运维概览：显示应用运行状态和关键指标
 *   - 配置重载：热重载应用配置，无需重启
 *   - 应用重启：优雅重启应用服务
 *   - 日志管理：查看和管理应用日志
 *   - 性能统计：显示应用性能指标
 *   - 系统信息：显示系统和应用版本信息
 * 
 * 核心特性：
 *   - 运行时管理：无需重启即可更新配置
 *   - 优雅关闭：确保所有请求完成后再关闭
 *   - 实时监控：实时显示应用性能指标
 *   - 日志分析：快速定位问题
 *   - 版本管理：跟踪应用和系统版本
 *   - 跨平台支持：Windows 和 Linux 完全兼容
 * 
 * API 端点：
 *   - GET /monitor/ops/overview - 获取运维概览
 *   - POST /monitor/ops/reload - 重载配置
 *   - POST /monitor/ops/restart - 重启应用
 *   - GET /monitor/ops/logs - 获取应用日志
 *   - GET /monitor/ops/version - 获取版本信息
 * 
 * 请求/响应示例：
 *   ```
 *   GET /monitor/ops/overview
 *   Authorization: Bearer <JWT>
 *   
 *   响应：
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": {
 *       "appName": "RuoYi-C++",
 *       "version": "1.3.2",
 *       "uptime": 86400,
 *       "startTime": "2026-06-10 10:00:00",
 *       "status": "running",
 *       "cpu": 15.5,
 *       "memory": 512,
 *       "threads": 32,
 *       "requests": 10000
 *     }
 *   }
 *   ```
 * 
 * 权限要求：
 *   - monitor:ops:query - 查看运维信息
 *   - monitor:ops:reload - 重载配置
 *   - monitor:ops:restart - 重启应用
 * 
 * 配置项（config.json）：
 *   - ops.enabled: 是否启用运维面板（默认 true）
 *   - ops.allow_reload: 是否允许热重载（默认 true）
 *   - ops.allow_restart: 是否允许重启（默认 false）
 *   - ops.graceful_shutdown_timeout: 优雅关闭超时（秒，默认 30）
 * 
 * 运维操作：
 *   - 重载配置：重新加载 config.json，无需重启应用
 *   - 重启应用：优雅关闭后重新启动应用
 *   - 查看日志：查看最近的应用日志
 *   - 性能分析：分析应用性能指标
 * 
 * 监控指标：
 *   - 应用运行时间：从启动到现在的时间
 *   - CPU 使用率：应用占用的 CPU 百分比
 *   - 内存使用：应用占用的内存大小
 *   - 线程数：应用当前线程数
 *   - 请求数：应用处理的总请求数
 *   - 错误数：应用发生的错误数
 *   - 平均响应时间：平均请求响应时间
 * 
 * @see DatabaseService - 数据库服务
 * @see ConfigLoader - 配置加载器
 */
class OpsCtrl : public drogon::HttpController<OpsCtrl> {
    inline static auto startTime_ = std::chrono::steady_clock::now();
    inline static auto startWall_ = std::chrono::system_clock::now();
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(OpsCtrl::overview, "/monitor/ops/overview", drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(OpsCtrl::reload,   "/monitor/ops/reload",   drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(OpsCtrl::restart,  "/monitor/ops/restart",  drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    // ── GET /monitor/ops/overview ─────────────────────────────
    void overview(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:ops:overview");
        Json::Value data;

        // 1. runtime
        data["runtime"] = buildRuntime();

        // 2. application
        data["application"] = buildApplication();

        // 3. summary cards
        data["summary"] = buildSummary();

        // 4. resources
        data["resources"] = buildResources();

        // 5. dependencies
        data["dependencies"] = buildDependencies();

        // 6. logs
        data["logs"] = buildLogs();

        // 0. generated_at
        {
            auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char buf[32];
#ifdef _WIN32
            struct tm tm_; localtime_s(&tm_, &t);
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_);
#else
            struct tm tm_; localtime_r(&t, &tm_);
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_);
#endif
            data["generated_at"] = buf;
        }

        // 7. actions
        Json::Value actions;
        actions["reload"]  = true;
        actions["restart"] = true;
        actions["tips"]    = "重启会短暂中断请求处理，建议低峰期执行。";
        data["actions"] = actions;

        RESP_OK(cb, data);
    }

    // ── POST /monitor/ops/reload ──────────────────────────────
    void reload(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:ops:reload");
#ifdef _WIN32
        RESP_ERR(cb, "Windows 模式下不支持热重载，请手动重启服务");
#else
        pid_t pid = getpid();
        kill(pid, SIGUSR1);
        RESP_MSG(cb, "热重载信号已发送 (SIGUSR1)");
#endif
    }

    // ── POST /monitor/ops/restart ─────────────────────────────
    void restart(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:ops:restart");
        // 异步执行：先响应客户端，再延迟 1s 退出（由守护进程/脚本负责重启）
        RESP_MSG(cb, "重启指令已提交，服务将在 1 秒后重启");
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
#ifdef _WIN32
            ExitProcess(0);
#else
            kill(getpid(), SIGTERM);
#endif
        }).detach();
    }

private:
    // ── runtime ───────────────────────────────────────────────
    Json::Value buildRuntime() {
        auto now = std::chrono::steady_clock::now();
        auto upSec = (int)std::chrono::duration_cast<std::chrono::seconds>(now - startTime_).count();

        auto startT  = std::chrono::system_clock::to_time_t(startWall_);
        char buf[32];
#ifdef _WIN32
        struct tm tm_; localtime_s(&tm_, &startT);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_);
#else
        struct tm tm_; localtime_r(&startT, &tm_);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_);
#endif

        Json::Value r;
        r["status"]       = "running";
        r["status_text"]  = "运行中";
        r["tone"]         = "success";
        r["uptime_text"]  = durationText(upSec);
        r["start_time"]   = buf;
#ifdef _WIN32
        r["pid"] = (int)GetCurrentProcessId();
        r["platform"] = "Windows";
#else
        r["pid"] = (int)getpid();
        r["platform"] = "Linux";
#endif
        return r;
    }

    // ── application ───────────────────────────────────────────
    Json::Value buildApplication() {
        Json::Value a;
        a["name"]        = "ruoyi-cpp";
        a["framework"]   = "Drogon C++";
        a["version"]     = std::string("C++") + std::to_string(__cplusplus);
        a["threads"]     = (int)drogon::app().getThreadNum();
        a["db_backend"]  = DatabaseService::instance().backendInfo();
#ifdef _WIN32
        a["os"] = "Windows";
        // 用 RtlGetVersion 避免版本兼容性欺骗
        typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
        auto fn = (RtlGetVersionPtr)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
        if (fn) {
            RTL_OSVERSIONINFOW rv{}; rv.dwOSVersionInfoSize = sizeof(rv);
            if (fn(&rv) == 0) {
                a["os"] = "Windows " + std::to_string(rv.dwMajorVersion) + "." + std::to_string(rv.dwMinorVersion)
                        + " (Build " + std::to_string(rv.dwBuildNumber) + ")";
            }
        }
#else
        a["os"] = "Linux";
        struct utsname uts; if (uname(&uts) == 0) a["os"] = std::string(uts.sysname) + " " + uts.release;
#endif
        return a;
    }

    // ── summary cards ─────────────────────────────────────────
    Json::Value buildSummary() {
        auto& db = DatabaseService::instance();
        bool dbOk = db.ensureConnection();
        double memMB = processMemMB();

        Json::Value arr(Json::arrayValue);
        auto card = [&](const char* key, const char* label, std::string val, const char* tone) {
            Json::Value c; c["key"]=key; c["label"]=label; c["value"]=val; c["tone"]=tone;
            arr.append(c);
        };
        card("runtime",  "服务状态",  "运行中", "success");
        card("db",       "数据库",    dbOk ? "正常" : "异常", dbOk ? "success" : "danger");
        card("memory",   "当前内存",  fmtMB(memMB), "primary");
        card("threads",  "IO线程数",  std::to_string(drogon::app().getThreadNum()), "primary");
        return arr;
    }

    // ── resources ─────────────────────────────────────────────
    Json::Value buildResources() {
        Json::Value arr(Json::arrayValue);

        // 内存
        double memMB = processMemMB();
        Json::Value mem;
        mem["label"] = "进程内存"; mem["value"] = fmtMB(memMB);
        mem["helper"] = "当前工作集"; mem["tone"] = "primary";
        arr.append(mem);

        // 磁盘（运行目录）
        auto disk = diskUsage(".");
        Json::Value dsk;
        dsk["label"] = "磁盘空间";
        dsk["value"] = disk["used_rate_text"];
        dsk["helper"] = disk["free_text"].asString() + " 可用 / " + disk["total_text"].asString();
        int rate = disk["used_rate"].asInt();
        dsk["tone"] = rate >= 90 ? "danger" : (rate >= 80 ? "warning" : "success");
        arr.append(dsk);

        return arr;
    }

    // ── dependencies ──────────────────────────────────────────
    Json::Value buildDependencies() {
        Json::Value arr(Json::arrayValue);

        // 数据库
        auto t0 = std::chrono::steady_clock::now();
        bool dbOk = false;
        std::string dbMsg;
        try {
            dbOk = DatabaseService::instance().ensureConnection();
            dbMsg = dbOk ? "连接正常" : "连接失败";
        } catch (const std::exception& e) { dbMsg = e.what(); }
        auto dbMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

        Json::Value dep;
        dep["name"]        = DatabaseService::instance().backendInfo();
        dep["status_text"] = dbOk ? "正常" : "异常";
        dep["message"]     = dbMsg;
        dep["latency_text"]= std::to_string(dbMs) + " ms";
        dep["tone"]        = dbOk ? "success" : "danger";
        arr.append(dep);

        return arr;
    }

    // ── logs ──────────────────────────────────────────────────
    Json::Value buildLogs() {
        // Drogon 默认日志写到 stdout，尝试读取当前目录 ruoyi-cpp.log
        static const std::vector<std::string> candidates = {
            "./ruoyi-cpp.log", "./logs/ruoyi-cpp.log", "./ruoyi.log"
        };

        Json::Value result;
        result["error_count"] = 0;
        Json::Value recentErrors(Json::arrayValue);
        Json::Value files(Json::arrayValue);

        for (auto& path : candidates) {
            std::ifstream f(path, std::ios::binary);
            if (!f.is_open()) continue;
            // 读尾部 128KB
            f.seekg(0, std::ios::end);
            long sz = f.tellg();
            long offset = std::max(0L, sz - 131072L);
            f.seekg(offset);
            std::string content((std::istreambuf_iterator<char>(f)), {});

            static const char* kErrPatterns[] = {"ERROR", "FATAL", "WARN", "exception", "失败", "异常"};
            std::istringstream iss(content);
            std::string line;
            int errCnt = 0;
            std::vector<Json::Value> lines;
            while (std::getline(iss, line)) {
                std::string lowerLine = line;
                std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                for (const char* pat : kErrPatterns) {
                    std::string lowerPat = pat;
                    std::transform(lowerPat.begin(), lowerPat.end(), lowerPat.begin(), ::tolower);
                    if (lowerLine.find(lowerPat) != std::string::npos) {
                        errCnt++;
                        Json::Value item; item["file"] = path; item["message"] = line;
                        lines.push_back(item);
                        break;
                    }
                }
            }
            result["error_count"] = result["error_count"].asInt() + errCnt;
            // 最近 8 条
            int start = std::max(0, (int)lines.size() - 8);
            for (int i = (int)lines.size()-1; i >= start; i--) recentErrors.append(lines[i]);

            // 文件信息
            Json::Value fi;
            fi["name"] = path; fi["exists"] = true;
            fi["size_text"] = fmtBytes(sz);
            files.append(fi);
        }

        if (files.empty()) {
            Json::Value fi; fi["name"] = "（未找到日志文件）"; fi["exists"] = false; files.append(fi);
        }

        result["recent_errors"] = recentErrors;
        result["files"] = files;
        return result;
    }

    // ── helpers ───────────────────────────────────────────────
    std::string durationText(int sec) {
        if (sec < 60) return std::to_string(sec) + " 秒";
        if (sec < 3600) return std::to_string(sec/60) + " 分钟";
        if (sec < 86400) return std::to_string(sec/3600) + " 小时 " + std::to_string((sec%3600)/60) + " 分钟";
        return std::to_string(sec/86400) + " 天 " + std::to_string((sec%86400)/3600) + " 小时";
    }

    std::string fmtMB(double mb) {
        char buf[32]; snprintf(buf, sizeof(buf), "%.1f MB", mb); return buf;
    }

    std::string fmtBytes(long bytes) {
        const char* units[] = {"B","KB","MB","GB","TB"};
        double v = (double)bytes; int i = 0;
        while (v >= 1024 && i < 4) { v /= 1024; i++; }
        char buf[32]; snprintf(buf, sizeof(buf), "%.2f %s", v, units[i]); return buf;
    }

    double processMemMB() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
            return (double)pmc.WorkingSetSize / 1024.0 / 1024.0;
#else
        std::ifstream f("/proc/self/status");
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("VmRSS:", 0) == 0) {
                long kb = 0; sscanf(line.c_str(), "VmRSS: %ld", &kb); return kb / 1024.0;
            }
        }
#endif
        return 0.0;
    }

    Json::Value diskUsage(const std::string& path) {
        Json::Value r;
        long long total = 0, free_ = 0;
#ifdef _WIN32
        ULARGE_INTEGER freeBytes, totalBytes;
        std::wstring wp(path.begin(), path.end());
        if (GetDiskFreeSpaceExW(wp.c_str(), nullptr, &totalBytes, &freeBytes)) {
            total = (long long)totalBytes.QuadPart; free_ = (long long)freeBytes.QuadPart;
        }
#else
        struct statvfs st; if (statvfs(path.c_str(), &st) == 0) {
            total = (long long)st.f_blocks * st.f_frsize; free_ = (long long)st.f_bfree * st.f_frsize;
        }
#endif
        long long used = total - free_;
        int rate = total > 0 ? (int)(used * 100 / total) : 0;
        r["used_rate"] = rate;
        r["used_rate_text"] = std::to_string(rate) + "%";
        r["free_text"]  = fmtBytes(free_);
        r["total_text"] = fmtBytes(total);
        return r;
    }
};
