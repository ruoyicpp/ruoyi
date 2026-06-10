/**
 * @file MetricsCollector.h
 * @brief Prometheus 指标采集器 — 支持多种指标类型的性能监控
 * 
 * 功能概述：
 *   - 多指标类型：Counter（计数）、Gauge（仪表）、Histogram（直方图）、Summary（摘要）
 *   - 标签支持：为指标添加标签，支持多维度分析
 *   - 线程安全：使用原子操作和互斥锁保证线程安全
 *   - Prometheus 兼容：输出格式兼容 Prometheus 监控系统
 *   - 性能监控：记录 HTTP 请求、数据库查询、缓存操作等性能指标
 * 
 * 指标类型说明：
 *   - Counter：计数器，只增不减，用于记录总次数
 *   - Gauge：仪表盘，可增可减，用于记录当前值
 *   - Histogram：直方图，分布统计，用于记录响应时间分布
 *   - Summary：摘要，分位数统计，用于计算百分位数
 * 
 * 使用示例：
 *   // 创建计数器
 *   auto& counter = MetricsCollector::instance().counter("http_requests_total",
 *       "HTTP 请求总数", {{"method", "GET"}, {"path", "/api/users"}});
 *   counter.inc();
 *   
 *   // 创建仪表盘
 *   auto& gauge = MetricsCollector::instance().gauge("active_connections",
 *       "活跃连接数");
 *   gauge.set(42);
 *   
 *   // 创建直方图
 *   auto& histogram = MetricsCollector::instance().histogram("http_request_duration_ms",
 *       "HTTP 请求耗时（毫秒）");
 *   histogram.observe(123.45);
 * 
 * @see JobScheduler - 定时任务调度
 * @see Tracer - 链路追踪
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

/**
 * @namespace Metrics
 * @brief 指标采集命名空间
 */
namespace Metrics {

/**
 * @enum MetricType
 * @brief 指标类型枚举
 */
enum class MetricType {
    Counter,    ///< 计数器，只增不减
    Gauge,      ///< 仪表盘，可增可减
    Histogram,  ///< 直方图，分布统计
    Summary     ///< 摘要，分位数统计
};

/**
 * @struct Labels
 * @brief 指标标签
 * 
 * 为指标添加标签，支持多维度分析。
 */
struct Labels {
    std::map<std::string, std::string> data;  ///< 标签数据

    Labels() = default;

    /**
     * @brief 从 map 构造标签
     * @param d 标签 map
     */
    Labels(const std::map<std::string, std::string>& d) : data(d) {}

    /**
     * @brief 从初始化列表构造标签
     * @param init 标签初始化列表
     */
    Labels(std::initializer_list<std::pair<std::string, std::string>> init) {
        for (const auto& p : init) {
            data[p.first] = p.second;
        }
    }

    /**
     * @brief 添加标签
     * @param k 标签键
     * @param v 标签值
     * @return 返回自身，支持链式调用
     */
    Labels& add(const std::string& k, const std::string& v) {
        data[k] = v;
        return *this;
    }

    /**
     * @brief 将标签转换为字符串
     * @return 标签字符串表示
     */
    std::string toString() const;
};

/**
 * @class Counter
 * @brief 计数器指标
 * 
 * 只增不减的计数器，用于记录总次数。
 * 线程安全，使用原子操作。
 */
class Counter {
public:
    /**
     * @brief 增加计数
     * @param value 增加的值（默认 1.0）
     */
    void inc(double value = 1.0);

    /**
     * @brief 获取当前计数值
     * @return 计数值
     */
    double get() const { return value_.load(); }

private:
    std::atomic<double> value_{0};  ///< 原子计数值
};

/**
 * @class Gauge
 * @brief 仪表盘指标
 * 
 * 可增可减的仪表盘，用于记录当前值。
 * 线程安全，使用原子操作。
 */
class Gauge {
public:
    /**
     * @brief 增加值
     * @param value 增加的值（默认 1.0）
     */
    void inc(double value = 1.0);

    /**
     * @brief 减少值
     * @param value 减少的值（默认 1.0）
     */
    void dec(double value = 1.0);

    /**
     * @brief 设置值
     * @param value 新值
     */
    void set(double value);

    /**
     * @brief 获取当前值
     * @return 当前值
     */
    double get() const { return value_.load(); }

private:
    std::atomic<double> value_{0};  ///< 原子值
};

/**
 * @class Histogram
 * @brief 直方图指标
 * 
 * 分布统计指标，用于记录响应时间分布。
 */
class Histogram {
public:
    /**
     * @struct Bucket
     * @brief 直方图桶
     * 
     * 记录落在某个范围内的样本数量。
     */
    struct Bucket {
        double bound;  ///< 桶的上界
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
