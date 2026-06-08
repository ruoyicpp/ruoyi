#pragma once

#include "LogQuery.h"

#include <filesystem>
#include <mutex>
#include <vector>

namespace Log {

class LogSearchEngine {
public:
    static LogSearchEngine& instance();

    void init(const LogConfig& config);
    const LogConfig& config() const;

    LogSearchResult search(const LogQuery& query) const;
    std::vector<std::filesystem::path> discoverLogFiles() const;

private:
    struct CandidateFile {
        std::filesystem::path path;
        std::string sourceName;
    };
    LogSearchEngine() = default;

    LogConfig snapshotConfig() const;
    bool isSupportedLogFile(const std::filesystem::path& path) const;
    bool matchesQuery(const LogRecord& record, const LogQuery& query) const;
    std::vector<CandidateFile> discoverCandidateFiles() const;
    LogRecord parseLine(const CandidateFile& file,
                        const std::string& line,
                        std::size_t lineNumber) const;

    mutable std::mutex mutex_;
    LogConfig config_;
};

} // namespace Log
