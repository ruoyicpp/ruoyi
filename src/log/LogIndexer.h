/**
 * @file LogIndexer.h
 * @brief 系统日志文件 → Manticore 全文索引器
 *
 * 功能概述：
 *   - 增量 tail：周期扫描 ./logs/*.log|*.jsonl，按文件偏移量只读新增行
 *   - 轮转感知：文件变小（轮替/截断）时重置偏移并清除该文件旧文档
 *   - 行解析：drogon 标准日志格式 `yyyymmdd hh:mm:ss.usec TZ pid LEVEL [tag] msg - file.cc:line`
 *   - 批量上报：攒批经 ManticoreClient::bulkInsert 写入 RT 索引
 *   - 优雅降级：Manticore 不可用时静默跳过（本地文件仍是权威副本）
 *
 * 表结构（sys_logs，RT 索引自动建表）：
 *   CREATE TABLE sys_logs (
 *     ts bigint, file string, level string, line_no int, msg text
 *   );
 *   - ts：日志行时间戳（解析失败回退为写入时间）
 *   - file：文件名（不含路径）
 *   - level：DEBUG/INFO/WARN/ERROR/FATAL/RAW（未识别）
 *   - line_no：当日文件内行号（辅助定位）
 *   - msg：完整原始行（全文检索目标）
 *
 * 查询端点：GET /monitor/logfile/search?q=..&level=..&file=..&limit=N
 *
 * 配置项（config.json → log.manticore）：
 *   - enabled:  是否启用（默认 false，需要 Manticore searchd 进程）
 *   - endpoint: Manticore HTTP 地址（默认 http://127.0.0.1:7700，
 *               留空时回退 security.audit.endpoint）
 *   - index:    索引名（默认 sys_logs）
 *   - log_dir:  日志目录（默认 ./logs）
 *   - batch_size: 单次 bulk 上限（默认 500）
 *   - max_lines_per_tick: 每 tick 单文件最多解析行数（默认 20000）
 *
 * 使用：
 *   LogIndexer::instance().init(cfg);          // 启动时
 *   drogon::app().getLoop()->runEvery(3.0, []{ LogIndexer::instance().tick(); });
 *   LogIndexer::instance().configure(cfg);     // 热重载时（ConfigReloader）
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <ctime>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "../audit/ManticoreClient.h"

class LogIndexer {
public:
    struct Config {
        bool        enabled          = false;
        std::string endpoint         = "http://127.0.0.1:7700";
        std::string index            = "sys_logs";
        std::string logDir           = "./logs";
        int         batchSize        = 500;
        int         maxLinesPerTick  = 20000;
    };

    static LogIndexer& instance() { static LogIndexer inst; return inst; }

    /// 启动时初始化（建表 + 记录配置）
    void init(const Config& cfg) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            cfg_ = cfg;
        }
        if (!cfg.enabled) { LOG_INFO << "[LogIndexer] disabled"; return; }
        ManticoreClient::sql(cfg.endpoint, createTableSql(cfg.index),
            [](bool ok, int status, const std::string&) {
                if (ok && status == 200)
                    LOG_INFO << "[LogIndexer] manticore index ready";
                else
                    LOG_WARN << "[LogIndexer] manticore unreachable, "
                                "log indexing paused until it recovers";
            });
        LOG_INFO << "[LogIndexer] enabled -> " << cfg.endpoint
                 << " index=" << cfg.index << " dir=" << cfg.logDir;
    }

    /// 热重载（ConfigReloader 调用）；enabled 切换时顺带清偏移重扫
    void configure(const Config& cfg) {
        Config old;
        {
            std::lock_guard<std::mutex> lk(mu_);
            old = cfg_;
            cfg_ = cfg;
        }
        if (!old.enabled && cfg.enabled) init(cfg);   // 关→开：重新 init 建表
        if (old.index != cfg.index || old.logDir != cfg.logDir) {
            std::lock_guard<std::mutex> lk(mu_);
            offsets_.clear();
        }
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lk(mu_);
        return cfg_.enabled;
    }

    /// 周期 tick：扫描目录、读新增行、批量上报
    void tick() {
        Config c;
        {
            std::lock_guard<std::mutex> lk(mu_);
            c = cfg_;
        }
        if (!c.enabled) return;

        namespace fs = std::filesystem;
        std::error_code ec;
        if (!fs::exists(c.logDir, ec)) return;

        for (auto& entry : fs::directory_iterator(c.logDir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".log" && ext != ".jsonl") continue;
            indexFile(entry.path(), c);
        }
    }

    static std::string createTableSql(const std::string& index) {
        return "CREATE TABLE IF NOT EXISTS " + index + " ("
               "ts bigint, file string, level string, line_no int, msg text)";
    }

    /// 供查询端点使用：当前 endpoint/index（未启用返回 false）
    bool queryTarget(std::string& endpoint, std::string& index) const {
        std::lock_guard<std::mutex> lk(mu_);
        endpoint = cfg_.endpoint;
        index    = cfg_.index;
        return cfg_.enabled;
    }

private:
    LogIndexer() = default;

    /// 解析单行：drogon 格式 "20260925 10:47:38.187011 UTC 252814 WARN [tag] msg - file.cc:14"
    /// 解析失败时 ts=now, level=RAW
    static void parseLine(const std::string& line, int64_t& ts, std::string& level) {
        ts = std::time(nullptr);
        level = "RAW";
        // 形如 yyyymmdd(8位) + 空格 + hh:mm:ss(8位)
        if (line.size() < 17) return;
        auto isDig = [](char c){ return c >= '0' && c <= '9'; };
        bool head = true;
        for (int i = 0; i < 8; ++i) if (!isDig(line[i])) { head = false; break; }
        if (!head || line[8] != ' ') return;
        // hh:mm:ss
        if (!(isDig(line[9]) && isDig(line[10]) && line[11] == ':' &&
              isDig(line[12]) && isDig(line[13]) && line[14] == ':' &&
              isDig(line[15]) && isDig(line[16]))) return;
        std::tm tm{};
        tm.tm_year = std::stoi(line.substr(0,4)) - 1900;
        tm.tm_mon  = std::stoi(line.substr(4,2)) - 1;
        tm.tm_mday = std::stoi(line.substr(6,2));
        tm.tm_hour = std::stoi(line.substr(9,2));
        tm.tm_min  = std::stoi(line.substr(12,2));
        tm.tm_sec  = std::stoi(line.substr(15,2));
        // drogon 时间戳是 UTC，mktime 是按本地时区解释的 → 用 timegm 修正
#ifdef _WIN32
        ts = _mkgmtime(&tm);
#else
        ts = timegm(&tm);
#endif
        // 级别：时间戳后找第一个全大写词（跳过 TZ 如 UTC、pid 数字）
        size_t p = line.find(' ', 17);
        while (p != std::string::npos && p + 1 < line.size()) {
            size_t s = p + 1, e = line.find(' ', s);
            std::string w = line.substr(s, e == std::string::npos ? e : e - s);
            if (w == "TRACE" || w == "DEBUG" || w == "INFO" ||
                w == "WARN"  || w == "ERROR" || w == "FATAL") { level = w; return; }
            p = e;
        }
    }

    /// 增量读取一个文件并 bulk 上报（失败静默）
    void indexFile(const std::filesystem::path& path, const Config& c) {
        std::string fileName = path.filename().string();
        std::error_code ec;
        auto fsize = std::filesystem::file_size(path, ec);
        if (ec) return;

        uint64_t offset;
        {
            std::lock_guard<std::mutex> lk(mu_);
            offset = offsets_[fileName];
            if (fsize < offset) {
                // 轮替/截断 → 清该文件旧文档 + 重置偏移
                offset = 0;
                std::string fn = fileName;
                for (size_t p = 0; (p = fn.find('\'', p)) != std::string::npos; p += 2)
                    fn.insert(p, "''");
                ManticoreClient::sql(c.endpoint,
                    "DELETE FROM " + c.index + " WHERE file='" + fn + "'");
            }
        }
        if (fsize == offset) return;

        std::ifstream in(path, std::ios::binary);
        if (!in) return;
        in.seekg((std::streamoff)offset);

        std::vector<Json::Value> batch;
        batch.reserve((size_t)c.batchSize);
        std::string line;
        int lines = 0;
        int64_t checkpoint = (int64_t)offset;   // 已成功入队发送的最后位置
        while (lines < c.maxLinesPerTick && std::getline(in, line)) {
            auto pos = in.tellg();
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            ++lines;

            int64_t ts; std::string level;
            parseLine(line, ts, level);
            Json::Value doc;
            doc["ts"]      = (Json::Int64)ts;
            doc["file"]    = fileName;
            doc["level"]   = level;
            doc["line_no"] = (int)pos;          // 行起始偏移（跨轮替唯一，辅助定位）
            doc["msg"]     = line;
            batch.push_back(std::move(doc));

            if ((int)batch.size() >= c.batchSize) {
                if (!sendBatch(c, batch)) { batch.clear(); break; }  // 并发上限，下 tick 从 checkpoint 重试
                batch.clear();
                checkpoint = (int64_t)pos;
            }
        }
        if (!batch.empty()) {
            if (sendBatch(c, batch)) checkpoint = (int64_t)fsize;
        } else if (lines > 0 && in.eof()) {
            checkpoint = (int64_t)fsize;         // 全部行都已入队
        }

        {
            std::lock_guard<std::mutex> lk(mu_);
            if ((uint64_t)checkpoint > offsets_[fileName])
                offsets_[fileName] = (uint64_t)checkpoint;
        }
    }

    /// 发送一批；并发满返回 false（调用方保留 offset 重试，不丢行）
    bool sendBatch(const Config& c, const std::vector<Json::Value>& batch) {
        if (batch.empty()) return true;
        if (inFlight_ >= 4) return false;   // 最多 4 个并发 bulk，防止压垮 searchd
        inFlight_++;
        ManticoreClient::bulkInsert(c.endpoint, c.index, batch,
            [this, n = batch.size()](bool ok, int status, const std::string&) {
                inFlight_--;
                if (!(ok && status == 200))
                    LOG_DEBUG << "[LogIndexer] bulk insert failed, " << n << " lines dropped";
            });
        return true;
    }

    Config      cfg_;
    std::unordered_map<std::string, uint64_t> offsets_;   ///< file → 已入队消费字节偏移
    mutable std::mutex mu_;
    std::atomic<int>    inFlight_{0};
};
