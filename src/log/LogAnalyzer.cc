#include "LogAnalyzer.h"

#include "LogSearchEngine.h"

#include <algorithm>
#include <map>
#include <utility>

namespace Log {
namespace {

std::vector<LogRecord> collectAllRecords() {
    LogQuery query;
    query.limit = LogSearchEngine::instance().config().maxResults;
    return LogSearchEngine::instance().search(query).records;
}

std::vector<LogHotspot> topHotspots(const std::map<std::string, std::size_t>& counts,
                                    std::size_t limit) {
    std::vector<LogHotspot> hotspots;
    hotspots.reserve(counts.size());
    for (const auto& [key, count] : counts) {
        if (key.empty()) {
            continue;
        }
        hotspots.push_back({key, count});
    }

    std::sort(hotspots.begin(), hotspots.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.count != rhs.count) {
            return lhs.count > rhs.count;
        }
        return lhs.key < rhs.key;
    });

    if (hotspots.size() > limit) {
        hotspots.resize(limit);
    }
    return hotspots;
}

void appendAlert(LogAlertSummary& summary,
                 const std::string& ruleId,
                 const std::string& level,
                 const std::string& title,
                 const std::string& message,
                 double value,
                 double threshold) {
    summary.alerts.push_back({ruleId, level, title, message, value, threshold});
}

} // namespace

LogAnalyzer& LogAnalyzer::instance() {
    static LogAnalyzer analyzer;
    return analyzer;
}

void LogAnalyzer::init(const LogConfig& config) {
    LogCollector::instance().init(config);
    LogSearchEngine::instance().init(config);
}

LogStats LogAnalyzer::getStats() const {
    LogStats stats;
    const auto files = LogSearchEngine::instance().discoverLogFiles();
    stats.totalFiles = files.size();

    LogQuery query;
    query.limit = LogSearchEngine::instance().config().maxResults;
    const auto result = LogSearchEngine::instance().search(query);
    stats.totalLines = result.scannedLines;

    for (const auto& record : result.records) {
        ++stats.levelCounts[record.level];
        ++stats.fileCounts[record.sourceFile];
        ++stats.sourceCounts[record.sourceName];
    }

    return stats;
}

std::vector<LogTrendPoint> LogAnalyzer::getTrends(const TimeRange& range,
                                                  std::int64_t bucketSeconds) const {
    std::vector<LogTrendPoint> trends;
    if (bucketSeconds <= 0) {
        bucketSeconds = 3600;
    }

    LogQuery query;
    query.timeRange = range;
    query.limit = LogSearchEngine::instance().config().maxResults;
    const auto result = LogSearchEngine::instance().search(query);

    std::map<std::int64_t, LogTrendPoint> buckets;
    for (const auto& record : result.records) {
        const auto ts = record.timestamp > 0 ? record.timestamp : 0;
        const auto bucketStart = ts > 0 ? (ts / bucketSeconds) * bucketSeconds : 0;
        auto& bucket = buckets[bucketStart];
        bucket.bucketStart = bucketStart;
        ++bucket.total;
        if (record.level == "ERROR" || record.level == "FATAL") {
            ++bucket.errors;
        }
        if (record.level == "WARN") {
            ++bucket.warnings;
        }
    }

    for (const auto& [_, bucket] : buckets) {
        trends.push_back(bucket);
    }
    return trends;
}

LogAnalysis LogAnalyzer::analyze() const {
    LogAnalysis analysis;
    const auto stats = getStats();
    analysis.totalFiles = stats.totalFiles;
    analysis.totalLines = stats.totalLines;
    analysis.hottestFiles = getHotFiles();
    analysis.hottestMessages = getHotMessages();
    analysis.anomalies = getAnomalies();

    std::size_t maxLevelCount = 0;
    for (const auto& [level, count] : stats.levelCounts) {
        if (count > maxLevelCount) {
            maxLevelCount = count;
            analysis.dominantLevel = level;
        }
    }

    std::size_t maxFileCount = 0;
    for (const auto& [file, count] : stats.fileCounts) {
        if (count > maxFileCount) {
            maxFileCount = count;
            analysis.hottestFile = file;
        }
    }

    const auto errorCount = stats.levelCounts.count("ERROR") ? stats.levelCounts.at("ERROR") : 0;
    analysis.errorRate = stats.totalLines == 0 ? 0.0 : static_cast<double>(errorCount) / stats.totalLines;
    return analysis;
}

LogCollectorStatus LogAnalyzer::getCollectorStatus() const {
    return LogCollector::instance().status();
}

std::vector<LogHotspot> LogAnalyzer::getHotFiles(std::size_t limit) const {
    std::map<std::string, std::size_t> counts;
    for (const auto& record : collectAllRecords()) {
        ++counts[record.sourceFile];
    }
    return topHotspots(counts, limit);
}

std::vector<LogHotspot> LogAnalyzer::getHotMessages(std::size_t limit) const {
    std::map<std::string, std::size_t> counts;
    for (const auto& record : collectAllRecords()) {
        if (!record.message.empty()) {
            ++counts[record.message];
        }
    }
    return topHotspots(counts, limit);
}

LogAnomalySummary LogAnalyzer::getAnomalies() const {
    LogAnomalySummary anomalies;
    std::map<std::int64_t, std::size_t> errorBuckets;
    const auto alertConfig = LogSearchEngine::instance().config().alerts;

    for (const auto& record : collectAllRecords()) {
        if (record.level == "UNKNOWN") {
            ++anomalies.unknownLevelLines;
        }
        if (record.timestamp == 0) {
            ++anomalies.parseFailures;
        }
        if (record.level == "ERROR" || record.level == "FATAL") {
            const auto bucketStart = record.timestamp > 0 ? (record.timestamp / 300) * 300 : 0;
            ++errorBuckets[bucketStart];
        }
    }

    for (const auto& [_, count] : errorBuckets) {
        if (count >= alertConfig.errorSpikeThreshold) {
            ++anomalies.errorSpikes;
        }
    }
    return anomalies;
}

LogAlertSummary LogAnalyzer::getAlerts() const {
    LogAlertSummary summary;
    const auto analysis = analyze();
    const auto alertConfig = LogSearchEngine::instance().config().alerts;

    if (analysis.totalLines > 0 && analysis.errorRate >= alertConfig.highErrorRate) {
        appendAlert(summary,
                    "high_error_rate",
                    analysis.errorRate >= alertConfig.criticalErrorRate ? "critical" : "warning",
                    "错误率过高",
                    "ERROR 日志占比超过预设阈值",
                    analysis.errorRate,
                    alertConfig.highErrorRate);
    }

    if (analysis.anomalies.errorSpikes > 0) {
        appendAlert(summary,
                    "error_spike",
                    analysis.anomalies.errorSpikes >= alertConfig.criticalErrorSpikeCount ? "critical" : "warning",
                    "错误突增",
                    "存在 5 分钟窗口内错误集中爆发的现象",
                    static_cast<double>(analysis.anomalies.errorSpikes),
                    1.0);
    }

    if (analysis.totalLines > 0) {
        const auto unknownRate = static_cast<double>(analysis.anomalies.unknownLevelLines) /
                                 static_cast<double>(analysis.totalLines);
        if (unknownRate >= alertConfig.unknownLevelRatio) {
            appendAlert(summary,
                        "unknown_level_ratio",
                        unknownRate >= alertConfig.warningUnknownLevelRatio ? "warning" : "info",
                        "未知级别日志偏多",
                        "存在较多无法识别级别的日志行，建议检查日志格式",
                        unknownRate,
                        alertConfig.unknownLevelRatio);
        }

        const auto parseFailureRate = static_cast<double>(analysis.anomalies.parseFailures) /
                                      static_cast<double>(analysis.totalLines);
        if (parseFailureRate >= alertConfig.parseFailureRatio) {
            appendAlert(summary,
                        "parse_failure_ratio",
                        parseFailureRate >= alertConfig.warningParseFailureRatio ? "warning" : "info",
                        "日志解析失败偏多",
                        "存在较多无法提取时间戳的日志行，建议统一时间格式",
                        parseFailureRate,
                        alertConfig.parseFailureRatio);
        }
    }

    if (!analysis.hottestMessages.empty() && analysis.totalLines > 0) {
        const auto hottestRatio = static_cast<double>(analysis.hottestMessages.front().count) /
                                  static_cast<double>(analysis.totalLines);
        if (analysis.hottestMessages.front().count >= alertConfig.repeatedMessageCount ||
            hottestRatio >= alertConfig.repeatedMessageRatio) {
            appendAlert(summary,
                        "repeated_hot_message",
                        hottestRatio >= alertConfig.warningRepeatedMessageRatio ? "warning" : "info",
                        "重复热点消息",
                        "某条消息重复出现过多，可能代表抖动、重试或同类异常持续发生",
                        static_cast<double>(analysis.hottestMessages.front().count),
                        static_cast<double>(alertConfig.repeatedMessageCount));
        }
    }

    summary.totalAlerts = summary.alerts.size();
    summary.hasAlerts = !summary.alerts.empty();
    return summary;
}

LogSearchResult LogAnalyzer::getErrors(int limit) const {
    LogQuery query;
    query.level = "ERROR";
    query.limit = limit;
    return LogSearchEngine::instance().search(query);
}

LogSearchResult LogAnalyzer::getWarnings(int limit) const {
    LogQuery query;
    query.level = "WARN";
    query.limit = limit;
    return LogSearchEngine::instance().search(query);
}

LogSearchResult LogAnalyzer::getPerformance(int limit) const {
    LogQuery query;
    query.keyword = "slow";
    query.limit = limit;
    return LogSearchEngine::instance().search(query);
}

} // namespace Log
