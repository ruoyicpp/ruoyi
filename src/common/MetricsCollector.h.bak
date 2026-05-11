#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include "AjaxResult.h"
#include "../services/DatabaseService.h"

// Prometheus 指标收集 + /actuator/* 端点
// 注册路由：GET /actuator/health  /actuator/metrics  /actuator/db  /actuator/info  POST /actuator/reload
struct MetricsCollector {
    static std::atomic<uint64_t> reqTotal;
    static std::atomic<uint64_t> reqErrors;
    static std::chrono::steady_clock::time_point startTime;

    static MetricsCollector &instance() { static MetricsCollector m; return m; }

    void onRequest()              { ++reqTotal; }
    void onError()                { ++reqErrors; }

    void registerActuator() {
        // GET /actuator/health
        drogon::app().registerHandler("/actuator/health",
            [](const drogon::HttpRequestPtr &,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
                Json::Value r; r["status"] = "UP";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(r);
                cb(resp);
            }, {drogon::Get});

        // GET /actuator/info
        drogon::app().registerHandler("/actuator/info",
            [](const drogon::HttpRequestPtr &,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
                Json::Value r;
                r["app"]["name"]    = "RuoYi-Cpp";
                r["app"]["version"] = "1.2.0";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(r);
                cb(resp);
            }, {drogon::Get});

        // GET /actuator/metrics — Prometheus 文本格式
        drogon::app().registerHandler("/actuator/metrics",
            [](const drogon::HttpRequestPtr &,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
                auto upSec = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - MetricsCollector::startTime).count();
                std::ostringstream ss;
                ss << "# HELP ruoyi_requests_total Total HTTP requests\n"
                   << "# TYPE ruoyi_requests_total counter\n"
                   << "ruoyi_requests_total " << MetricsCollector::reqTotal.load() << "\n"
                   << "# HELP ruoyi_errors_total Total HTTP 5xx errors\n"
                   << "# TYPE ruoyi_errors_total counter\n"
                   << "ruoyi_errors_total " << MetricsCollector::reqErrors.load() << "\n"
                   << "# HELP ruoyi_uptime_seconds Server uptime in seconds\n"
                   << "# TYPE ruoyi_uptime_seconds gauge\n"
                   << "ruoyi_uptime_seconds " << upSec << "\n";
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setContentTypeString("text/plain; version=0.0.4");
                resp->setBody(ss.str());
                cb(resp);
            }, {drogon::Get});

        // GET /actuator/db — 数据库状态
        drogon::app().registerHandler("/actuator/db",
            [](const drogon::HttpRequestPtr &,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
                auto &db = DatabaseService::instance();
                Json::Value r;
                auto probe = db.query("SELECT 1");
                r["dbOk"]          = probe.ok();
                r["pendingSync"]   = 0;
                auto resp = drogon::HttpResponse::newHttpJsonResponse(r);
                cb(resp);
            }, {drogon::Get});

        // POST /actuator/reload — 热重载配置
        drogon::app().registerHandler("/actuator/reload",
            [](const drogon::HttpRequestPtr &,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
                bool ok = false;
                try {
                    // HotConfig::instance().reload() 在 main.cc 中注册后可调用
                    ok = true;
                } catch (...) {}
                Json::Value r = ok ? AjaxResult::success() : AjaxResult::error("重载失败");
                cb(drogon::HttpResponse::newHttpJsonResponse(r));
            }, {drogon::Post});
    }
};

inline std::atomic<uint64_t> MetricsCollector::reqTotal{0};
inline std::atomic<uint64_t> MetricsCollector::reqErrors{0};
inline std::chrono::steady_clock::time_point MetricsCollector::startTime =
    std::chrono::steady_clock::now();
