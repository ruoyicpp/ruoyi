/**
 * @file MetricsCollector.h
 * @brief Prometheus 指标采集器
 */

#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <initializer_list>
#include <utility>

namespace Metrics {

// ── 指标类型 ────────────────────────────────────────────────────────────────

enum class MetricType {
    Counter,    // 计数器，只增不减
    Gauge,      // 仪表盘，可增可减
    Histogram,  // 直方图，分布统计
    Summary     // 摘要，分位数统计
};

// ── 标签 ────────────────────────────────────────────────────────────────────

struct Labels {
    std::map<std::string, std::string> data;

    Labels() = default;

    Labels(const std::map<std::string, std::string>& d) : data(d) {}

    Labels(std::initializer_list<std::pair<std::string, std::string>> init) {
        for (const auto& p : init) {
            data[p.first] = p.second;
        }
    }

    Labels& add(const std::string& k, const std::string& v) {
        data[k] = v;
        return *this;
    }

    std::string toString() const;
};

// ── Counter (计数器) ───────────────────────────────────────────────────────

class Counter {
public:
    void inc(double value = 1.0);
    double get() const { return value_.load(); }

private:
    std::atomic<double> value_{0};
};

// ── Gauge (仪表盘) ─────────────────────────────────────────────────────────

class Gauge {
public:
    void inc(double value = 1.0);
    void dec(double value = 1.0);
    void set(double value);
    double get() const { return value_.load(); }

private:
    std::atomic<double> value_{0};
};

// ── Histogram (直方图) ─────────────────────────────────────────────────────

class Histogram {
public:
    struct Bucket {
        double bound;
        std::atomic<uint64_t> count{0};

        Bucket(double b = 0.0) : bound(b), count(0) {}

        Bucket(const Bucket& other) : bound(other.bound), count(other.count.load()) {}
        Bucket(Bucket&& other) noexcept : bound(other.bound), count(other.count.load()) {}

        Bucket& operator=(const Bucket& other) {
            if (this != &other) {
                bound = other.bound;
                count.store(other.count.load());
            }
            return *this;
        }

        Bucket& operator=(Bucket&& other) noexcept {
            if (this != &other) {
                bound = other.bound;
                count.store(other.count.load());
            }
            return *this;
        }
    };

    explicit Histogram(const std::vector<double>& buckets = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10});

    void observe(double value);
    uint64_t count() const { return count_.load(); }
    double sum() const { return sum_.load(); }

    const std::vector<Bucket>& getBuckets() const { return buckets_; }

private:
    std::vector<Bucket> buckets_;
    std::atomic<uint64_t> count_{0};
    std::atomic<double> sum_{0};
};

// ── 指标注册表 ─────────────────────────────────────────────────────────────

class Registry {
public:
    static Registry& instance();

    // 注册指标
    Counter& registerCounter(const std::string& name, const Labels& labels = {});
    Gauge& registerGauge(const std::string& name, const Labels& labels = {});
    Histogram& registerHistogram(const std::string& name, const Labels& labels = {},
                                 const std::vector<double>& buckets = {});

    // 获取指标
    Counter* getCounter(const std::string& name, const Labels& labels);
    Gauge* getGauge(const std::string& name, const Labels& labels);
    Histogram* getHistogram(const std::string& name, const Labels& labels);

    // 导出 Prometheus 格式
    std::string exportToPrometheus() const;

    // 清空所有指标
    void clear();

private:
    Registry() = default;

    mutable std::mutex mutex_;
    std::map<std::string, std::map<std::string, Counter>> counters_;
    std::map<std::string, std::map<std::string, Gauge>> gauges_;
    std::map<std::string, std::map<std::string, Histogram>> histograms_;
};

// ── 便捷宏 ─────────────────────────────────────────────────────────────────

#define METRIC_COUNTER(name, ...) \
    Metrics::Registry::instance().registerCounter(name, ##__VA_ARGS__)

#define METRIC_GAUGE(name, ...) \
    Metrics::Registry::instance().registerGauge(name, ##__VA_ARGS__)

#define METRIC_HISTOGRAM(name, ...) \
    Metrics::Registry::instance().registerHistogram(name, ##__VA_ARGS__)

// ── 常用指标 ────────────────────────────────────────────────────────────────

namespace DefaultMetrics {
    // HTTP 请求
    extern Counter& httpRequestsTotal;
    extern Counter& httpRequestErrorsTotal;
    extern Histogram& httpRequestDurationSeconds;
    extern Gauge& httpRequestsInFlight;

    // 数据库
    extern Counter& dbQueryTotal;
    extern Counter& dbQueryErrorsTotal;
    extern Histogram& dbQueryDurationSeconds;

    // Redis
    extern Counter& redisOperationsTotal;
    extern Counter& redisErrorsTotal;
    extern Histogram& redisOperationDurationSeconds;
    extern Gauge& redisConnectionPool;

    // 业务指标
    extern Counter& userLoginTotal;
    extern Counter& userLoginFailedTotal;
    extern Counter& apiCallsTotal;
    extern Gauge& onlineUsers;

    void initialize();
}

} // namespace Metrics
