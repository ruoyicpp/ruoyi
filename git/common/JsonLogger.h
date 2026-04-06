#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <trantor/utils/Logger.h>

/**
 * JsonLogger — 将 trantor 的文本日志拦截并重格式化为 NDJSON
 *
 * 输出格式（每行一个 JSON 对象）：
 *   {"ts":"2026-03-28T03:41:23.123456","level":"INFO","thread":"0x1234",
 *    "file":"main.cc","line":150,"msg":"服务器已启动"}
 *
 * 用法（在 loadConfigFile 之后调用，保证覆盖 Drogon 的文件输出函数）：
 *   JsonLogger::instance().init("./logs", "ruoyi");
 */
class JsonLogger {
public:
    static JsonLogger& instance() {
        static JsonLogger inst;
        return inst;
    }

    // logDir: 日志目录；baseName: 文件名前缀
    // maxFileSizeBytes: 单文件上限（字节），超过后轮转；0 表示不限
    // keepFiles: 保留最近文件数（含当前）
    void init(const std::string& logDir, const std::string& baseName,
              size_t maxFileSizeBytes = 100ULL * 1024 * 1024,
              int keepFiles = 5) {
        logDir_          = logDir;
        baseName_        = baseName;
        maxFileSizeBytes_ = maxFileSizeBytes;
        keepFiles_       = keepFiles;
        currentSize_     = 0;
        openFile();

        // 覆盖 trantor 的输出函数（channel -1 = 全局默认通道）
        trantor::Logger::setOutputFunction(
            [this](const char* msg, uint64_t len) { write(msg, len); },
            [this]() { flush(); }
        );
    }

private:
    std::string   logDir_;
    std::string   baseName_;
    std::ofstream file_;
    std::mutex    mu_;
    std::string   lineBuf_;          // 未完成行的缓冲
    size_t        maxFileSizeBytes_ = 100ULL * 1024 * 1024;
    int           keepFiles_        = 5;
    std::atomic<size_t> currentSize_{0};
    std::string   currentPath_;

    // ── 文件管理 ──────────────────────────────────────────────────────────────
    void openFile() {
        std::time_t now = std::time(nullptr);
        std::tm* t = std::localtime(&now);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%y%m%d-%H%M%S", t);
        currentPath_ = logDir_ + "/" + baseName_ + "." + ts + ".jsonl";
        if (file_.is_open()) file_.close();
        file_.open(currentPath_, std::ios::app);
        // 若追加到已有文件，记录当前大小
        currentSize_ = std::filesystem::exists(currentPath_)
            ? (size_t)std::filesystem::file_size(currentPath_) : 0;
    }

    void rotateIfNeeded() {
        if (maxFileSizeBytes_ == 0) return;
        if (currentSize_ < maxFileSizeBytes_) return;
        openFile();
        pruneOldFiles();
    }

    void pruneOldFiles() {
        if (keepFiles_ <= 0) return;
        std::vector<std::filesystem::path> logs;
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(logDir_, ec)) {
            if (!e.is_regular_file(ec)) continue;
            auto name = e.path().filename().string();
            if (name.rfind(baseName_ + ".", 0) == 0 && name.size() > 6
                && name.substr(name.size() - 6) == ".jsonl")
                logs.push_back(e.path());
        }
        std::sort(logs.begin(), logs.end()); // 按名称（含时间戳）升序
        while ((int)logs.size() > keepFiles_) {
            std::filesystem::remove(logs.front(), ec);
            logs.erase(logs.begin());
        }
    }

    // ── 输出回调（由 trantor 各线程调用）────────────────────────────────────
    void write(const char* msg, uint64_t len) {
        std::lock_guard<std::mutex> lk(mu_);
        lineBuf_.append(msg, static_cast<size_t>(len));
        flushLines();
    }

    void flush() {
        std::lock_guard<std::mutex> lk(mu_);
        if (!lineBuf_.empty()) {
            flushLines();
        }
        if (file_.is_open()) file_.flush();
    }

    // 按换行符切分、逐行处理
    void flushLines() {
        size_t pos;
        while ((pos = lineBuf_.find('\n')) != std::string::npos) {
            std::string line = lineBuf_.substr(0, pos);
            lineBuf_.erase(0, pos + 1);
            if (!line.empty()) {
                std::string json = toJson(line);
                // 写 JSON 文件（轮转检查）
                if (file_.is_open()) {
                    file_ << json << '\n';
                    currentSize_ += json.size() + 1;
                    rotateIfNeeded();
                }
                // 控制台保留彩色文本（原行），方便开发时查看
                fputs((line + "\n").c_str(), stderr);
            }
        }
    }

    // ── 解析 trantor 文本行 → JSON ───────────────────────────────────────────
    // trantor 格式：YYYYMMDD HH:MM:SS.ffffff  threadId LEVEL  file:line - msg
    static std::string toJson(const std::string& raw) {
        const char* p   = raw.c_str();
        const char* end = p + raw.size();

        std::string ts, thread, level, file, lineno, msg;

        // 1. 日期 + 时间（"YYYYMMDD HH:MM:SS.ffffff"，共 24 chars + space）
        //    trantor 实际格式："%Y%m%d %H:%M:%S.%06lld"
        if (end - p >= 24) {
            // date part: YYYYMMDD → YYYY-MM-DD
            char date[11];
            snprintf(date, sizeof(date), "%.4s-%.2s-%.2s", p, p+4, p+6);
            p += 9; // skip "YYYYMMDD "
            // time part until next space
            const char* tEnd = (const char*)memchr(p, ' ', end - p);
            if (tEnd) {
                ts = std::string(date) + "T" + std::string(p, tEnd);
                p = tEnd + 1;
            }
        }

        // 2. Thread id（到下一个空格）
        skipSpaces(p, end);
        const char* tokEnd = nextSpace(p, end);
        thread.assign(p, tokEnd);
        p = skipSpaces(tokEnd, end);

        // 3. Level（到下一个空格或多空格）
        tokEnd = nextSpace(p, end);
        level.assign(p, tokEnd);
        trim(level);
        p = skipSpaces(tokEnd, end);

        // 4. file:line（到 " - "）
        const char* dash = findSeq(p, end, " - ", 3);
        if (dash) {
            std::string src(p, dash);
            size_t col = src.rfind(':');
            if (col != std::string::npos) {
                file   = src.substr(0, col);
                lineno = src.substr(col + 1);
            } else {
                file = src;
            }
            p = dash + 3;
        }

        // 5. 剩余即消息
        msg.assign(p, end);
        trim(msg);

        // 6. 构建 JSON（避免引入额外依赖，手工序列化）
        std::string j;
        j.reserve(128 + msg.size());
        j += "{\"ts\":\"";       j += esc(ts);
        j += "\",\"level\":\"";  j += esc(level);
        j += "\",\"thread\":\""; j += esc(thread);
        j += "\",\"file\":\"";   j += esc(file);
        j += "\",\"line\":";
        j += (lineno.empty() ? "0" : onlyDigits(lineno) ? lineno : "0");
        j += ",\"msg\":\"";      j += esc(msg);
        j += "\"}";
        return j;
    }

    // ── 辅助函数 ──────────────────────────────────────────────────────────────
    static const char* skipSpaces(const char* p, const char* end) {
        while (p < end && *p == ' ') ++p;
        return p;
    }
    static const char* nextSpace(const char* p, const char* end) {
        while (p < end && *p != ' ') ++p;
        return p;
    }
    static const char* findSeq(const char* p, const char* end,
                                const char* seq, size_t seqLen) {
        while (p + seqLen <= end) {
            if (memcmp(p, seq, seqLen) == 0) return p;
            ++p;
        }
        return nullptr;
    }
    static void trim(std::string& s) {
        size_t a = s.find_first_not_of(" \t\r");
        size_t b = s.find_last_not_of(" \t\r");
        if (a == std::string::npos) { s.clear(); return; }
        s = s.substr(a, b - a + 1);
    }
    static bool onlyDigits(const std::string& s) {
        for (char c : s) if (c < '0' || c > '9') return false;
        return !s.empty();
    }

    // JSON 字符串转义
    static std::string esc(const std::string& s) {
        std::string r;
        r.reserve(s.size() + 4);
        for (unsigned char c : s) {
            switch (c) {
                case '"':  r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n";  break;
                case '\r': r += "\\r";  break;
                case '\t': r += "\\t";  break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                        r += buf;
                    } else {
                        r += (char)c;
                    }
            }
        }
        return r;
    }
};
