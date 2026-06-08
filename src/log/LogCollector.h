#pragma once

#include "LogQuery.h"

#include <mutex>
#include <string>
#include <vector>

namespace Log {

struct LogSourceStatus {
    std::string name;
    std::string path;
    bool enabled{true};
    bool exists{false};
    bool directory{false};
    std::size_t fileCount{0};
};

struct LogCollectorStatus {
    bool running{false};
    bool enabled{true};
    std::string primaryPath;
    bool primaryPathExists{false};
    bool primaryPathDirectory{false};
    std::size_t totalDiscoveredFiles{0};
    std::size_t totalEnabledSources{0};
    std::vector<LogSourceStatus> sources;
};

class LogCollector {
public:
    static LogCollector& instance();

    void init(const LogConfig& config);
    void start();
    void stop();
    bool isRunning() const;
    LogConfig config() const;
    LogCollectorStatus status() const;

private:
    LogCollector() = default;

    mutable std::mutex mutex_;
    LogConfig config_;
    LogCollectorStatus status_;
    bool running_{false};
};

} // namespace Log
