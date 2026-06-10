/**
 * @file LogCollector.h
 * @brief 日志收集器 — 从多个源收集和聚合日志
 * 
 * 功能概述：
 *   - 多源收集：从多个日志文件和目录收集日志
 *   - 实时监控：监控日志文件变化，实时收集新日志
 *   - 日志聚合：将多个源的日志聚合到统一存储
 *   - 源管理：管理日志源的启用/禁用状态
 *   - 状态监控：监控日志收集器的运行状态
 *   - 性能优化：高效的日志收集和存储
 * 
 * 核心特性：
 *   - 多源支持：支持多个日志文件和目录
 *   - 实时收集：实时监控日志文件变化
 *   - 自动发现：自动发现新的日志文件
 *   - 增量收集：只收集新增的日志
 *   - 线程安全：支持多线程并发访问
 *   - 高效存储：优化的日志存储格式
 * 
 * 日志源类型：
 *   - 文件源：单个日志文件
 *   - 目录源：目录下的所有日志文件
 *   - 应用源：应用内部日志
 *   - 系统源：系统日志
 * 
 * 使用示例：
 *   ```cpp
 *   LogCollector& collector = LogCollector::instance();
 *   
 *   // 添加日志源
 *   collector.addSource("app_logs", "/var/log/app/", true);
 *   collector.addSource("system_logs", "/var/log/syslog", false);
 *   
 *   // 启动收集
 *   collector.start();
 *   
 *   // 获取收集器状态
 *   auto status = collector.getStatus();
 *   std::cout << "Collector running: " << status.running << std::endl;
 *   std::cout << "Total files: " << status.totalDiscoveredFiles << std::endl;
 *   ```
 * 
 * 配置项（config.json）：
 *   - log.collector.enabled: 是否启用收集器（默认 true）
 *   - log.collector.primary_path: 主日志路径
 *   - log.collector.sources: 日志源列表
 *   - log.collector.watch_interval: 监控间隔（秒，默认 5）
 *   - log.collector.max_files: 最大监控文件数（默认 1000）
 * 
 * 日志源配置：
 *   ```json
 *   {
 *     "sources": [
 *       {
 *         "name": "app_logs",
 *         "path": "/var/log/app/",
 *         "enabled": true,
 *         "recursive": true
 *       }
 *     ]
 *   }
 *   ```
 * 
 * 状态字段：
 *   - running：收集器是否运行中
 *   - enabled：收集器是否启用
 *   - primaryPath：主日志路径
 *   - totalDiscoveredFiles：发现的总文件数
 *   - totalEnabledSources：启用的源数
 *   - sources：日志源列表
 * 
 * @see LogAnalyzer - 日志分析器
 * @see LogQuery - 日志查询
 * @see LogSearchEngine - 日志搜索引擎
 */

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
