/**
 * @file LogCtrl.h
 * @brief 日志查询和分析控制器 — 提供日志搜索、统计、分析等功能
 * 
 * 功能概述：
 *   - 日志搜索：支持多条件搜索日志
 *   - 统计分析：日志统计、趋势分析、异常检测
 *   - 热点分析：热点文件、热点消息、热点错误
 *   - 性能监控：性能指标、告警信息、错误统计
 *   - 日志收集：日志收集器状态、日志收集统计
 * 
 * API 端点：
 *   - GET /monitor/logs/search          - 搜索日志
 *   - GET /monitor/logs/stats           - 日志统计
 *   - GET /monitor/logs/trends          - 日志趋势
 *   - GET /monitor/logs/analysis        - 日志分析
 *   - GET /monitor/logs/collector       - 日志收集器状态
 *   - GET /monitor/logs/hot-files       - 热点文件
 *   - GET /monitor/logs/hot-messages    - 热点消息
 *   - GET /monitor/logs/anomalies       - 异常检测
 *   - GET /monitor/logs/alerts          - 告警信息
 *   - GET /monitor/logs/errors          - 错误日志
 *   - GET /monitor/logs/warnings        - 警告日志
 *   - GET /monitor/logs/performance     - 性能指标
 * 
 * 查询参数：
 *   - keyword: 搜索关键词
 *   - level: 日志级别（DEBUG、INFO、WARN、ERROR）
 *   - start_time: 开始时间（ISO 8601 格式）
 *   - end_time: 结束时间（ISO 8601 格式）
 *   - page: 页码（默认 1）
 *   - limit: 每页数量（默认 20）
 * 
 * @see LogCollector - 日志收集器
 * @see LogSearchEngine - 日志搜索引擎
 * @see LogAnalyzer - 日志分析器
 */

#pragma once

#include <drogon/HttpController.h>

namespace Monitor {

/**
 * @class LogCtrl
 * @brief 日志查询和分析控制器
 * 
 * 提供日志搜索、统计、分析等功能的 HTTP 接口。
 * 支持多条件搜索、趋势分析、异常检测等高级功能。
 */
class LogCtrl : public drogon::HttpController<LogCtrl> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LogCtrl::search, "/monitor/logs/search", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getStats, "/monitor/logs/stats", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getTrends, "/monitor/logs/trends", drogon::Get);
    ADD_METHOD_TO(LogCtrl::analyze, "/monitor/logs/analysis", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getCollectorStatus, "/monitor/logs/collector", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getHotFiles, "/monitor/logs/hot-files", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getHotMessages, "/monitor/logs/hot-messages", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getAnomalies, "/monitor/logs/anomalies", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getAlerts, "/monitor/logs/alerts", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getErrors, "/monitor/logs/errors", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getWarnings, "/monitor/logs/warnings", drogon::Get);
    ADD_METHOD_TO(LogCtrl::getPerformance, "/monitor/logs/performance", drogon::Get);
    METHOD_LIST_END

    /**
     * @brief 搜索日志
     * 
     * GET /monitor/logs/search
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 日志列表和分页信息
     */
    void search(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取日志统计
     * 
     * GET /monitor/logs/stats
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 日志统计信息（按级别、来源等）
     */
    void getStats(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取日志趋势
     * 
     * GET /monitor/logs/trends
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 日志趋势数据（时间序列）
     */
    void getTrends(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 日志分析
     * 
     * GET /monitor/logs/analysis
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 日志分析结果
     */
    void analyze(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取日志收集器状态
     * 
     * GET /monitor/logs/collector
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 日志收集器状态和统计
     */
    void getCollectorStatus(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取热点文件
     * 
     * GET /monitor/logs/hot-files
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 日志最多的文件列表
     */
    void getHotFiles(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取热点消息
     * 
     * GET /monitor/logs/hot-messages
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 出现最频繁的日志消息
     */
    void getHotMessages(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取异常检测结果
     * 
     * GET /monitor/logs/anomalies
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 检测到的异常日志
     */
    void getAnomalies(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取告警信息
     * 
     * GET /monitor/logs/alerts
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 告警日志列表
     */
    void getAlerts(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取错误日志
     * 
     * GET /monitor/logs/errors
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 错误日志列表
     */
    void getErrors(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取警告日志
     * 
     * GET /monitor/logs/warnings
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 警告日志列表
     */
    void getWarnings(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    /**
     * @brief 获取性能指标
     * 
     * GET /monitor/logs/performance
     * 
     * @param req HTTP 请求
     * @param callback 回调函数
     * @return 性能相关的日志指标
     */
    void getPerformance(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace Monitor
