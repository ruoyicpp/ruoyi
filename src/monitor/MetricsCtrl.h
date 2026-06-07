/**
 * @file MetricsCtrl.h
 * @brief Prometheus 指标端点控制器
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <sstream>

#include <drogon/HttpController.h>

#include "MetricsCollector.h"

class MetricsCtrl : public drogon::HttpController<MetricsCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MetricsCtrl::getMetrics, "/metrics", drogon::Get);
        ADD_METHOD_TO(MetricsCtrl::getStats, "/monitor/stats", drogon::Get);
    METHOD_LIST_END

    void getMetrics(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        (void)req;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);

        std::ostringstream body;
        body << "# HELP ruoyi_build_info Build information\n";
        body << "# TYPE ruoyi_build_info gauge\n";
        body << "ruoyi_build_info{version=\"1.0.0\"} 1\n\n";
        body << Metrics::Registry::instance().exportToPrometheus();

        resp->setBody(body.str());
        cb(resp);
    }

    void getStats(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        (void)req;
        Json::Value stats;
        stats["uptimeSeconds"] = static_cast<Json::Int64>(getUptime());
        stats["version"] = "1.0.0";
        stats["service"] = "ruoyi-cpp";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(stats);
        cb(resp);
    }

private:
    static int64_t getUptime() {
        static const auto start = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now() - start)
            .count();
    }
};
