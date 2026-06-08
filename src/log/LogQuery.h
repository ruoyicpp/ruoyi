#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Log {

struct ElasticsearchConfig {
    bool enabled{false};
    std::string host{"127.0.0.1"};
    int port{9200};
    std::string indexPrefix{"ruoyi-logs"};
};

struct KibanaConfig {
    bool enabled{false};
    std::string host{"127.0.0.1"};
    int port{5601};
};

struct LogSource {
    std::string name;
    std::string path;
    bool enabled{true};
};

struct LogAlertRuleConfig {
    double highErrorRate{0.20};
    double criticalErrorRate{0.40};
    std::size_t errorSpikeThreshold{5};
    std::size_t criticalErrorSpikeCount{3};
    double unknownLevelRatio{0.10};
    double warningUnknownLevelRatio{0.25};
    double parseFailureRatio{0.10};
    double warningParseFailureRatio{0.30};
    std::size_t repeatedMessageCount{20};
    double repeatedMessageRatio{0.15};
    double warningRepeatedMessageRatio{0.30};
};

struct LogConfig {
    bool enabled{true};
    std::string path{"./logs"};
    int maxFiles{20};
    int maxResults{500};
    bool recursive{true};
    std::vector<std::string> paths;
    std::vector<LogSource> sources;
    ElasticsearchConfig elasticsearch;
    KibanaConfig kibana;
    LogAlertRuleConfig alerts;
};

struct TimeRange {
    std::int64_t start{0};
    std::int64_t end{0};

    bool contains(std::int64_t value) const {
        if (start > 0 && value < start) {
            return false;
        }
        if (end > 0 && value > end) {
            return false;
        }
        return true;
    }
};

struct LogQuery {
    std::string keyword;
    std::string level;
    std::string sourceFile;
    TimeRange timeRange;
    int limit{100};
    int offset{0};
};

struct LogRecord {
    std::int64_t timestamp{0};
    std::string timestampText;
    std::string level;
    std::string message;
    std::string sourceFile;
    std::string sourcePath;
    std::string sourceName;
    std::string thread;
    std::string logger;
    std::string rawLine;
    std::size_t lineNumber{0};
};

struct LogSearchResult {
    std::vector<LogRecord> records;
    std::size_t totalMatches{0};
    std::size_t scannedFiles{0};
    std::size_t scannedLines{0};
    std::uint64_t elapsedMs{0};
};

struct LogStats {
    std::size_t totalFiles{0};
    std::size_t totalLines{0};
    std::map<std::string, std::size_t> levelCounts;
    std::map<std::string, std::size_t> fileCounts;
    std::map<std::string, std::size_t> sourceCounts;
};

struct LogTrendPoint {
    std::int64_t bucketStart{0};
    std::size_t total{0};
    std::size_t errors{0};
    std::size_t warnings{0};
};

struct LogHotspot {
    std::string key;
    std::size_t count{0};
};

struct LogAnomalySummary {
    std::size_t errorSpikes{0};
    std::size_t unknownLevelLines{0};
    std::size_t parseFailures{0};
};

struct LogAlert {
    std::string ruleId;
    std::string level;
    std::string title;
    std::string summary;
    double value{0.0};
    double threshold{0.0};
};

struct LogAlertSummary {
    bool hasAlerts{false};
    std::size_t totalAlerts{0};
    std::vector<LogAlert> alerts;
};

struct LogAnalysis {
    std::string dominantLevel;
    std::string hottestFile;
    double errorRate{0.0};
    std::size_t totalFiles{0};
    std::size_t totalLines{0};
    std::vector<LogHotspot> hottestFiles;
    std::vector<LogHotspot> hottestMessages;
    LogAnomalySummary anomalies;
};

} // namespace Log
