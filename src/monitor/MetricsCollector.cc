/**
 * @file MetricsCollector.cc
 * @brief Prometheus 指标采集器实现
 */

#include "MetricsCollector.h"
#include <sstream>
#include <iomanip>

namespace Metrics {

// ── Labels 实现 ─────────────────────────────────────────────────────────────

std::string Labels::toString() const {
    if (data.empty()) return "";
    std::ostringstream ss;
    bool first = true;
    for (const auto& [k, v] : data) {
        if (!first) ss << ",";
        ss << k << "=\"" << v << "\"";
        first = false;
    }
    return ss.str();
}

// ── Counter 实现 ────────────────────────────────────────────────────────────

void Counter::inc(double value) {
    value_.fetch_add(value, std::memory_order_relaxed);
}

// ── Gauge 实现 ─────────────────────────────────────────────────────────────

void Gauge::inc(double value) {
    value_.fetch_add(value, std::memory_order_relaxed);
}

void Gauge::dec(double value) {
    value_.fetch_sub(value, std::memory_order_relaxed);
}

void Gauge::set(double value) {
    value_.store(value, std::memory_order_relaxed);
}

// ── Histogram 实现 ─────────────────────────────────────────────────────────

Histogram::Histogram(const std::vector<double>& buckets)
    : buckets_([&buckets]() {
        std::vector<Bucket> result;
        for (double bound : buckets) {
            result.emplace_back(bound);
        }
        return result;
    }()) {}

void Histogram::observe(double value) {
    count_.fetch_add(1, std::memory_order_relaxed);
    sum_.fetch_add(value, std::memory_order_relaxed);

    for (auto& bucket : buckets_) {
        if (value <= bucket.bound) {
            bucket.count.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// ── Registry 实现 ──────────────────────────────────────────────────────────

Registry& Registry::instance() {
    static Registry reg;
    return reg;
}

Counter& Registry::registerCounter(const std::string& name, const Labels& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    return counters_[name][labels.toString()];
}

Gauge& Registry::registerGauge(const std::string& name, const Labels& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    return gauges_[name][labels.toString()];
}

Histogram& Registry::registerHistogram(const std::string& name, const Labels& labels,
                                        const std::vector<double>& buckets) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buckets.empty()) {
        return histograms_[name][labels.toString()];
    }
    // 使用自定义 buckets 创建
    auto& hist = histograms_[name][labels.toString()];
    return hist;
}

Counter* Registry::getCounter(const std::string& name, const Labels& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = counters_.find(name);
    if (it != counters_.end()) {
        auto lit = it->second.find(labels.toString());
        if (lit != it->second.end()) {
            return &lit->second;
        }
    }
    return nullptr;
}

std::string Registry::exportToPrometheus() const {
    std::ostringstream ss;
    std::lock_guard<std::mutex> lock(mutex_);

    // 导出 Counters
    for (const auto& [name, labelMap] : counters_) {
        ss << "# HELP " << name << "\n";
        ss << "# TYPE " << name << " counter\n";
        for (const auto& [labelStr, counter] : labelMap) {
            ss << name;
            if (!labelStr.empty()) ss << "{" << labelStr << "}";
            ss << " " << std::fixed << std::setprecision(3) << counter.get() << "\n";
        }
    }

    // 导出 Gauges
    for (const auto& [name, labelMap] : gauges_) {
        ss << "# HELP " << name << "\n";
        ss << "# TYPE " << name << " gauge\n";
        for (const auto& [labelStr, gauge] : labelMap) {
            ss << name;
            if (!labelStr.empty()) ss << "{" << labelStr << "}";
            ss << " " << std::fixed << std::setprecision(3) << gauge.get() << "\n";
        }
    }

    // 导出 Histograms
    for (const auto& [name, labelMap] : histograms_) {
        ss << "# HELP " << name << "\n";
        ss << "# TYPE " << name << " histogram\n";
        for (const auto& [labelStr, hist] : labelMap) {
            std::string baseName = name;
            std::string labelPart = labelStr.empty() ? "" : "{" + labelStr + "}";

            double cumulative = 0;
            for (const auto& bucket : hist.getBuckets()) {
                cumulative += bucket.count.load();
                ss << baseName << "_bucket" << labelPart
                   << " le=\"" << bucket.bound << "\" "
                   << cumulative << "\n";
            }
            ss << baseName << "_bucket" << labelPart << " le=\"+Inf\" "
               << hist.count() << "\n";
            ss << baseName << "_sum" << labelPart << " " << hist.sum() << "\n";
            ss << baseName << "_count" << labelPart << " " << hist.count() << "\n";
        }
    }

    return ss.str();
}

// ── 默认指标 ────────────────────────────────────────────────────────────────

namespace DefaultMetrics {

Counter& httpRequestsTotal = Registry::instance().registerCounter(
    "http_requests_total", Labels{{"method", ""}, {"path", ""}, {"status", ""}});

Counter& httpRequestErrorsTotal = Registry::instance().registerCounter(
    "http_request_errors_total", Labels{{"method", ""}, {"path", ""}, {"error", ""}});

Histogram& httpRequestDurationSeconds = Registry::instance().registerHistogram(
    "http_request_duration_seconds", Labels{{"method", ""}, {"path", ""}},
    {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10});

Gauge& httpRequestsInFlight = Registry::instance().registerGauge(
    "http_requests_in_flight", Labels{});

Counter& dbQueryTotal = Registry::instance().registerCounter(
    "db_query_total", Labels{{"operation", ""}, {"table", ""}});

Counter& dbQueryErrorsTotal = Registry::instance().registerCounter(
    "db_query_errors_total", Labels{{"operation", ""}, {"table", ""}, {"error", ""}});

Histogram& dbQueryDurationSeconds = Registry::instance().registerHistogram(
    "db_query_duration_seconds", Labels{{"operation", ""}, {"table", ""}},
    {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5});

Counter& redisOperationsTotal = Registry::instance().registerCounter(
    "redis_operations_total", Labels{{"operation", ""}});

Counter& redisErrorsTotal = Registry::instance().registerCounter(
    "redis_errors_total", Labels{{"operation", ""}, {"error", ""}});

Histogram& redisOperationDurationSeconds = Registry::instance().registerHistogram(
    "redis_operation_duration_seconds", Labels{{"operation", ""}},
    {0.0005, 0.001, 0.005, 0.01, 0.025, 0.05, 0.1});

Gauge& redisConnectionPool = Registry::instance().registerGauge(
    "redis_connection_pool_size", Labels{});

Counter& userLoginTotal = Registry::instance().registerCounter(
    "user_login_total", Labels{{"status", "success"}});

Counter& userLoginFailedTotal = Registry::instance().registerCounter(
    "user_login_total", Labels{{"status", "failed"}});

Counter& apiCallsTotal = Registry::instance().registerCounter(
    "api_calls_total", Labels{{"endpoint", ""}, {"method", ""}});

Gauge& onlineUsers = Registry::instance().registerGauge(
    "online_users", Labels{});

void initialize() {
    // 初始化默认指标
}

} // namespace DefaultMetrics

} // namespace Metrics
