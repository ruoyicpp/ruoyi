#pragma once

// ════════════════════════════════════════════════════════════════════════════
// StructuredLogger.h — 结构化日志系统
//
// 特性：
//   - JSON格式输出，便于ELK/Grafana解析
//   - 统一字段：timestamp, level, traceId, userId, requestId, message, fields
//   - 支持多输出目标：文件(.log/.jsonl)、控制台、syslog
//   - 敏感字段自动脱敏
//   - 异步写入，避免阻塞主线程
// ════════════════════════════════════════════════════════════════════════════

// Windows 头文件中的 ERROR 宏会与 enum class Level { ERROR } 冲突
#ifdef _WIN32
#undef ERROR
#endif

#include <string>
#include <memory>
#include <map>
#include <variant>
#include <vector>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <optional>
#include <sstream>

namespace structured_log {

// ─────────────────────────────────────────────────────────────────────────────
// 日志级别
// ─────────────────────────────────────────────────────────────────────────────
enum class Level {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    FATAL = 5
};

inline const char* levelToString(Level l) {
    switch (l) {
        case Level::TRACE: return "TRACE";
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO";
        case Level::WARN:  return "WARN";
        case Level::ERROR: return "ERROR";
        case Level::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 字段类型（支持多种数据类型）
// ─────────────────────────────────────────────────────────────────────────────
using FieldValue = std::variant<
    std::string, int, int64_t, unsigned, uint64_t,
    double, bool, std::nullptr_t
>;

struct Field {
    std::string key;
    FieldValue value;
};

// ─────────────────────────────────────────────────────────────────────────────
// 日志上下文（请求级数据，自动附加到每条日志）
// ─────────────────────────────────────────────────────────────────────────────
struct LogContext {
    std::string traceId;       // 全链路追踪ID
    std::string requestId;     // 请求ID
    int64_t userId = 0;       // 用户ID（0表示未登录）
    std::string userName;     // 用户名
    std::string clientIp;      // 客户端IP
    std::string requestPath;   // 请求路径
    std::string userAgent;    // User-Agent
    int64_t responseMs = 0;   // 响应耗时(ms)

    // 从上下文创建JSON片段
    std::string toJson() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// 日志写入器接口
// ─────────────────────────────────────────────────────────────────────────────
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(Level level, const std::string& json) = 0;
    virtual void flush() {}
    virtual void close() {}
    virtual std::string name() const { return "LogSink"; }
};

// 文件写入器
class FileSink : public LogSink {
public:
    FileSink(const std::string& path, bool jsonFormat = true, size_t maxSizeMb = 100);
    ~FileSink() override;

    void write(Level level, const std::string& json) override;
    void flush() override;

    // 轮转日志文件
    void rotate();

    // 获取当前日志文件路径
    const std::string& currentPath() const { return currentPath_; }

private:
    std::ofstream file_;
    std::string basePath_;
    std::string currentPath_;
    bool jsonFormat_;
    size_t maxSize_;
    std::atomic<size_t> currentSize_{0};
    std::mutex mutex_;
};

// 控制台写入器（支持ANSI颜色）
class ConsoleSink : public LogSink {
public:
    ConsoleSink(bool color = true, bool timestamps = true, bool jsonFormat = false);
    void write(Level level, const std::string& json) override;

    // 设置最小输出级别
    void setMinLevel(Level l) { minLevel_ = l; }

private:
    bool color_;
    bool timestamps_;
    bool jsonFormat_;
    Level minLevel_ = Level::INFO;
    std::mutex mutex_;
};

// 多路复用写入器
class MultiSink : public LogSink {
public:
    void addSink(std::shared_ptr<LogSink> sink, const std::string& name = "anon");
    void removeSink(const std::string& name);
    void write(Level level, const std::string& json) override;
    void flush() override;
    std::string name() const override { return "MultiSink"; }

private:
    std::map<std::string, std::shared_ptr<LogSink>> sinks_;
    std::mutex mutex_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 日志生成器（链式API）
// ─────────────────────────────────────────────────────────────────────────────
class Logger {
public:
    Logger(Level level, const char* file, int line, const char* func);

    // 添加上下文
    Logger& ctx(const LogContext& ctx);
    Logger& traceId(const std::string& id);
    Logger& requestId(const std::string& id);
    Logger& userId(int64_t id);
    Logger& userName(const std::string& name);
    Logger& clientIp(const std::string& ip);

    // 添加字段（链式调用）
    Logger& field(const std::string& key, const FieldValue& value);
    Logger& field(const std::string& key, const std::string& value) {
        return field(key, FieldValue{value});
    }
    Logger& field(const std::string& key, int value) {
        return field(key, FieldValue{value});
    }
    Logger& field(const std::string& key, int64_t value) {
        return field(key, FieldValue{value});
    }
    Logger& field(const std::string& key, double value) {
        return field(key, FieldValue{value});
    }
    Logger& field(const std::string& key, bool value) {
        return field(key, FieldValue{value});
    }

    // 添加带描述的字段
    Logger& message(const std::string& msg);
    Logger& error(const std::string& err);
    Logger& duration(int64_t ms);
    Logger& statusCode(int code);
    Logger& method(const std::string& m);
    Logger& path(const std::string& p);
    Logger& count(int n);

    // 输出
    ~Logger();
    std::string toJson() const;

private:
    Level level_;
    std::chrono::system_clock::time_point timestamp_;
    const char* file_;
    int line_;
    const char* func_;
    std::optional<LogContext> ctx_;
    std::map<std::string, FieldValue> fields_;
    std::string message_;
    std::string error_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 全局日志器管理器
// ─────────────────────────────────────────────────────────────────────────────
class LogManager {
public:
    static LogManager& instance();

    // 初始化（从配置文件加载）
    void init(const std::string& configPath = "config.json");

    // 初始化（从代码配置）
    struct Config {
        std::string logDir = "./logs";
        std::string logLevel = "INFO";
        bool console = true;
        bool jsonFile = true;
        bool textFile = false;
        size_t maxFileSizeMb = 100;
        int maxFiles = 10;
        bool async = true;           // 异步写入
        size_t queueSize = 10000;    // 异步队列大小
        std::vector<std::string> maskFields = {"password", "token", "secret", "key"}; // 脱敏字段
    };
    void init(const Config& cfg);

    // 获取日志器
    Logger logger(Level level, const char* file, int line, const char* func);

    // 设置最小日志级别
    void setLevel(Level l);

    // 设置请求上下文（当前线程）
    void setContext(const LogContext& ctx);
    void clearContext();

    // 获取当前上下文
    LogContext* currentContext();

    // 全局属性（自动附加到每条日志）
    void setGlobalField(const std::string& key, const FieldValue& value);
    void setServiceName(const std::string& name);
    void setServiceVersion(const std::string& ver);

    // 获取日志路径
    const std::string& logDir() const { return config_.logDir; }

    // 内部日志写入（Logger 析构函数使用）
    void log(Level level, const std::string& json) { writeLog(level, json); }

private:
    LogManager() = default;
    LogManager(const LogManager&) = delete;
    void operator=(const LogManager&) = delete;

    void writeLog(Level level, const std::string& json);

    Config config_;
    std::shared_ptr<LogSink> sink_;
    std::thread asyncThread_;
    std::atomic<bool> stop_{false};
    std::vector<LogContext> contextStack_;
    std::mutex contextMutex_;
    std::map<std::string, FieldValue> globalFields_;
    std::string serviceName_;
    std::string serviceVersion_;
    Level minLevel_ = Level::INFO;
};

// ─────────────────────────────────────────────────────────────────────────────
// 敏感信息脱敏
// ─────────────────────────────────────────────────────────────────────────────
namespace mask {
    std::string sensitive(const std::string& value, const std::string& type = "default");
    std::string phone(const std::string& phone);
    std::string email(const std::string& email);
    std::string idCard(const std::string& idCard);
    std::string bankCard(const std::string& card);
    std::string password(const std::string&);
    std::string token(const std::string&);
}

} // namespace structured_log

// 为了兼容 main.cc 中的 LOG_* 宏，映射到 Drogon 的日志系统
// 禁用 StructuredLogger 的宏，使用 Drogon 的日志
#ifndef LOG_TRACE
#define LOG_TRACE LOG_TRACE
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG LOG_DEBUG
#endif
#ifndef LOG_INFO
#define LOG_INFO LOG_INFO
#endif
#ifndef LOG_WARN
#define LOG_WARN LOG_WARN
#endif
#ifndef LOG_ERROR
#define LOG_ERROR LOG_ERROR
#endif
#ifndef LOG_FATAL
#define LOG_FATAL LOG_FATAL
#endif

// 带上下文的日志
#define LOGCTX(level) \
    ::structured_log::LogManager::instance().logger(level, __FILE__, __LINE__, __FUNCTION__) \
        .ctx(*::structured_log::LogManager::instance().currentContext())

// 常用场景宏
#define LOG_REQUEST(req) \
    LOG_INFO \
        .requestId(req->getHeader("X-Request-ID")) \
        .clientIp(req->getPeerAddr().toIp()) \
        .method(req->getMethodString()) \
        .path(req->getPath()) \
        .userAgent(req->getHeader("User-Agent"))

#define LOG_RESPONSE(durationMs, statusCode) \
        .duration(durationMs) \
        .field("status", statusCode)

#define LOG_DB_QUERY(sql, durationMs) \
    LOG_DEBUG \
        .field("type", "db_query") \
        .field("sql", sql) \
        .field("duration_ms", durationMs)

#define LOG_AUTH(userId, success, reason) \
    LOG_INFO \
        .field("type", "auth") \
        .userId(userId) \
        .field("success", success) \
        .field("reason", reason)

#define LOG_SECURITY(event, ip, detail) \
    LOG_WARN \
        .field("type", "security") \
        .field("event", event) \
        .clientIp(ip) \
        .field("detail", detail)

// 条件日志（只在满足条件时输出）
#define LOG_IF(condition, level) \
    if (condition) LOG_##level
