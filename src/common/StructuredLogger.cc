#include "StructuredLogger.h"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <queue>
#include <json/json.h>

namespace structured_log {

// ─────────────────────────────────────────────────────────────────────────────
// 工具函数
// ─────────────────────────────────────────────────────────────────────────────
static std::string escapeJson(const std::string& s) {
    std::string r;
    r.reserve(s.size() * 1.2);
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\b': r += "\\b";  break;
            case '\f': r += "\\f";  break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if (c >= 0 && c < 32) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    r += buf;
                } else {
                    r += c;
                }
        }
    }
    return r;
}

static void fieldToJson(std::ostringstream& ss, const std::string& key, const FieldValue& v) {
    ss << "\"" << key << "\":";
    if (std::holds_alternative<std::string>(v)) {
        ss << "\"" << escapeJson(std::get<std::string>(v)) << "\"";
    } else if (std::holds_alternative<int>(v)) {
        ss << std::get<int>(v);
    } else if (std::holds_alternative<int64_t>(v)) {
        ss << std::get<int64_t>(v);
    } else if (std::holds_alternative<unsigned>(v)) {
        ss << std::get<unsigned>(v);
    } else if (std::holds_alternative<uint64_t>(v)) {
        ss << std::get<uint64_t>(v);
    } else if (std::holds_alternative<double>(v)) {
        ss << std::get<double>(v);
    } else if (std::holds_alternative<bool>(v)) {
        ss << (std::get<bool>(v) ? "true" : "false");
    } else {
        ss << "null";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// LogContext
// ─────────────────────────────────────────────────────────────────────────────
std::string LogContext::toJson() const {
    std::ostringstream ss;
    bool first = true;
    auto add = [&](const char* k, const std::string& v) {
        if (v.empty()) return;
        if (!first) ss << ",";
        ss << "\"" << k << "\":\"" << escapeJson(v) << "\"";
        first = false;
    };
    auto addNum = [&](const char* k, int64_t v) {
        if (v == 0) return;
        if (!first) ss << ",";
        ss << "\"" << k << "\":" << v;
        first = false;
    };

    add("trace_id", traceId);
    add("request_id", requestId);
    addNum("user_id", userId);
    add("user_name", userName);
    add("client_ip", clientIp);
    add("request_path", requestPath);
    add("user_agent", userAgent);
    addNum("response_ms", responseMs);
    return ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// FileSink
// ─────────────────────────────────────────────────────────────────────────────
FileSink::FileSink(const std::string& path, bool jsonFormat, size_t maxSizeMb)
    : basePath_(path), jsonFormat_(jsonFormat), maxSize_(maxSizeMb * 1024 * 1024) {
    currentPath_ = path;
    file_.open(path, std::ios::app | std::ios::binary);
    if (file_.is_open()) {
        file_.seekp(0, std::ios::end);
        currentSize_ = file_.tellp();
    }
}

FileSink::~FileSink() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void FileSink::write(Level level, const std::string& json) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!file_.is_open()) return;

    if (currentSize_ >= maxSize_) {
        rotate();
    }

    if (jsonFormat_) {
        file_ << json << "\n";
    } else {
        // 文本格式
        file_ << json << "\n";
    }
    file_.flush();
    currentSize_ = file_.tellp();
}

void FileSink::flush() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (file_.is_open()) file_.flush();
}

void FileSink::rotate() {
    if (!file_.is_open()) return;
    file_.close();

    // 重命名现有文件
    auto p = std::filesystem::path(currentPath_);
    auto stem = p.stem().string();
    auto ext = p.extension().string();
    auto parent = p.parent_path();

    time_t now = std::time(nullptr);
    struct tm t;
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &t);

    auto newPath = parent / (stem + "." + buf + ext);
    try {
        std::filesystem::rename(currentPath_, newPath);
    } catch (...) {}

    // 打开新文件
    file_.open(currentPath_, std::ios::app | std::ios::binary);
    currentSize_ = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// ConsoleSink
// ─────────────────────────────────────────────────────────────────────────────
ConsoleSink::ConsoleSink(bool color, bool timestamps, bool jsonFormat)
    : color_(color), timestamps_(timestamps), jsonFormat_(jsonFormat) {}

void ConsoleSink::write(Level level, const std::string& json) {
    if (level < minLevel_) return;

    std::lock_guard<std::mutex> lk(mutex_);

    std::ostringstream ss;
    if (timestamps_) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        time_t t = std::chrono::system_clock::to_time_t(now);
        struct tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        ss << "\033[90m" << buf << "." << std::setfill('0') << std::setw(3)
           << ms.count() << "\033[0m ";
    }

    // 颜色
    const char* colorCode = "";
    if (color_) {
        switch (level) {
            case Level::TRACE: colorCode = "\033[90m"; break;  // 灰色
            case Level::DEBUG: colorCode = "\033[36m"; break;  // 青色
            case Level::INFO:  colorCode = "\033[32m"; break;  // 绿色
            case Level::WARN:  colorCode = "\033[33m"; break;  // 黄色
            case Level::ERROR: colorCode = "\033[31m"; break;  // 红色
            case Level::FATAL: colorCode = "\033[35m"; break;  // 紫色
        }
    }

    ss << colorCode << "[" << levelToString(level) << "]\033[0m ";

    if (jsonFormat_) {
        // 简化JSON输出，提取关键字段（使用简单的字符串解析，避免 <regex> 兼容问题）
        try {
            // 提取 "msg":"xxx"  (简化查找)
            size_t msgPos = json.find("\"msg\":\"");
            if (msgPos != std::string::npos) {
                size_t start = msgPos + 7;
                size_t end = json.find('"', start);
                if (end != std::string::npos) {
                    ss << "\033[37m" << json.substr(start, end - start) << "\033[0m";
                }
            }
            // 提取字段名  (简化查找 key":)
            size_t pos = 0;
            while ((pos = json.find("\"", pos)) != std::string::npos) {
                size_t colon = json.find(":", pos);
                size_t nextQuote = json.find("\"", pos + 1);
                if (colon != std::string::npos && nextQuote != std::string::npos &&
                    colon < nextQuote && colon > pos) {
                    // 检查是否像字段名 (后面跟着 :)
                    std::string possibleKey = json.substr(pos + 1, nextQuote - pos - 1);
                    if (!possibleKey.empty() && possibleKey.find(' ') == std::string::npos &&
                        possibleKey.find('\\') == std::string::npos) {
                        // 可能是字段名，跳过常见值
                        if (possibleKey != "msg" && possibleKey != "level" && 
                            possibleKey != "timestamp" && possibleKey != "location" &&
                            possibleKey != "fields" && possibleKey != "service" &&
                            possibleKey != "version") {
                            // 简单的启发式：key 太长或太短跳过
                            if (possibleKey.length() <= 30) {
                                ss << " \033[90m" << possibleKey << "=\033[0m";
                            }
                        }
                    }
                }
                pos++;
            }
        } catch (...) {
            ss << json;
        }
    } else {
        ss << json;
    }

    std::cout << ss.str() << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
// MultiSink
// ─────────────────────────────────────────────────────────────────────────────
void MultiSink::addSink(std::shared_ptr<LogSink> sink, const std::string& name) {
    std::lock_guard<std::mutex> lk(mutex_);
    sinks_[name.empty() ? "anon" : name] = sink;
}

void MultiSink::removeSink(const std::string& name) {
    std::lock_guard<std::mutex> lk(mutex_);
    sinks_.erase(name);
}

void MultiSink::write(Level level, const std::string& json) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& [name, sink] : sinks_) {
        try {
            sink->write(level, json);
        } catch (...) {}
    }
}

void MultiSink::flush() {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& [name, sink] : sinks_) {
        try { sink->flush(); } catch (...) {}
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Logger
// ─────────────────────────────────────────────────────────────────────────────
Logger::Logger(Level level, const char* file, int line, const char* func)
    : level_(level), file_(file), line_(line), func_(func) {
    timestamp_ = std::chrono::system_clock::now();
}

Logger& Logger::ctx(const LogContext& c) {
    ctx_ = c;
    return *this;
}

Logger& Logger::traceId(const std::string& id) {
    if (!ctx_) ctx_.emplace();
    ctx_->traceId = id;
    return *this;
}

Logger& Logger::requestId(const std::string& id) {
    if (!ctx_) ctx_.emplace();
    ctx_->requestId = id;
    return *this;
}

Logger& Logger::userId(int64_t id) {
    if (!ctx_) ctx_.emplace();
    ctx_->userId = id;
    return *this;
}

Logger& Logger::userName(const std::string& name) {
    if (!ctx_) ctx_.emplace();
    ctx_->userName = name;
    return *this;
}

Logger& Logger::clientIp(const std::string& ip) {
    if (!ctx_) ctx_.emplace();
    ctx_->clientIp = ip;
    return *this;
}

Logger& Logger::field(const std::string& key, const FieldValue& value) {
    fields_[key] = value;
    return *this;
}

Logger& Logger::message(const std::string& msg) {
    message_ = msg;
    return *this;
}

Logger& Logger::error(const std::string& err) {
    error_ = err;
    return *this;
}

Logger& Logger::duration(int64_t ms) {
    return field("duration_ms", ms);
}

Logger& Logger::statusCode(int code) {
    return field("status", code);
}

Logger& Logger::method(const std::string& m) {
    return field("method", m);
}

Logger& Logger::path(const std::string& p) {
    if (!ctx_) ctx_.emplace();
    ctx_->requestPath = p;
    return *this;
}

Logger& Logger::count(int n) {
    return field("count", n);
}

std::string Logger::toJson() const {
    std::ostringstream ss;
    ss << "{";

    // timestamp
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_.time_since_epoch()).count();
    ss << "\"timestamp\":" << ms << ",";
    ss << "\"level\":\"" << levelToString(level_) << "\",";

    // location
    ss << "\"location\":{";
    ss << "\"file\":\"" << escapeJson(file_) << "\",";
    ss << "\"line\":" << line_ << ",";
    ss << "\"function\":\"" << escapeJson(func_) << "\"";
    ss << "},";

    // context
    if (ctx_) {
        std::string ctxJson = ctx_->toJson();
        if (!ctxJson.empty()) {
            ss << "\"context\":{" << ctxJson << "},";
        }
    }

    // fields
    ss << "\"fields\":{";
    bool first = true;
    for (auto& [k, v] : fields_) {
        if (!first) ss << ",";
        fieldToJson(ss, k, v);
        first = false;
    }
    ss << "}";

    // message
    if (!message_.empty()) {
        ss << ",\"msg\":\"" << escapeJson(message_) << "\"";
    }

    // error
    if (!error_.empty()) {
        ss << ",\"error\":\"" << escapeJson(error_) << "\"";
    }

    ss << "}";
    return ss.str();
}

Logger::~Logger() {
    try {
        LogManager::instance().log(level_, toJson());
    } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// LogManager
// ─────────────────────────────────────────────────────────────────────────────
LogManager& LogManager::instance() {
    static LogManager mgr;
    return mgr;
}

void LogManager::init(const std::string& configPath) {
    Config cfg;
    try {
        std::ifstream f(configPath);
        if (f.is_open()) {
            Json::Value j;
            f >> j;
            if (j.isMember("logging")) {
                auto& lg = j["logging"];
                cfg.logDir = lg.get("dir", cfg.logDir).asString();
                cfg.logLevel = lg.get("level", cfg.logLevel).asString();
                cfg.console = lg.get("console", cfg.console).asBool();
                cfg.jsonFile = lg.get("json_file", cfg.jsonFile).asBool();
                cfg.maxFileSizeMb = lg.get("max_file_size_mb", cfg.maxFileSizeMb).asInt();
                cfg.async = lg.get("async", cfg.async).asBool();
            }
        }
    } catch (...) {}
    init(cfg);
}

void LogManager::init(const Config& cfg) {
    config_ = cfg;

    // 设置最小级别
    if (cfg.logLevel == "TRACE") minLevel_ = Level::TRACE;
    else if (cfg.logLevel == "DEBUG") minLevel_ = Level::DEBUG;
    else if (cfg.logLevel == "WARN") minLevel_ = Level::WARN;
    else if (cfg.logLevel == "ERROR") minLevel_ = Level::ERROR;
    else minLevel_ = Level::INFO;

    // 创建目录
    std::filesystem::create_directories(cfg.logDir);

    // 创建输出目标
    auto multi = std::make_shared<MultiSink>();

    // 文件输出
    if (cfg.jsonFile) {
        auto filePath = (std::filesystem::path(cfg.logDir) / "app.jsonl").string();
        multi->addSink(std::make_shared<FileSink>(filePath, true, cfg.maxFileSizeMb));
    }

    // 控制台输出
    if (cfg.console) {
        auto console = std::make_shared<ConsoleSink>(true, true);
        console->setMinLevel(minLevel_);
        multi->addSink(console);
    }

    sink_ = multi;

    // 异步线程
    if (cfg.async) {
        stop_ = false;
        asyncThread_ = std::thread([this]() {
            while (!stop_) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                sink_->flush();
            }
        });
    }
}

Logger LogManager::logger(Level level, const char* file, int line, const char* func) {
    Logger lg(level, file, line, func);
    if (!contextStack_.empty()) {
        lg.ctx(contextStack_.back());
    }
    return lg;
}

void LogManager::setLevel(Level l) {
    minLevel_ = l;
}

void LogManager::setContext(const LogContext& ctx) {
    std::lock_guard<std::mutex> lk(contextMutex_);
    contextStack_.push_back(ctx);
}

void LogManager::clearContext() {
    std::lock_guard<std::mutex> lk(contextMutex_);
    if (!contextStack_.empty()) {
        contextStack_.pop_back();
    }
}

LogContext* LogManager::currentContext() {
    std::lock_guard<std::mutex> lk(contextMutex_);
    if (contextStack_.empty()) return nullptr;
    return &contextStack_.back();
}

void LogManager::setGlobalField(const std::string& key, const FieldValue& value) {
    std::lock_guard<std::mutex> lk(contextMutex_);
    globalFields_[key] = value;
}

void LogManager::setServiceName(const std::string& name) {
    serviceName_ = name;
    setGlobalField("service", name);
}

void LogManager::setServiceVersion(const std::string& ver) {
    serviceVersion_ = ver;
    setGlobalField("version", ver);
}

void LogManager::writeLog(Level level, const std::string& json) {
    if (level < minLevel_) return;
    if (sink_) {
        sink_->write(level, json);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 敏感信息脱敏
// ─────────────────────────────────────────────────────────────────────────────
namespace mask {
    std::string sensitive(const std::string& value, const std::string&) {
        if (value.size() <= 4) return "***";
        return value.substr(0, 2) + "***" + value.substr(value.size() - 2);
    }

    std::string phone(const std::string& phone) {
        if (phone.size() != 11) return phone;
        return phone.substr(0, 3) + "****" + phone.substr(7);
    }

    std::string email(const std::string& email) {
        auto at = email.find('@');
        if (at == std::string::npos) return email;
        auto name = email.substr(0, at);
        if (name.size() <= 2) return "**" + email.substr(at);
        return name.substr(0, 2) + "***" + email.substr(at);
    }

    std::string idCard(const std::string& id) {
        if (id.size() != 18) return id;
        return id.substr(0, 6) + "********" + id.substr(14);
    }

    std::string bankCard(const std::string& card) {
        if (card.size() < 8) return card;
        return card.substr(0, 4) + " **** **** " + card.substr(card.size() - 4);
    }

    std::string password(const std::string&) { return "***MASKED***"; }
    std::string token(const std::string&) { return "***TOKEN***"; }
}

} // namespace structured_log
