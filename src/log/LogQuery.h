/**
 * @file LogQuery.h
 * @brief 日志查询和配置 — 日志查询参数、配置和数据结构
 * 
 * 功能概述：
 *   - 日志查询参数：定义日志查询的各种条件
 *   - 日志配置：日志系统的全局配置
 *   - 数据结构：日志相关的数据结构定义
 *   - Elasticsearch 集成：支持 Elasticsearch 日志存储
 *   - Kibana 集成：支持 Kibana 日志可视化
 *   - 告警规则：定义日志告警规则
 * 
 * 核心特性：
 *   - 灵活查询：支持多条件日志查询
 *   - 高级过滤：支持正则表达式和复杂过滤
 *   - 分页支持：支持大数据量分页查询
 *   - 排序功能：支持多字段排序
 *   - 聚合统计：支持日志聚合统计
 *   - 告警规则：支持自定义告警规则
 * 
 * 查询参数：
 *   - keyword：关键词搜索
 *   - level：日志级别
 *   - file：日志文件
 *   - function：函数名
 *   - start_time：开始时间
 *   - end_time：结束时间
 *   - limit：返回条数
 *   - offset：分页偏移
 *   - sort_by：排序字段
 *   - order：排序顺序
 * 
 * 配置项（config.json）：
 *   - log.enabled: 是否启用日志系统（默认 true）
 *   - log.primary_path: 主日志路径
 *   - log.sources: 日志源列表
 *   - log.elasticsearch: Elasticsearch 配置
 *   - log.kibana: Kibana 配置
 *   - log.alert_rules: 告警规则配置
 * 
 * Elasticsearch 配置：
 *   - enabled：是否启用 Elasticsearch
 *   - host：Elasticsearch 服务器地址
 *   - port：Elasticsearch 服务器端口
 *   - indexPrefix：索引前缀
 * 
 * Kibana 配置：
 *   - enabled：是否启用 Kibana
 *   - host：Kibana 服务器地址
 *   - port：Kibana 服务器端口
 * 
 * 告警规则：
 *   - highErrorRate：高错误率阈值（默认 20%）
 *   - criticalErrorRate：严重错误率阈值（默认 40%）
 *   - errorSpikeThreshold：错误数量激增阈值
 *   - repeatedMessageCount：重复消息计数阈值
 * 
 * @see LogAnalyzer - 日志分析器
 * @see LogCollector - 日志收集器
 * @see LogSearchEngine - 日志搜索引擎
 */

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
