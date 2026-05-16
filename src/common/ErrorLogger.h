#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <sstream>

// 专用错误日志（NDJSON 格式，每行一个 JSON 对象）
// 文件：logs/error.log  轮转：超过 maxBytes 时 → error.1.log … error.N.log
// 线程安全；零外部依赖
//
// 输出示例：
// {"time":"2026-05-16 10:41:00.123","level":"ERROR","source":"Redis","msg":"SETEX failed, key=captcha_codes:abc"}
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
