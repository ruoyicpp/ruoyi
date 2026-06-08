#include "LogCtrl.h"

#include "LogAnalyzer.h"
#include "LogCollector.h"
#include "LogSearchEngine.h"

#include <drogon/HttpResponse.h>
#include <json/json.h>

#include <algorithm>
#include <string>

namespace Monitor {
namespace {

int parseIntParam(const std::string& value, int fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

std::int64_t parseInt64Param(const std::string& value, std::int64_t fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        return std::stoll(value);
    } catch (...) {
        return fallback;
    }
}

drogon::HttpResponsePtr makeJsonResponse(const Json::Value& body,
                                         drogon::HttpStatusCode status = drogon::k200OK) {
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
}

Json::Value recordToJson(const Log::LogRecord& record) {
    Json::Value json;
    json["timestamp"] = Json::Int64(record.timestamp);
    json["timestampText"] = record.timestampText;
    json["level"] = record.level;
    json["message"] = record.message;
    json["sourceFile"] = record.sourceFile;
    json["sourcePath"] = record.sourcePath;
    json["sourceName"] = record.sourceName;
    json["thread"] = record.thread;
    json["logger"] = record.logger;
    json["rawLine"] = record.rawLine;
    json["lineNumber"] = Json::UInt64(record.lineNumber);
    return json;
}

Json::Value resultToJson(const Log::LogSearchResult& result) {
    Json::Value body;
    body["totalMatches"] = Json::UInt64(result.totalMatches);
    body["scannedFiles"] = Json::UInt64(result.scannedFiles);
    body["scannedLines"] = Json::UInt64(result.scannedLines);
    body["elapsedMs"] = Json::UInt64(result.elapsedMs);
    body["records"] = Json::arrayValue;
    for (const auto& record : result.records) {
        body["records"].append(recordToJson(record));
    }
    return body;
}

Json::Value hotspotToJson(const Log::LogHotspot& hotspot) {
    Json::Value json;
    json["key"] = hotspot.key;
    json["count"] = Json::UInt64(hotspot.count);
    return json;
}

Json::Value collectorStatusToJson(const Log::LogCollectorStatus& status) {
    Json::Value json;
    json["running"] = status.running;
    json["enabled"] = status.enabled;
    json["primaryPath"] = status.primaryPath;
    json["primaryPathExists"] = status.primaryPathExists;
    json["primaryPathDirectory"] = status.primaryPathDirectory;
    json["totalDiscoveredFiles"] = Json::UInt64(status.totalDiscoveredFiles);
    json["totalEnabledSources"] = Json::UInt64(status.totalEnabledSources);
    json["sources"] = Json::arrayValue;

    for (const auto& source : status.sources) {
        Json::Value sourceJson;
        sourceJson["name"] = source.name;
        sourceJson["path"] = source.path;
        sourceJson["enabled"] = source.enabled;
        sourceJson["exists"] = source.exists;
        sourceJson["directory"] = source.directory;
        sourceJson["fileCount"] = Json::UInt64(source.fileCount);
        json["sources"].append(sourceJson);
    }

    return json;
}

Json::Value anomaliesToJson(const Log::LogAnomalySummary& anomalies) {
    Json::Value json;
    json["errorSpikes"] = Json::UInt64(anomalies.errorSpikes);
    json["unknownLevelLines"] = Json::UInt64(anomalies.unknownLevelLines);
    json["parseFailures"] = Json::UInt64(anomalies.parseFailures);
    return json;
}

Json::Value alertToJson(const Log::LogAlert& alert) {
    Json::Value json;
    json["ruleId"] = alert.ruleId;
    json["level"] = alert.level;
    json["title"] = alert.title;
    json["summary"] = alert.summary;
    json["value"] = alert.value;
    json["threshold"] = alert.threshold;
    return json;
}

Log::LogQuery buildQuery(const drogon::HttpRequestPtr& req) {
    Log::LogQuery query;
    query.keyword = req->getParameter("keyword");
    query.level = req->getParameter("level");
    query.sourceFile = req->getParameter("file");
    query.limit = parseIntParam(req->getParameter("limit"), 100);
    query.offset = parseIntParam(req->getParameter("offset"), 0);
    query.timeRange.start = parseInt64Param(req->getParameter("start"), 0);
    query.timeRange.end = parseInt64Param(req->getParameter("end"), 0);
    return query;
}

} // namespace

void LogCtrl::search(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto query = buildQuery(req);
    auto body = resultToJson(Log::LogSearchEngine::instance().search(query));
    body["collectorRunning"] = Log::LogCollector::instance().isRunning();
    callback(makeJsonResponse(body));
}

void LogCtrl::getStats(const drogon::HttpRequestPtr&,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto stats = Log::LogAnalyzer::instance().getStats();
    Json::Value body;
    body["totalFiles"] = Json::UInt64(stats.totalFiles);
    body["totalLines"] = Json::UInt64(stats.totalLines);
    body["collectorRunning"] = Log::LogCollector::instance().isRunning();
    body["levels"] = Json::objectValue;
    for (const auto& [level, count] : stats.levelCounts) {
        body["levels"][level] = Json::UInt64(count);
    }
    body["files"] = Json::objectValue;
    for (const auto& [file, count] : stats.fileCounts) {
        body["files"][file] = Json::UInt64(count);
    }
    body["sources"] = Json::objectValue;
    for (const auto& [source, count] : stats.sourceCounts) {
        body["sources"][source] = Json::UInt64(count);
    }
    callback(makeJsonResponse(body));
}

void LogCtrl::getTrends(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    Log::TimeRange range;
    range.start = parseInt64Param(req->getParameter("start"), 0);
    range.end = parseInt64Param(req->getParameter("end"), 0);
    const auto bucketSeconds = std::max(parseInt64Param(req->getParameter("bucket"), 3600), static_cast<std::int64_t>(1));

    const auto trends = Log::LogAnalyzer::instance().getTrends(range, bucketSeconds);
    Json::Value body;
    body["bucketSeconds"] = Json::Int64(bucketSeconds);
    body["points"] = Json::arrayValue;
    for (const auto& point : trends) {
        Json::Value item;
        item["bucketStart"] = Json::Int64(point.bucketStart);
        item["total"] = Json::UInt64(point.total);
        item["errors"] = Json::UInt64(point.errors);
        item["warnings"] = Json::UInt64(point.warnings);
        body["points"].append(item);
    }
    callback(makeJsonResponse(body));
}

void LogCtrl::analyze(const drogon::HttpRequestPtr&,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto analysis = Log::LogAnalyzer::instance().analyze();
    Json::Value body;
    body["dominantLevel"] = analysis.dominantLevel;
    body["hottestFile"] = analysis.hottestFile;
    body["errorRate"] = analysis.errorRate;
    body["totalFiles"] = Json::UInt64(analysis.totalFiles);
    body["totalLines"] = Json::UInt64(analysis.totalLines);
    body["hottestFiles"] = Json::arrayValue;
    for (const auto& hotspot : analysis.hottestFiles) {
        body["hottestFiles"].append(hotspotToJson(hotspot));
    }
    body["hottestMessages"] = Json::arrayValue;
    for (const auto& hotspot : analysis.hottestMessages) {
        body["hottestMessages"].append(hotspotToJson(hotspot));
    }
    body["anomalies"] = anomaliesToJson(analysis.anomalies);
    callback(makeJsonResponse(body));
}

void LogCtrl::getCollectorStatus(const drogon::HttpRequestPtr&,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    callback(makeJsonResponse(collectorStatusToJson(Log::LogAnalyzer::instance().getCollectorStatus())));
}

void LogCtrl::getHotFiles(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto limit = std::max(parseIntParam(req->getParameter("limit"), 10), 0);
    Json::Value body;
    body["items"] = Json::arrayValue;
    for (const auto& hotspot : Log::LogAnalyzer::instance().getHotFiles(static_cast<std::size_t>(limit))) {
        body["items"].append(hotspotToJson(hotspot));
    }
    callback(makeJsonResponse(body));
}

void LogCtrl::getHotMessages(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto limit = std::max(parseIntParam(req->getParameter("limit"), 10), 0);
    Json::Value body;
    body["items"] = Json::arrayValue;
    for (const auto& hotspot : Log::LogAnalyzer::instance().getHotMessages(static_cast<std::size_t>(limit))) {
        body["items"].append(hotspotToJson(hotspot));
    }
    callback(makeJsonResponse(body));
}

void LogCtrl::getAnomalies(const drogon::HttpRequestPtr&,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    callback(makeJsonResponse(anomaliesToJson(Log::LogAnalyzer::instance().getAnomalies())));
}

void LogCtrl::getAlerts(const drogon::HttpRequestPtr&,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto alerts = Log::LogAnalyzer::instance().getAlerts();
    Json::Value body;
    body["hasAlerts"] = alerts.hasAlerts;
    body["totalAlerts"] = Json::UInt64(alerts.totalAlerts);
    body["alerts"] = Json::arrayValue;
    for (const auto& alert : alerts.alerts) {
        body["alerts"].append(alertToJson(alert));
    }
    callback(makeJsonResponse(body));
}

void LogCtrl::getErrors(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    callback(makeJsonResponse(resultToJson(
        Log::LogAnalyzer::instance().getErrors(parseIntParam(req->getParameter("limit"), 100)))));
}

void LogCtrl::getWarnings(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    callback(makeJsonResponse(resultToJson(
        Log::LogAnalyzer::instance().getWarnings(parseIntParam(req->getParameter("limit"), 100)))));
}

void LogCtrl::getPerformance(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    callback(makeJsonResponse(resultToJson(
        Log::LogAnalyzer::instance().getPerformance(parseIntParam(req->getParameter("limit"), 100)))));
}

} // namespace Monitor
