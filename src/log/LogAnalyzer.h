#pragma once

#include "LogCollector.h"
#include "LogQuery.h"

#include <vector>

namespace Log {

class LogAnalyzer {
public:
    static LogAnalyzer& instance();

    void init(const LogConfig& config);
    LogStats getStats() const;
    std::vector<LogTrendPoint> getTrends(const TimeRange& range,
                                         std::int64_t bucketSeconds) const;
    LogAnalysis analyze() const;
    LogCollectorStatus getCollectorStatus() const;
    std::vector<LogHotspot> getHotFiles(std::size_t limit = 10) const;
    std::vector<LogHotspot> getHotMessages(std::size_t limit = 10) const;
    LogAnomalySummary getAnomalies() const;
    LogAlertSummary getAlerts() const;
    LogSearchResult getErrors(int limit = 100) const;
    LogSearchResult getWarnings(int limit = 100) const;
    LogSearchResult getPerformance(int limit = 100) const;
};

} // namespace Log
