/**
 * @file ErrorLogger.h
 * @brief 错误日志工具 — 专用的错误日志记录
 * 
 * 功能概述：
 *   - 错误记录：记录系统错误、警告、致命错误
 *   - NDJSON 格式：每行一个 JSON 对象，便于日志分析
 *   - 日志轮转：超过大小限制自动轮转日志文件
 *   - 线程安全：使用 mutex 保护并发写入
 * 
 * 日志文件：
 *   - 主日志：logs/error.json
 *   - 轮转文件：logs/error.1.json、logs/error.2.json 等
 *   - 轮转策略：当日志文件超过 maxBytes 时自动轮转
 * 
 * 日志格式（NDJSON）：
 *   每行一个 JSON 对象，包含以下字段：
 *   - time: 时间戳（格式：YYYY-MM-DD HH:MM:SS.mmm）
 *   - level: 日志级别（WARN、ERROR、FATAL）
 *   - source: 日志来源（如 "Redis"、"Database" 等）
 *   - msg: 日志消息
 * 
 * 使用示例：
 *   // 初始化
 *   ErrorLogger::instance().init("logs", 10*1024*1024, 5);
 *   
 *   // 记录错误
 *   ErrorLogger::instance().error("Redis", "SETEX failed, key=captcha_codes:abc");
 *   ErrorLogger::instance().warn("Database", "Slow query detected");
 *   ErrorLogger::instance().fatal("System", "Critical error occurred");
 * 
 * 特性：
 *   - 零外部依赖：不依赖 jsoncpp，自实现 JSON 转义
 *   - 线程安全：所有操作都通过 mutex 保护
 *   - 自动轮转：日志文件超过限制自动轮转
 *   - 时间戳：精确到毫秒
 * 
 * 配置项（config.json）：
 *   - errorlog.enabled: 是否启用错误日志（默认 true）
 *   - errorlog.dir: 日志目录（默认 "logs"）
 *   - errorlog.maxBytes: 单个日志文件最大大小（默认 10MB）
 *   - errorlog.maxFiles: 最多保留的日志文件数（默认 5）
 */

#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <sstream>

/**
 * @class ErrorLogger
 * @brief 错误日志记录器单例
 * 
 * 提供专用的错误日志记录功能，支持日志轮转和线程安全。
 * 日志格式为 NDJSON（Newline Delimited JSON），每行一个 JSON 对象。
 */
class ErrorLogger {
public:
    static ErrorLogger& instance() {
        static ErrorLogger inst;
        return inst;
    }

    void init(const std::string& dir = "logs",
              size_t maxBytes = 10 * 1024 * 1024,
              int    maxFiles = 5) {
        std::lock_guard<std::mutex> lk(mu_);
        dir_      = dir;
        maxBytes_ = maxBytes;
        maxFiles_ = maxFiles;
        path_     = dir + "/error.json";
        std::filesystem::create_directories(dir);
        file_.open(path_, std::ios::app | std::ios::binary);
    }

    void write(const char* level, const char* source, const std::string& msg) {
        if (!file_.is_open()) return;
        std::string line = buildJson(level, source, msg);
        std::lock_guard<std::mutex> lk(mu_);
        file_ << line;
        file_.flush();
        bytes_ += line.size();
        if (bytes_ >= maxBytes_) rotate();
    }

    void warn (const char* src, const std::string& msg) { write("WARN",  src, msg); }
    void error(const char* src, const std::string& msg) { write("ERROR", src, msg); }
    void fatal(const char* src, const std::string& msg) { write("FATAL", src, msg); }

private:
    ErrorLogger() = default;

    // 简单 JSON 转义（不依赖 jsoncpp）
    static std::string jsonEscape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (unsigned char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (c < 0x20) {
                        char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += (char)c;
                    }
            }
        }
        return out;
    }

    std::string buildJson(const char* level, const char* source, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        long long ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()).count() % 1000;
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        char msbuf[8];
        std::snprintf(msbuf, sizeof(msbuf), ".%03lld", ms);

        std::ostringstream o;
        o << "{\"time\":\"" << buf << msbuf << "\""
          << ",\"level\":\"" << level << "\""
          << ",\"source\":\"" << jsonEscape(source) << "\""
          << ",\"msg\":\"" << jsonEscape(msg) << "\"}\n";
        return o.str();
    }

    void rotate() {
        file_.close();
        std::filesystem::remove(dir_ + "/error." + std::to_string(maxFiles_) + ".json");
        for (int i = maxFiles_ - 1; i >= 1; --i)
            std::filesystem::rename(
                dir_ + "/error." + std::to_string(i) + ".json",
                dir_ + "/error." + std::to_string(i + 1) + ".json");
        std::filesystem::rename(path_, dir_ + "/error.1.json");
        file_.open(path_, std::ios::trunc | std::ios::binary);
        bytes_ = 0;
    }

    std::mutex    mu_;
    std::ofstream file_;
    std::string   dir_;
    std::string   path_;
    size_t        maxBytes_ = 10 * 1024 * 1024;
    int           maxFiles_ = 5;
    size_t        bytes_    = 0;
};

// ── 快捷宏 ──────────────────────────────────────────────────────
#define ELOG_WARN(src, msg)  ErrorLogger::instance().warn (src, msg)
#define ELOG_ERROR(src, msg) ErrorLogger::instance().error(src, msg)
#define ELOG_FATAL(src, msg) ErrorLogger::instance().fatal(src, msg)
