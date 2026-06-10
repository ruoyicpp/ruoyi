/**
 * @file MetricsCtrl.h
 * @brief Prometheus 指标端点控制器 — 导出应用性能指标
 * 
 * 功能概述：
 *   - Prometheus 指标导出：导出 Prometheus 格式的性能指标
 *   - 应用统计：构建信息、启动时间、运行时长
 *   - 性能指标：请求数、响应时间、错误率
 *   - 资源使用：CPU、内存、线程、连接数
 *   - 业务指标：用户数、会话数、缓存命中率
 * 
 * 核心特性：
 *   - Prometheus 兼容：完全兼容 Prometheus 指标格式
 *   - 实时导出：动态计算和导出最新指标
 *   - 多维度标签：支持标签维度的指标分组
 *   - 低开销：最小化指标收集的性能开销
 *   - 易于集成：与 Prometheus、Grafana 无缝集成
 * 
 * API 端点：
 *   - GET /metrics - Prometheus 指标端点
 *   - GET /monitor/stats - 应用统计信息
 * 
 * 导出的指标类型：
 *   - Counter（计数器）：请求总数、错误总数
 *   - Gauge（仪表）：当前连接数、内存使用
 *   - Histogram（直方图）：请求延迟分布
 *   - Summary（摘要）：请求延迟分位数
 * 
 * 指标示例：
 *   ```
 *   # HELP ruoyi_build_info Build information
 *   # TYPE ruoyi_build_info gauge
 *   ruoyi_build_info{version="1.3.2",build_time="2026-06-10"} 1
 *   
 *   # HELP ruoyi_http_requests_total Total HTTP requests
 *   # TYPE ruoyi_http_requests_total counter
 *   ruoyi_http_requests_total{method="GET",path="/api/users"} 1234
 *   
 *   # HELP ruoyi_http_request_duration_seconds HTTP request duration
 *   # TYPE ruoyi_http_request_duration_seconds histogram
 *   ruoyi_http_request_duration_seconds_bucket{le="0.1"} 100
 *   ```
 * 
 * Prometheus 配置示例：
 *   ```yaml
 *   global:
 *     scrape_interval: 15s
 *   
 *   scrape_configs:
 *     - job_name: 'ruoyi-cpp'
 *       static_configs:
 *         - targets: ['localhost:18080']
 *       metrics_path: '/metrics'
 *   ```
 * 
 * Grafana 集成：
 *   1. 添加 Prometheus 数据源
 *   2. 创建仪表板
 *   3. 使用 PromQL 查询指标
 *   4. 配置告警规则
 * 
 * 配置项（config.json）：
 *   - metrics.enabled: 是否启用指标导出（默认 true）
 *   - metrics.include_system: 是否包含系统指标（默认 true）
 *   - metrics.include_business: 是否包含业务指标（默认 true）
 *   - metrics.histogram_buckets: 直方图桶配置
 * 
 * 最佳实践：
 *   - 定期抓取指标（15-30 秒）
 *   - 配置告警规则监控关键指标
 *   - 使用 Grafana 可视化指标
 *   - 定期审查指标趋势
 *   - 基于指标进行容量规划
 * 
 * @see MetricsCollector - 指标收集器
 * @see HealthCtrl - 健康检查控制器
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <sstream>

#include <drogon/HttpController.h>

#include "MetricsCollector.h"

class MetricsCtrl : public drogon::HttpController<MetricsCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MetricsCtrl::getMetrics, "/metrics", drogon::Get);
        ADD_METHOD_TO(MetricsCtrl::getStats, "/monitor/stats", drogon::Get);
    METHOD_LIST_END

    void getMetrics(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        (void)req;
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);

        std::ostringstream body;
        body << "# HELP ruoyi_build_info Build information\n";
        body << "# TYPE ruoyi_build_info gauge\n";
        body << "ruoyi_build_info{version=\"1.0.0\"} 1\n\n";
        body << Metrics::Registry::instance().exportToPrometheus();

        resp->setBody(body.str());
        cb(resp);
    }

    void getStats(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        (void)req;
        Json::Value stats;
        stats["uptimeSeconds"] = static_cast<Json::Int64>(getUptime());
        stats["version"] = "1.0.0";
        stats["service"] = "ruoyi-cpp";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(stats);
        cb(resp);
    }

private:
    static int64_t getUptime() {
        static const auto start = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now() - start)
            .count();
    }
};
