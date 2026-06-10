/**
 * @file LogAnalyzer.h
 * @brief 日志分析器 — 日志统计、趋势分析和异常检测
 * 
 * 功能概述：
 *   - 日志统计：统计日志数量、错误数、警告数等
 *   - 趋势分析：分析日志的时间趋势和变化规律
 *   - 异常检测：检测异常日志和性能问题
 *   - 热点分析：找出日志最多的文件和消息
 *   - 告警摘要：汇总告警日志和错误日志
 *   - 性能分析：分析性能相关的日志
 * 
 * 核心特性：
 *   - 实时分析：实时分析日志数据
 *   - 多维度统计：按日期、级别、文件等多维度统计
 *   - 趋势预测：分析日志趋势，预测未来变化
 *   - 异常告警：自动检测异常日志和性能问题
 *   - 热点识别：快速识别问题热点
 *   - 高效查询：支持快速查询和过滤
 * 
 * 主要方法：
 *   - getStats() - 获取日志统计信息
 *   - getTrends() - 获取日志趋势数据
 *   - analyze() - 执行完整分析
 *   - getHotFiles() - 获取日志最多的文件
 *   - getHotMessages() - 获取最常见的日志消息
 *   - getAnomalies() - 获取异常日志摘要
 *   - getAlerts() - 获取告警日志摘要
 *   - getErrors() - 获取错误日志
 *   - getWarnings() - 获取警告日志
 *   - getPerformance() - 获取性能日志
 * 
 * 使用示例：
 *   ```cpp
 *   LogAnalyzer& analyzer = LogAnalyzer::instance();
 *   LogStats stats = analyzer.getStats();
 *   std::cout << "Total logs: " << stats.totalCount << std::endl;
 *   
 *   // 获取趋势数据
 *   TimeRange range{start_time, end_time};
 *   auto trends = analyzer.getTrends(range, 3600); // 按小时统计
 *   
 *   // 获取异常日志
 *   auto anomalies = analyzer.getAnomalies();
 *   for (const auto& anomaly : anomalies.items) {
 *       std::cout << "Anomaly: " << anomaly.description << std::endl;
 *   }
 *   ```
 * 
 * 配置项（config.json）：
 *   - log.analysis.enabled: 是否启用分析（默认 true）
 *   - log.analysis.retention_days: 分析数据保留天数（默认 30）
 *   - log.analysis.anomaly_threshold: 异常检测阈值（默认 2.0）
 *   - log.analysis.performance_threshold: 性能告警阈值（默认 1000ms）
 * 
 * 统计指标：
 *   - totalCount：总日志数
 *   - errorCount：错误日志数
 *   - warningCount：警告日志数
 *   - infoCount：信息日志数
 *   - debugCount：调试日志数
 *   - avgResponseTime：平均响应时间
 *   - maxResponseTime：最大响应时间
 *   - errorRate：错误率
 * 
 * 趋势分析：
 *   - 时间序列数据：按时间段统计日志数
 *   - 变化率：计算日志数的变化率
 *   - 预测值：预测未来日志数
 *   - 异常点：识别异常的时间段
 * 
 * @see LogCollector - 日志收集器
 * @see LogQuery - 日志查询
 * @see LogSearchEngine - 日志搜索引擎
 */

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
