#include "LogCollector.h"

#include <filesystem>

namespace Log {
namespace {

std::size_t countRegularFiles(const std::filesystem::path& root, bool recursive) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return 0;
    }

    std::size_t count = 0;
    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
            if (ec) {
                break;
            }
            if (entry.is_regular_file()) {
                ++count;
            }
        }
        return count;
    }

    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file()) {
            ++count;
        }
    }
    return count;
}

LogSourceStatus buildSourceStatus(const std::string& name,
                                  const std::string& path,
                                  bool enabled,
                                  bool recursive) {
    LogSourceStatus status;
    status.name = name;
    status.path = path;
    status.enabled = enabled;

    std::error_code ec;
    const std::filesystem::path fsPath(path);
    status.exists = !path.empty() && std::filesystem::exists(fsPath, ec);
    status.directory = status.exists && std::filesystem::is_directory(fsPath, ec);
    status.fileCount = status.directory ? countRegularFiles(fsPath, recursive) : 0;
    return status;
}

} // namespace

LogCollector& LogCollector::instance() {
    static LogCollector collector;
    return collector;
}

void LogCollector::init(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    if (config_.path.empty()) {
        config_.path = "./logs";
    }

    status_ = {};
    status_.enabled = config_.enabled;
    status_.primaryPath = config_.path;

    std::error_code ec;
    const std::filesystem::path primaryPath(config_.path);
    status_.primaryPathExists = std::filesystem::exists(primaryPath, ec);
    status_.primaryPathDirectory = status_.primaryPathExists && std::filesystem::is_directory(primaryPath, ec);
    status_.sources.push_back(buildSourceStatus("default", config_.path, true, config_.recursive));

    for (const auto& extraPath : config_.paths) {
        if (extraPath.empty()) {
            continue;
        }
        status_.sources.push_back(buildSourceStatus(extraPath, extraPath, true, config_.recursive));
    }

    for (const auto& source : config_.sources) {
        status_.sources.push_back(buildSourceStatus(
            source.name.empty() ? source.path : source.name,
            source.path,
            source.enabled,
            config_.recursive));
    }

    for (const auto& sourceStatus : status_.sources) {
        if (sourceStatus.enabled) {
            ++status_.totalEnabledSources;
            status_.totalDiscoveredFiles += sourceStatus.fileCount;
        }
    }
}

void LogCollector::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    std::filesystem::create_directories(config_.path.empty() ? "./logs" : config_.path, ec);
    running_ = !ec;
    status_.running = running_;
    if (running_) {
        status_.primaryPathExists = true;
        status_.primaryPathDirectory = true;
        if (!status_.sources.empty()) {
            status_.sources.front() = buildSourceStatus("default", config_.path, true, config_.recursive);
        }
    }
}

void LogCollector::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    status_.running = false;
}

bool LogCollector::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

LogConfig LogCollector::config() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

LogCollectorStatus LogCollector::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

} // namespace Log
