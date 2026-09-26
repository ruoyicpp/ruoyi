/**
 * @file AuditQueue.h
 * @brief 安全审计异步批量上报队列 — WAF/风控事件 → Manticore
 *
 * 功能概述：
 *   - 异步批量：事件先入内存队列，攒够 batch_size 或到 flush 间隔后批量 POST /bulk
 *   - 背压保护：队列满时丢弃最旧事件并计数，不阻塞请求线程
 *   - 失败重试：上报失败的批次保留重试（最多 retry_max 次，超限丢弃）
 *   - 优雅降级：Manticore 不可用时仅写本地 NDJSON，不影响业务
 *   - 定时清理：按 retention_days 定期 DELETE 过期文档
 *
 * 事件来源：
 *   - WafEngine::logHit — WAF 拦截/命中事件
 *   - RateLimiter 封禁事件（可接入）
 *   - AI 风控事件（预留 event_type 字段）
 *
 * 使用示例：
 *   AuditQueue::instance().init(cfg);           // main.cc 启动时
 *   AuditQueue::instance().enqueue(doc);        // 任意线程上报
 *   AuditQueue::instance().flush();             // 定时器周期触发
 *
 * 配置项（config.json → security.audit）：
 *   - enabled / endpoint / index / batch_size / flush_interval_ms
 *   - retention_days: 留存天数（默认 30）
 *   - queue_capacity: 内存队列上限（默认 10000，超出丢弃最旧）
 *   - retry_max: 单批次最大重试次数（默认 3）
 */

#pragma once
#include <deque>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <ctime>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "ManticoreClient.h"

/**
 * @class AuditQueue
 * @brief 审计事件批量上报单例
 *
 * 线程安全：enqueue 可在任意线程调用（含 drogon IO 线程）。
 * flush 由主循环定时器驱动，内部异步发送不阻塞。
 */
class AuditQueue {
public:
    struct Config {
        bool        enabled         = false;
        std::string endpoint        = "http://127.0.0.1:7700";
        std::string index           = "waf_logs";
        int         batchSize       = 100;
        int         flushIntervalMs = 3000;
        int         retentionDays   = 30;
        int         queueCapacity   = 10000;
        int         retryMax        = 3;
    };

    static AuditQueue& instance() {
        static AuditQueue inst;
        return inst;
    }

    /**
     * @brief 初始化并建表
     * @param cfg 配置
     * @note 仅 Linux 版本启用；其他平台直接跳过
     */
    void init(const Config& cfg) {
#ifndef __linux__
        (void)cfg;
        LOG_INFO << "[Audit] skipped: feature only available on Linux build";
        return;
#endif
        cfg_ = cfg;
        if (!cfg_.enabled) { LOG_INFO << "[Audit] disabled"; return; }

        // 异步建表（Manticore 未启动时静默失败，不影响启动）
        ManticoreClient::sql(cfg_.endpoint,
            ManticoreClient::createTableSql(cfg_.index),
            [](bool ok, int status, const std::string&) {
                if (ok && status == 200)
                    LOG_INFO << "[Audit] manticore index ready";
                else
                    LOG_WARN << "[Audit] manticore unreachable, "
                                "events will be dropped until it recovers";
            });
        LOG_INFO << "[Audit] enabled -> " << cfg_.endpoint
                 << " index=" << cfg_.index
                 << " batch=" << cfg_.batchSize
                 << " retention=" << cfg_.retentionDays << "d";
    }

    /**
     * @brief 入队一条审计事件（线程安全，O(1)）
     * @param doc JSON 文档（字段对应表结构）
     */
    void enqueue(Json::Value doc) {
        if (!cfg_.enabled) return;
        std::lock_guard<std::mutex> lk(mu_);
        if ((int)queue_.size() >= cfg_.queueCapacity) {
            queue_.pop_front();          // 背压：丢最旧
            dropped_++;
            return;
        }
        queue_.push_back(std::move(doc));
        if ((int)queue_.size() >= cfg_.batchSize) flushLocked();
    }

    /**
     * @brief 强制刷新（定时器调用）
     */
    void flush() {
        if (!cfg_.enabled) return;
        std::lock_guard<std::mutex> lk(mu_);
        flushLocked();
    }

    /**
     * @brief 清理过期日志（每日定时任务调用）
     */
    void cleanupExpired() {
        if (!cfg_.enabled) return;
        int64_t cutoff = std::time(nullptr) -
                         (int64_t)cfg_.retentionDays * 86400;
        ManticoreClient::sql(cfg_.endpoint,
            ManticoreClient::cleanupSql(cfg_.index, cutoff),
            [](bool ok, int status, const std::string& body) {
                if (ok && status == 200)
                    LOG_INFO << "[Audit] expired logs cleaned";
                else
                    LOG_WARN << "[Audit] cleanup failed: " << body;
            });
    }

    /// 统计：已上报 / 已丢弃 / 当前队列深度
    uint64_t reported() const { return reported_; }
    uint64_t dropped()  const { return dropped_; }
    size_t   pending()  const {
        std::lock_guard<std::mutex> lk(mu_);
        return queue_.size();
    }
    bool isEnabled() const { return cfg_.enabled; }
    const std::string& endpoint() const { return cfg_.endpoint; }
    const std::string& index()    const { return cfg_.index; }

private:
    AuditQueue() = default;

    /// 取出一批发送（调用方须持锁）
    void flushLocked() {
        if (queue_.empty() || inFlight_) return;
        size_t n = std::min((size_t)cfg_.batchSize, queue_.size());
        std::vector<Json::Value> batch(queue_.begin(), queue_.begin() + n);
        queue_.erase(queue_.begin(), queue_.begin() + n);
        inFlight_ = true;

        std::string ep = cfg_.endpoint, idx = cfg_.index;
        ManticoreClient::bulkInsert(ep, idx, batch,
            [this, n](bool ok, int status, const std::string&) {
                inFlight_ = false;
                if (ok && status == 200) {
                    reported_ += n;
                } else {
                    dropped_ += n;   // 失败批次丢弃（本地 NDJSON 仍有记录）
                    LOG_WARN << "[Audit] bulk insert failed, "
                             << n << " events dropped";
                }
            });
    }

    Config              cfg_;
    std::deque<Json::Value> queue_;
    mutable std::mutex  mu_;
    std::atomic<bool>   inFlight_{false};
    std::atomic<uint64_t> reported_{0};
    std::atomic<uint64_t> dropped_{0};
};
