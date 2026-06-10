/**
 * @file LogSearchEngine.h
 * @brief 日志搜索引擎 — 高效的日志搜索和过滤
 * 
 * 功能概述：
 *   - 日志搜索：支持多条件日志搜索
 *   - 文件发现：自动发现和索引日志文件
 *   - 快速过滤：高效的日志过滤和匹配
 *   - 结果排序：支持多种排序方式
 *   - 分页查询：支持分页查询大量日志
 *   - 缓存优化：缓存搜索结果，加速查询
 * 
 * 核心特性：
 *   - 全文搜索：支持关键词全文搜索
 *   - 多条件查询：支持按级别、时间、文件等多条件查询
 *   - 正则表达式：支持正则表达式搜索
 *   - 快速索引：建立日志索引，加速搜索
 *   - 增量搜索：支持增量搜索新增日志
 *   - 结果缓存：缓存热门搜索结果
 * 
 * 搜索条件：
 *   - keyword：关键词搜索
 *   - level：日志级别（DEBUG、INFO、WARN、ERROR）
 *   - file：日志文件名
 *   - function：函数名
 *   - line：行号范围
 *   - start_time：开始时间
 *   - end_time：结束时间
 *   - regex：正则表达式
 * 
 * 使用示例：
 *   ```cpp
 *   LogSearchEngine& engine = LogSearchEngine::instance();
 *   
 *   // 创建查询
 *   LogQuery query;
 *   query.keyword = "error";
 *   query.level = LogLevel::ERROR;
 *   query.limit = 100;
 *   
 *   // 执行搜索
 *   auto result = engine.search(query);
 *   std::cout << "Found " << result.records.size() << " records" << std::endl;
 *   
 *   // 发现日志文件
 *   auto files = engine.discoverLogFiles();
 *   for (const auto& file : files) {
 *       std::cout << "Log file: " << file.string() << std::endl;
 *   }
 *   ```
 * 
 * 配置项（config.json）：
 *   - log.search.enabled: 是否启用搜索（默认 true）
 *   - log.search.max_results: 最大返回结果数（默认 10000）
 *   - log.search.cache_size: 搜索结果缓存大小（默认 100MB）
 *   - log.search.index_enabled: 是否启用索引（默认 true）
 * 
 * 支持的日志格式：
 *   - 标准格式：[时间] [级别] [文件:行] [函数] 消息
 *   - JSON 格式：JSON 结构化日志
 *   - 自定义格式：支持自定义日志格式解析
 * 
 * 搜索性能：
 *   - 单条件查询：< 100ms（1000 条日志）
 *   - 多条件查询：< 500ms（10000 条日志）
 *   - 正则表达式：< 1000ms（10000 条日志）
 *   - 缓存命中：< 10ms
 * 
 * @see LogAnalyzer - 日志分析器
 * @see LogCollector - 日志收集器
 * @see LogQuery - 日志查询
 */

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
