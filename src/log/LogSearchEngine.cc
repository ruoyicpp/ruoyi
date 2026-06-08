#include "LogSearchEngine.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <map>
#include <regex>
#include <system_error>

namespace Log {
namespace {

std::string toLowerCopy(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string toUpperCopy(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return out;
}

std::string trimCopy(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::int64_t toUnixSeconds(const std::tm& tm) {
    std::tm copy = tm;
    return static_cast<std::int64_t>(std::mktime(&copy));
}

std::int64_t tryParseTimestamp(const std::string& line, std::string& timestampText) {
    static const std::regex patterns[] = {
        std::regex(R"(((\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2}):(\d{2})))"),
        std::regex(R"(((\d{4})/(\d{2})/(\d{2})[ T](\d{2}):(\d{2}):(\d{2})))")
    };

    std::smatch match;
    for (const auto& pattern : patterns) {
        if (std::regex_search(line, match, pattern)) {
            std::tm tm{};
            tm.tm_year = std::stoi(match[2].str()) - 1900;
            tm.tm_mon = std::stoi(match[3].str()) - 1;
            tm.tm_mday = std::stoi(match[4].str());
            tm.tm_hour = std::stoi(match[5].str());
            tm.tm_min = std::stoi(match[6].str());
            tm.tm_sec = std::stoi(match[7].str());
            timestampText = match[1].str();
            return toUnixSeconds(tm);
        }
    }

    timestampText.clear();
    return 0;
}

std::string detectLevel(const std::string& line) {
    static const std::vector<std::string> knownLevels = {
        "TRACE", "DEBUG", "INFO", "WARN", "WARNING", "ERROR", "FATAL"
    };

    const auto upper = toUpperCopy(line);
    for (const auto& level : knownLevels) {
        const auto token = "[" + level + "]";
        if (upper.find(token) != std::string::npos || upper.find(level) != std::string::npos) {
            return level == "WARNING" ? "WARN" : level;
        }
    }
    return "UNKNOWN";
}

std::string detectBracketField(const std::string& line, const std::string& fieldName) {
    const auto pattern = std::regex("\\[" + fieldName + R"([=: ]([^"]+?)\])", std::regex::icase);
    std::smatch match;
    if (std::regex_search(line, match, pattern) && match.size() > 1) {
        return trimCopy(match[1].str());
    }
    return {};
}

std::string extractMessage(const std::string& line, const std::string& level) {
    if (level == "UNKNOWN") {
        return trimCopy(line);
    }

    const auto upper = toUpperCopy(line);
    const auto tokenPos = upper.find(level);
    if (tokenPos == std::string::npos) {
        return trimCopy(line);
    }

    auto start = tokenPos + level.size();
    while (start < line.size()) {
        const auto ch = line[start];
        if (ch != ' ' && ch != '\t' && ch != ':' && ch != ']' && ch != '-') {
            break;
        }
        ++start;
    }
    return trimCopy(line.substr(start));
}

bool isSupportedLogFilePath(const std::filesystem::path& path) {
    const auto ext = toLowerCopy(path.extension().string());
    return ext == ".log" || ext == ".txt" || ext == ".jsonl";
}

void appendFilesFromRoot(std::vector<std::pair<std::filesystem::path, std::string>>& files,
                         const std::filesystem::path& root,
                         const std::string& sourceName,
                         bool recursive) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return;
    }

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
            if (ec) {
                break;
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            if (!isSupportedLogFilePath(entry.path())) {
                continue;
            }
            files.push_back({entry.path(), sourceName});
        }
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!isSupportedLogFilePath(entry.path())) {
            continue;
        }
        files.push_back({entry.path(), sourceName});
    }
}

} // namespace

LogSearchEngine& LogSearchEngine::instance() {
    static LogSearchEngine engine;
    return engine;
}

void LogSearchEngine::init(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    if (config_.path.empty()) {
        config_.path = "./logs";
    }
    if (config_.maxFiles <= 0) {
        config_.maxFiles = 20;
    }
    if (config_.maxResults <= 0) {
        config_.maxResults = 500;
    }
}

const LogConfig& LogSearchEngine::config() const {
    return config_;
}

LogConfig LogSearchEngine::snapshotConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

std::vector<LogSearchEngine::CandidateFile> LogSearchEngine::discoverCandidateFiles() const {
    const auto config = snapshotConfig();
    std::vector<std::pair<std::filesystem::path, std::string>> discovered;

    appendFilesFromRoot(discovered,
                        std::filesystem::path(config.path.empty() ? "./logs" : config.path),
                        "default",
                        config.recursive);

    for (const auto& extraPath : config.paths) {
        if (!extraPath.empty()) {
            appendFilesFromRoot(discovered,
                                std::filesystem::path(extraPath),
                                extraPath,
                                config.recursive);
        }
    }

    for (const auto& source : config.sources) {
        if (!source.enabled || source.path.empty()) {
            continue;
        }
        appendFilesFromRoot(discovered,
                            std::filesystem::path(source.path),
                            source.name.empty() ? source.path : source.name,
                            config.recursive);
    }

    std::sort(discovered.begin(), discovered.end(), [](const auto& lhs, const auto& rhs) {
        std::error_code lec;
        std::error_code rec;
        const auto lt = std::filesystem::last_write_time(lhs.first, lec);
        const auto rt = std::filesystem::last_write_time(rhs.first, rec);
        if (lec || rec) {
            return lhs.first.filename().string() < rhs.first.filename().string();
        }
        return lt > rt;
    });

    std::vector<CandidateFile> deduped;
    std::map<std::string, bool> seen;
    for (const auto& file : discovered) {
        const auto normalized = file.first.lexically_normal().string();
        if (seen[normalized]) {
            continue;
        }
        seen[normalized] = true;
        deduped.push_back({file.first, file.second});
    }

    if (static_cast<int>(deduped.size()) > config.maxFiles) {
        deduped.resize(static_cast<std::size_t>(config.maxFiles));
    }
    return deduped;
}

std::vector<std::filesystem::path> LogSearchEngine::discoverLogFiles() const {
    std::vector<std::filesystem::path> files;
    for (const auto& candidate : discoverCandidateFiles()) {
        files.push_back(candidate.path);
    }
    return files;
}

bool LogSearchEngine::isSupportedLogFile(const std::filesystem::path& path) const {
    return isSupportedLogFilePath(path);
}

LogRecord LogSearchEngine::parseLine(const CandidateFile& file,
                                     const std::string& line,
                                     std::size_t lineNumber) const {
    LogRecord record;
    record.sourceFile = file.path.filename().string();
    record.sourcePath = file.path.string();
    record.sourceName = file.sourceName;
    record.rawLine = line;
    record.lineNumber = lineNumber;
    record.timestamp = tryParseTimestamp(line, record.timestampText);
    record.level = detectLevel(line);
    record.thread = detectBracketField(line, "thread");
    record.logger = detectBracketField(line, "logger");
    record.message = extractMessage(line, record.level);
    if (record.message.empty()) {
        record.message = line;
    }
    return record;
}

bool LogSearchEngine::matchesQuery(const LogRecord& record, const LogQuery& query) const {
    if (!query.keyword.empty()) {
        const auto haystack = toLowerCopy(record.rawLine + "\n" + record.message);
        const auto needle = toLowerCopy(query.keyword);
        if (haystack.find(needle) == std::string::npos) {
            return false;
        }
    }

    if (!query.level.empty() && toUpperCopy(record.level) != toUpperCopy(query.level)) {
        return false;
    }

    if (!query.sourceFile.empty()) {
        const auto fileNeedle = toLowerCopy(query.sourceFile);
        const auto fileHaystack = toLowerCopy(record.sourceFile + "\n" + record.sourcePath + "\n" + record.sourceName);
        if (fileHaystack.find(fileNeedle) == std::string::npos) {
            return false;
        }
    }

    if (!query.timeRange.contains(record.timestamp)) {
        return false;
    }

    return true;
}

LogSearchResult LogSearchEngine::search(const LogQuery& query) const {
    LogSearchResult result;
    const auto begin = std::chrono::steady_clock::now();
    const auto files = discoverCandidateFiles();
    result.scannedFiles = files.size();

    const auto limit = std::max(query.limit, 0);
    const auto offset = std::max(query.offset, 0);

    for (const auto& file : files) {
        std::ifstream in(file.path, std::ios::in);
        if (!in.is_open()) {
            continue;
        }

        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(in, line)) {
            ++lineNumber;
            ++result.scannedLines;

            auto record = parseLine(file, line, lineNumber);
            if (!matchesQuery(record, query)) {
                continue;
            }

            const auto currentIndex = result.totalMatches++;
            if (currentIndex < static_cast<std::size_t>(offset)) {
                continue;
            }
            if (limit > 0 && static_cast<int>(result.records.size()) >= limit) {
                continue;
            }
            result.records.push_back(std::move(record));
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsedMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
    return result;
}

} // namespace Log
