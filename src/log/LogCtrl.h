#pragma once

#include <drogon/HttpController.h>

namespace Monitor {

class LogCtrl : public drogon::HttpController<LogCtrl> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LogCtrl::search, "/monitor/logs/search", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getStats, "/monitor/logs/stats", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getTrends, "/monitor/logs/trends", drogon::Get);
    ADD_METHOD_TO(LogCtrl::analyze, "/monitor/logs/analysis", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getCollectorStatus, "/monitor/logs/collector", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getHotFiles, "/monitor/logs/hot-files", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getHotMessages, "/monitor/logs/hot-messages", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getAnomalies, "/monitor/logs/anomalies", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getAlerts, "/monitor/logs/alerts", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getErrors, "/monitor/logs/errors", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getWarnings, "/monitor/logs/warnings", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getPerformance, "/monitor/logs/performance", drogon::Get);
    METHOD_LIST_END

    void search(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getStats(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getTrends(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void analyze(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getCollectorStatus(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getHotFiles(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getHotMessages(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getAnomalies(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getAlerts(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getErrors(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getWarnings(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getPerformance(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace Monitor
