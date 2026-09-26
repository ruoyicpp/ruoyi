/**
 * @file SlowLogQueue.h
 * @brief SQL 慢查询审计 — 异步落库队列 + 告警
 *
 * 功能概述：
 *   - 抓取：DatabaseService::logSlow 触发 DbMetricsHook::notifySlow → 入队
 *   - 落库：定时器批量 INSERT sys_slow_log（异步，不在 DB mutex 内执行）
 *   - 告警：超过 alert_ms 时给管理员发站内信（限频 1 条/分钟）
 *   - 背压：队列满丢最旧，不影响业务线程
 *
 * 为什么异步：logSlow 在 DatabaseService::mutex_ 内执行，
 *   若直接 execParams 写库会死锁（非递归锁）。必须入队后由外部定时器落库。
 *
 * 接线（main.cc）：
 *   DbMetricsHook::slowHook = [](const char* op, const std::string& sql, long ms){
 *       SlowLogQueue::instance().push(op, sql, ms);
 *   };
 *   drogon::app().getLoop()->runEvery(10.0, []{
 *       SlowLogQueue::instance().flush();
 *   });
 *
 * 配置项（config.json → database.slow_log）：
 *   - enabled: 总开关（默认 true）
 *   - alert_ms: 告警阈值（默认 2000，超过发站内信）
 *   - queue_capacity: 队列上限（默认 2000）
 */

#pragma once
#include <deque>
#include <mutex>
#include <string>
#include <atomic>
#include <chrono>
#include <ctime>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "../services/DatabaseService.h"
#include "../common/NotifyService.h"

/**
 * @class SlowLogQueue
 * @brief 慢查询日志异步队列单例
 */
class SlowLogQueue {
public:
    struct Config {
        bool enabled       = true;
        long alertMs       = 2000;
        int  queueCapacity = 2000;
    };

    static SlowLogQueue& instance() {
        static SlowLogQueue inst;
        return inst;
    }

    void init(const Config& cfg) {
        cfg_ = cfg;
        if (cfg_.enabled)
            LOG_INFO << "[SlowLog] enabled alert_ms=" << cfg_.alertMs;
    }

    /**
     * @brief 入队（在 DB mutex 内调用，必须 O(1) 不写库）
     */
    void push(const char* op, const std::string& sql, long ms) {
        if (!cfg_.enabled) return;
        std::lock_guard<std::mutex> lk(mu_);
        if ((int)queue_.size() >= cfg_.queueCapacity) {
            queue_.pop_front();
            dropped_++;
            return;
        }
        Entry e;
        e.ts  = std::time(nullptr);
        e.op  = op;
        e.sql = sql.size() > 2000 ? sql.substr(0, 2000) : sql;
        e.ms  = ms;
        queue_.push_back(std::move(e));
        total_++;
    }

    /// 批量落库（定时器调用，不在 DB mutex 内）
    void flush() {
        std::deque<Entry> batch;
        {
            std::lock_guard<std::mutex> lk(mu_);
            batch.swap(queue_);
        }
        if (batch.empty()) return;
        auto& db = DatabaseService::instance();
        int ok = 0;
        for (auto& e : batch) {
            if (db.execParams(
                    "INSERT INTO sys_slow_log(op,sql_text,cost_ms) VALUES($1,$2,$3)",
                    {e.op, e.sql, std::to_string(e.ms)})) ++ok;
            // 告警：超阈值发站内信（限频 1/min）
            if (e.ms >= cfg_.alertMs) alert(e);
        }
        flushed_ += ok;
        if (ok < (int)batch.size())
            LOG_WARN << "[SlowLog] flush partial: " << ok << "/" << batch.size();
    }

    uint64_t total()   const { return total_; }
    uint64_t flushed() const { return flushed_; }
    uint64_t dropped() const { return dropped_; }
    size_t   pending() const {
        std::lock_guard<std::mutex> lk(mu_);
        return queue_.size();
    }

private:
    SlowLogQueue() = default;

    struct Entry {
        std::time_t ts;
        std::string op;
        std::string sql;
        long        ms;
    };

    /// 慢查询告警（限频 1 条/分钟，防刷屏）
    void alert(const Entry& e) {
        auto now = std::chrono::steady_clock::now();
        if (now - lastAlert_ < std::chrono::minutes(1)) return;
        lastAlert_ = now;
        NotifyService::sendInbox(1,   // 管理员
            "慢SQL告警",
            "[" + e.op + "] 耗时 " + std::to_string(e.ms) + "ms\n" +
                e.sql.substr(0, 300),
            "danger");
    }

    Config cfg_;
    std::deque<Entry> queue_;
    mutable std::mutex mu_;
    std::atomic<uint64_t> total_{0};
    std::atomic<uint64_t> flushed_{0};
    std::atomic<uint64_t> dropped_{0};
    std::chrono::steady_clock::time_point lastAlert_{};
};
