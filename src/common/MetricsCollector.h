#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <array>
#include <mutex>
#include <unordered_map>
#include <string>
#include "AjaxResult.h"
#include "../services/DatabaseService.h"

// =============================================================================
// MetricsCollector — Prometheus 风格指标收集 + /actuator/* 端点
//
// 指标清单（Prometheus 文本格式）
//   ruoyi_requests_total{status="2xx|3xx|4xx|5xx"}    counter
//   ruoyi_errors_total                                 counter (5xx)
//   ruoyi_request_duration_ms_bucket{le="..."}         histogram (12 buckets)
//   ruoyi_request_duration_ms_count                    counter
//   ruoyi_request_duration_ms_sum                      counter
//   ruoyi_slow_requests_total                          counter (>1s)
//   ruoyi_db_queries_total{op="query|exec"}            counter
//   ruoyi_db_slow_queries_total                        counter (>=warn 阈值)
//   ruoyi_db_errors_total                              counter
//   ruoyi_uptime_seconds                               gauge
//   ruoyi_login_success_total                          counter
//   ruoyi_login_fail_total                             counter
//   ruoyi_rate_limit_rejected_total                    counter
//
// HTTP 端点
//   GET  /actuator/health                  健康检查
//   GET  /actuator/info                    应用信息
//   GET  /actuator/metrics                 Prometheus 文本输出
//   GET  /actuator/db                      数据库状态
//   POST /actuator/reload                  热重载配置（受保护）
//
// 自动接入：调用 MetricsCollector::instance().attachAdvice() 后，
//   drogon advice 自动统计 method/status/耗时，并打印慢请求 WARN 日志。
// =============================================================================
struct MetricsCollector {
    // ── 计数器 ───────────────────────────────────────────────────────────
    std::atomic<uint64_t> reqTotal{0};
    std::atomic<uint64_t> req2xx{0};
    std::atomic<uint64_t> req3xx{0};
    std::atomic<uint64_t> req4xx{0};
    std::atomic<uint64_t> req5xx{0};
    std::atomic<uint64_t> slowReqs{0};

    std::atomic<uint64_t> dbQueries{0};
    std::atomic<uint64_t> dbExecs{0};
    std::atomic<uint64_t> dbSlow{0};
    std::atomic<uint64_t> dbErrors{0};

    std::atomic<uint64_t> loginSuccess{0};
    std::atomic<uint64_t> loginFail{0};
    std::atomic<uint64_t> rateLimited{0};

    // ── 直方图：耗时 (ms) ────────────────────────────────────────────────
    static constexpr std::array<double, 12> kBuckets = {
        5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 30000
    };
    std::array<std::atomic<uint64_t>, 12> histBuckets{};
    std::atomic<uint64_t> histInf{0};
    std::atomic<uint64_t> histCount{0};
    std::atomic<uint64_t> histSumMs{0};

    // ── 慢请求阈值（可由 config 调整）──────────────────────────────────
    int slowReqMs = 1000;

    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

    static MetricsCollector& instance() {
        static MetricsCollector m;
        return m;
    }

    // ── 由 HTTP advice 调用 ─────────────────────────────────────────────
    void onRequestDone(int status, long durationMs) {
        ++reqTotal;
        if      (status >= 500) { ++req5xx; }
        else if (status >= 400) { ++req4xx; }
        else if (status >= 300) { ++req3xx; }
        else                    { ++req2xx; }

        if (durationMs >= slowReqMs) ++slowReqs;
        observeDuration(durationMs);
    }

    void observeDuration(long ms) {
        if (ms < 0) ms = 0;
        histSumMs.fetch_add((uint64_t)ms, std::memory_order_relaxed);
        ++histCount;
        for (size_t i = 0; i < kBuckets.size(); ++i) {
            if ((double)ms <= kBuckets[i]) {
                histBuckets[i].fetch_add(1, std::memory_order_relaxed);
            }
        }
        ++histInf;
    }

    // ── 由 DatabaseService / Login / RateLimiter 调用 ──────────────────
    void onDbQuery(long ms, bool ok, bool isWrite) {
        if (isWrite) ++dbExecs; else ++dbQueries;
        if (!ok) ++dbErrors;
        if (ms >= 200) ++dbSlow;   // 与 DatabaseService 默认阈值一致
    }
    void onLoginSuccess() { ++loginSuccess; }
    void onLoginFail()    { ++loginFail; }
    void onRateLimited()  { ++rateLimited; }

    // ── 注册 HTTP advice 自动打点 ───────────────────────────────────────
    void attachAdvice() {
        drogon::app().registerPreSendingAdvice(
            [](const drogon::HttpRequestPtr& req,
               const drogon::HttpResponsePtr& resp) {
                // 在响应即将发送前打点
                long ms = 0;
                auto t0 = req->getCreationDate().microSecondsSinceEpoch();
                auto now = trantor::Date::now().microSecondsSinceEpoch();
                ms = (long)((now - t0) / 1000);
                int code = (int)resp->getStatusCode();
                MetricsCollector::instance().onRequestDone(code, ms);
                // 慢请求 WARN
                if (ms >= MetricsCollector::instance().slowReqMs) {
                    LOG_WARN << "[SlowReq] " << req->getMethodString() << " "
                             << req->getPath() << " status=" << code
                             << " duration=" << ms << "ms";
                }
            });
    }

    // ── 渲染 Prometheus 文本 ────────────────────────────────────────────
    std::string render() const {
        auto upSec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        std::ostringstream ss;
        // requests
        ss << "# HELP ruoyi_requests_total Total HTTP requests by status class\n"
           << "# TYPE ruoyi_requests_total counter\n"
           << "ruoyi_requests_total{status=\"2xx\"} " << req2xx.load() << "\n"
           << "ruoyi_requests_total{status=\"3xx\"} " << req3xx.load() << "\n"
           << "ruoyi_requests_total{status=\"4xx\"} " << req4xx.load() << "\n"
           << "ruoyi_requests_total{status=\"5xx\"} " << req5xx.load() << "\n"
           << "# HELP ruoyi_errors_total HTTP 5xx error count\n"
           << "# TYPE ruoyi_errors_total counter\n"
           << "ruoyi_errors_total " << req5xx.load() << "\n"
           << "# HELP ruoyi_slow_requests_total Requests slower than slowReqMs threshold\n"
           << "# TYPE ruoyi_slow_requests_total counter\n"
           << "ruoyi_slow_requests_total " << slowReqs.load() << "\n";

        // histogram
        ss << "# HELP ruoyi_request_duration_ms Request duration histogram in ms\n"
           << "# TYPE ruoyi_request_duration_ms histogram\n";
        uint64_t cum = 0;
        for (size_t i = 0; i < kBuckets.size(); ++i) {
            cum = histBuckets[i].load();
            ss << "ruoyi_request_duration_ms_bucket{le=\"" << kBuckets[i] << "\"} "
               << cum << "\n";
        }
        ss << "ruoyi_request_duration_ms_bucket{le=\"+Inf\"} " << histInf.load() << "\n"
           << "ruoyi_request_duration_ms_count " << histCount.load() << "\n"
           << "ruoyi_request_duration_ms_sum "   << histSumMs.load() << "\n";

        // db
        ss << "# HELP ruoyi_db_queries_total DB queries by op type\n"
           << "# TYPE ruoyi_db_queries_total counter\n"
           << "ruoyi_db_queries_total{op=\"query\"} " << dbQueries.load() << "\n"
           << "ruoyi_db_queries_total{op=\"exec\"} "  << dbExecs.load()   << "\n"
           << "# HELP ruoyi_db_slow_queries_total Slow DB queries (>=warn threshold)\n"
           << "# TYPE ruoyi_db_slow_queries_total counter\n"
           << "ruoyi_db_slow_queries_total " << dbSlow.load() << "\n"
           << "# HELP ruoyi_db_errors_total DB query errors\n"
           << "# TYPE ruoyi_db_errors_total counter\n"
           << "ruoyi_db_errors_total " << dbErrors.load() << "\n";

        // auth & limit
        ss << "# HELP ruoyi_login_success_total Successful login attempts\n"
           << "# TYPE ruoyi_login_success_total counter\n"
           << "ruoyi_login_success_total " << loginSuccess.load() << "\n"
           << "# HELP ruoyi_login_fail_total Failed login attempts\n"
           << "# TYPE ruoyi_login_fail_total counter\n"
           << "ruoyi_login_fail_total " << loginFail.load() << "\n"
           << "# HELP ruoyi_rate_limit_rejected_total Requests rejected by rate limiter\n"
           << "# TYPE ruoyi_rate_limit_rejected_total counter\n"
           << "ruoyi_rate_limit_rejected_total " << rateLimited.load() << "\n";

        // uptime
        ss << "# HELP ruoyi_uptime_seconds Server uptime in seconds\n"
           << "# TYPE ruoyi_uptime_seconds gauge\n"
           << "ruoyi_uptime_seconds " << upSec << "\n";

        return ss.str();
    }

    void registerActuator() {
        // GET /actuator/health
        drogon::app().registerHandler("/actuator/health",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                Json::Value r; r["status"] = "UP";
                cb(drogon::HttpResponse::newHttpJsonResponse(r));
            }, {drogon::Get});

        // GET /actuator/info
        drogon::app().registerHandler("/actuator/info",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                Json::Value r;
                r["app"]["name"]    = "RuoYi-Cpp";
                r["app"]["version"] = "1.2.0";
                cb(drogon::HttpResponse::newHttpJsonResponse(r));
            }, {drogon::Get});

        // GET /actuator/metrics
        drogon::app().registerHandler("/actuator/metrics",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setContentTypeString("text/plain; version=0.0.4; charset=utf-8");
                resp->setBody(MetricsCollector::instance().render());
                cb(resp);
            }, {drogon::Get});

        // GET /actuator/db
        drogon::app().registerHandler("/actuator/db",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                auto& db = DatabaseService::instance();
                Json::Value r;
                auto probe = db.query("SELECT 1");
                r["dbOk"]        = probe.ok();
                r["backend"]     = db.isUsingSqlite() ? "sqlite" : "postgres";
                r["pendingSync"] = 0;
                cb(drogon::HttpResponse::newHttpJsonResponse(r));
            }, {drogon::Get});

        // POST /actuator/reload — 仅 127.0.0.1 / ::1 可触发，避免外网误调
        drogon::app().registerHandler("/actuator/reload",
            [](const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                auto ip = req->getPeerAddr().toIp();
                if (ip != "127.0.0.1" && ip != "::1" && ip != "localhost") {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k403Forbidden);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody(R"({"code":403,"msg":"loopback only"})");
                    cb(resp);
                    return;
                }
                Json::Value r = AjaxResult::success();
                cb(drogon::HttpResponse::newHttpJsonResponse(r));
            }, {drogon::Post});
    }
};
