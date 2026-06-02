#pragma once

// ════════════════════════════════════════════════════════════════════════════
// Performance.h — 性能优化工具
//
// 功能：
//   - Prepared Statement 缓存
//   - 读写锁（多读单写）
//   - 对象池
//   - 连接池管理
//   - 性能指标收集
// ════════════════════════════════════════════════════════════════════════════

#include <shared_mutex>
#include <mutex>
#include <vector>
#include <queue>
#include <functional>
#include <memory>
#include <optional>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <numeric>
#include <iterator>

// ════════════════════════════════════════════════════════════════════════════
// 读写锁（多读单写）
// ════════════════════════════════════════════════════════════════════════════

template<typename T>
class ReadWriteGuard {
public:
    ReadWriteGuard(T& data, std::shared_mutex& mutex, bool write = false)
        : data_(data), lock_(write ? std::unique_lock(mutex) : std::shared_lock(mutex)), write_(write) {}

    T& get() { return data_; }
    const T& get() const { return data_; }
    bool isWrite() const { return write_; }

private:
    T& data_;
    std::conditional_t<true, std::unique_lock<std::shared_mutex>, std::shared_lock<std::shared_mutex>> lock_;
    bool write_;
};

template<typename T>
class ReadWriteLocked {
public:
    ReadWriteLocked(T& data, std::shared_mutex& mutex)
        : data_(data), lock_(mutex) {}

    T& value() { return data_; }
    const T& value() const { return data_; }

private:
    T& data_;
    std::shared_lock<std::shared_mutex> lock_;
};

template<typename T>
class ReadWriteLockedWrite {
public:
    ReadWriteLockedWrite(T& data, std::shared_mutex& mutex)
        : data_(data), lock_(mutex) {}

    T& value() { return data_; }
    const T& value() const { return data_; }

private:
    T& data_;
    std::unique_lock<std::shared_mutex> lock_;
};

// 读写锁包装器
template<typename T>
class RWLock {
public:
    RWLock() = default;
    explicit RWLock(T initial) : data_(std::move(initial)) {}

    // 读操作（多个并发）
    template<typename Func>
    auto read(Func&& func) -> decltype(func(std::declval<const T&>())) {
        std::shared_lock<std::shared_mutex> lk(mutex_);
        return func(data_);
    }

    // 写操作（独占）
    template<typename Func>
    auto write(Func&& func) -> decltype(func(std::declval<T&>())) {
        std::unique_lock<std::shared_mutex> lk(mutex_);
        return func(data_);
    }

    // 获取引用（读）
    ReadWriteLocked<const T> lockRead() const {
        return ReadWriteLocked<const T>(data_, mutex_);
    }

    // 获取引用（写）
    ReadWriteLockedWrite<T> lockWrite() {
        return ReadWriteLockedWrite<T>(data_, mutex_);
    }

    T* get() { return &data_; }
    const T* get() const { return &data_; }

private:
    mutable std::shared_mutex mutex_;
    T data_;
};

// ════════════════════════════════════════════════════════════════════════════
// Prepared Statement 缓存
// ════════════════════════════════════════════════════════════════════════════

// PreparedStatement 接口（与具体DB驱动解耦）
class IPreparedStatement {
public:
    virtual ~IPreparedStatement() = default;
    virtual void* nativeHandle() = 0;
    virtual const std::string& sql() const = 0;
    virtual int paramCount() const = 0;
};

// PreparedStatement 缓存
class PreparedStatementCache {
public:
    static PreparedStatementCache& instance() {
        static PreparedStatementCache cache;
        return cache;
    }

    // 注册prepared statement（由DB驱动调用）
    void registerStatement(const std::string& sql, std::shared_ptr<IPreparedStatement> stmt) {
        std::unique_lock<std::shared_mutex> lk(mutex_);
        cache_[sql] = stmt;
        accessOrder_.push_back(sql);
        trimLocked(lk);
    }

    // 获取prepared statement
    std::shared_ptr<IPreparedStatement> get(const std::string& sql) {
        std::shared_lock<std::shared_mutex> lk(mutex_);
        auto it = cache_.find(sql);
        if (it != cache_.end()) {
            // 更新访问顺序
            lk.unlock();
            std::unique_lock<std::shared_mutex> ulk(mutex_);
            updateAccess(sql);
            return it->second;
        }
        return nullptr;
    }

    // 检查是否已缓存
    bool contains(const std::string& sql) const {
        std::shared_lock<std::shared_mutex> lk(mutex_);
        return cache_.find(sql) != cache_.end();
    }

    // 清空缓存
    void clear() {
        std::unique_lock<std::shared_mutex> lk(mutex_);
        cache_.clear();
        accessOrder_.clear();
    }

    // 缓存统计
    struct Stats {
        size_t count;
        size_t maxSize;
        size_t hitCount;
        size_t missCount;
        double hitRate() const {
            size_t total = hitCount + missCount;
            return total > 0 ? (double)hitCount / total : 0.0;
        }
    };

    Stats stats() const {
        std::shared_lock<std::shared_mutex> lk(mutex_);
        Stats s;
        s.count = cache_.size();
        s.maxSize = maxSize_;
        s.hitCount = hitCount_;
        s.missCount = missCount_;
        return s;
    }

    void setMaxSize(size_t max) { maxSize_ = max; }

private:
    PreparedStatementCache() : maxSize_(500), hitCount_(0), missCount_(0) {}

    void updateAccess(const std::string& sql) {
        auto it = std::find(accessOrder_.begin(), accessOrder_.end(), sql);
        if (it != accessOrder_.end()) {
            accessOrder_.erase(it);
        }
        accessOrder_.push_back(sql);
    }

    void trimLocked(std::unique_lock<std::shared_mutex>&) {
        while (cache_.size() > maxSize_) {
            cache_.erase(accessOrder_.front());
            accessOrder_.erase(accessOrder_.begin());
        }
    }

    size_t maxSize_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<IPreparedStatement>> cache_;
    std::vector<std::string> accessOrder_;
    std::atomic<size_t> hitCount_;
    std::atomic<size_t> missCount_;
};

// ════════════════════════════════════════════════════════════════════════════
// 对象池
// ════════════════════════════════════════════════════════════════════════════

template<typename T, typename... Args>
class ObjectPool {
public:
    ObjectPool(size_t initialSize = 0, size_t maxSize = 100, Args&&... args)
        : maxSize_(maxSize), args_(std::forward<Args>(args)...) {
        pool_.reserve(maxSize);
        for (size_t i = 0; i < initialSize; ++i) {
            pool_.push_back(createObject());
        }
    }

    // 获取对象
    std::shared_ptr<T> acquire() {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!pool_.empty()) {
            auto obj = std::move(pool_.back());
            pool_.pop_back();
            return obj;
        }
        return createObject();
    }

    // 归还对象
    void release(std::shared_ptr<T> obj) {
        if (!obj) return;
        std::lock_guard<std::mutex> lk(mutex_);
        if (pool_.size() < maxSize_) {
            pool_.push_back(std::move(obj));
        }
    }

    // 预热
    void warmup(size_t count) {
        std::lock_guard<std::mutex> lk(mutex_);
        for (size_t i = 0; i < count && pool_.size() < maxSize_; ++i) {
            pool_.push_back(createObject());
        }
    }

    size_t size() const { return pool_.size(); }
    size_t maxSize() const { return maxSize_; }

private:
    std::shared_ptr<T> createObject() {
        if constexpr (sizeof...(Args) > 0) {
            return std::apply([](auto&&... a) {
                return std::make_shared<T>(std::forward<decltype(a)>(a)...);
            }, args_);
        } else {
            return std::make_shared<T>();
        }
    }

    size_t maxSize_;
    std::tuple<Args...> args_;
    std::vector<std::shared_ptr<T>> pool_;
    mutable std::mutex mutex_;
};

// ════════════════════════════════════════════════════════════════════════════
// 连接池（通用）
// ════════════════════════════════════════════════════════════════════════════

template<typename Connection>
class ConnectionPool {
public:
    struct Config {
        size_t minSize = 5;
        size_t maxSize = 50;
        std::chrono::seconds idleTimeout{300};
        std::chrono::seconds maxLifetime{3600};
        std::chrono::seconds acquireTimeout{30};
    };

    ConnectionPool(const Config& config) : config_(config) {}

    // 获取连接
    std::shared_ptr<Connection> acquire() {
        auto deadline = std::chrono::steady_clock::now() + config_.acquireTimeout;

        while (true) {
            std::unique_lock<std::mutex> lk(mutex_);

            if (!available_.empty()) {
                auto conn = std::move(available_.front());
                available_.pop();

                if (isHealthy(conn)) {
                    borrowed_++;
                    return conn;
                }
                // 连接不健康，重新创建
            }

            if (total_ < config_.maxSize) {
                total_++;
                lk.unlock();
                auto conn = createConnection();
                borrowed_++;
                return conn;
            }

            // 等待可用连接
            if (std::chrono::steady_clock::now() >= deadline) {
                return nullptr;
            }

            cv_.wait_until(lk, deadline);
        }
    }

    // 归还连接
    void release(std::shared_ptr<Connection> conn) {
        if (!conn) return;
        std::lock_guard<std::mutex> lk(mutex_);
        borrowed_--;

        if (isHealthy(conn) && total_ <= config_.maxSize) {
            available_.push(std::move(conn));
        } else {
            total_--;
        }
        cv_.notify_one();
    }

    // 关闭池
    void close() {
        std::lock_guard<std::mutex> lk(mutex_);
        while (!available_.empty()) {
            available_.pop();
        }
        total_ = 0;
    }

    // 统计
    struct Stats {
        size_t total;
        size_t available;
        size_t borrowed;
        size_t maxSize;
    };

    Stats stats() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return {total_, available_.size(), borrowed_, config_.maxSize};
    }

protected:
    virtual Connection createConnection() = 0;
    virtual bool isHealthy(const std::shared_ptr<Connection>&) = 0;

    Config config_;
    std::queue<std::shared_ptr<Connection>> available_;
    size_t total_ = 0;
    std::atomic<size_t> borrowed_{0};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

// ════════════════════════════════════════════════════════════════════════════
// 性能指标
// ════════════════════════════════════════════════════════════════════════════

class PerformanceMetrics {
public:
    static PerformanceMetrics& instance() {
        static PerformanceMetrics m;
        return m;
    }

    // 计数器
    void incrementCounter(const std::string& name, int64_t delta = 1) {
        counters_[name].fetch_add(delta);
    }

    void setCounter(const std::string& name, int64_t value) {
        counters_[name].store(value);
    }

    int64_t getCounter(const std::string& name) const {
        auto it = counters_.find(name);
        return it != counters_.end() ? it->second.load() : 0;
    }

    // 仪表（瞬时值）
    void setGauge(const std::string& name, double value) {
        gauges_[name].store(value);
    }

    double getGauge(const std::string& name) const {
        auto it = gauges_.find(name);
        return it != gauges_.end() ? it->second.load() : 0.0;
    }

    // 直方图（耗时分布）
    void observeHistogram(const std::string& name, double value) {
        auto& hist = histograms_[name];
        std::lock_guard<std::mutex> lk(hist.mutex);
        hist.values.push_back(value);
        hist.sum += value;
        hist.count++;
        if (hist.values.size() > 1000) {
            hist.values.erase(hist.values.begin());
        }
    }

    struct HistogramStats {
        double min, max, avg, p50, p95, p99;
        size_t count;
    };

    HistogramStats getHistogramStats(const std::string& name) const {
        HistogramStats stats{};
        auto it = histograms_.find(name);
        if (it == histograms_.end()) return stats;

        std::lock_guard<std::mutex> lk(it->second.mutex);
        if (it->second.values.empty()) return stats;

        auto& vals = it->second.values;
        std::vector<double> sorted(vals);
        std::sort(sorted.begin(), sorted.end());

        stats.count = sorted.size();
        stats.min = sorted.front();
        stats.max = sorted.back();
        stats.avg = it->second.sum / stats.count;
        stats.p50 = sorted[stats.count * 0.5];
        stats.p95 = sorted[stats.count * 0.95];
        stats.p99 = sorted[stats.count * 0.99];
        return stats;
    }

    // 计时器（自动RAII）
    class Timer {
    public:
        Timer(const std::string& name, PerformanceMetrics& metrics)
            : name_(name), metrics_(metrics), start_(std::chrono::steady_clock::now()) {}

        ~Timer() {
            auto elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start_).count();
            metrics_.observeHistogram(name_, elapsed);
        }

    private:
        std::string name_;
        PerformanceMetrics& metrics_;
        std::chrono::steady_clock::time_point start_;
    };

    // 获取JSON格式的指标
    std::string toJson() const;

private:
    PerformanceMetrics() = default;

    std::unordered_map<std::string, std::atomic<int64_t>> counters_;
    std::unordered_map<std::string, std::atomic<double>> gauges_;

    struct HistogramData {
        mutable std::mutex mutex;
        std::vector<double> values;
        double sum = 0;
        size_t count = 0;
    };
    std::unordered_map<std::string, HistogramData> histograms_;
};

// 便捷宏
#define PERF_TIMER(name) \
    performance::PerformanceMetrics::Timer _timer##__LINE__(name, performance::PerformanceMetrics::instance())

#define PERF_COUNTER_INC(name) \
    performance::PerformanceMetrics::instance().incrementCounter(name)

#define PERF_GAUGE_SET(name, value) \
    performance::PerformanceMetrics::instance().setGauge(name, value)

// ════════════════════════════════════════════════════════════════════════════
// 并发限制器（令牌桶）
// ════════════════════════════════════════════════════════════════════════════

class RateLimiterTokenBucket {
public:
    RateLimiterTokenBucket(double rate, double capacity)
        : rate_(rate), capacity_(capacity), tokens_(capacity) {}

    // 尝试获取令牌
    bool tryAcquire(int count = 1) {
        std::lock_guard<std::mutex> lk(mutex_);
        refill();

        if (tokens_ >= count) {
            tokens_ -= count;
            return true;
        }
        return false;
    }

    // 阻塞获取令牌
    bool acquire(int count = 1, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true) {
            {
                std::lock_guard<std::mutex> lk(mutex_);
                refill();

                if (tokens_ >= count) {
                    tokens_ -= count;
                    return true;
                }
            }

            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // 获取当前令牌数
    double available() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return tokens_;
    }

private:
    void refill() {
        auto now = std::chrono::steady_clock::now();
        if (lastRefill_.time_since_epoch().count() == 0) {
            lastRefill_ = now;
            return;
        }

        auto elapsed = std::chrono::duration<double>(now - lastRefill_).count();
        tokens_ = std::min(capacity_, tokens_ + elapsed * rate_);
        lastRefill_ = now;
    }

    double rate_;
    double capacity_;
    double tokens_;
    std::chrono::steady_clock::time_point lastRefill_;
    mutable std::mutex mutex_;
};

// ════════════════════════════════════════════════════════════════════════════
// 批量处理
// ════════════════════════════════════════════════════════════════════════════

template<typename T>
class BatchProcessor {
public:
    using Handler = std::function<void(const std::vector<T>&)>;

    BatchProcessor(Handler handler, size_t batchSize, std::chrono::milliseconds flushInterval)
        : handler_(std::move(handler)),
          batchSize_(batchSize),
          flushInterval_(flushInterval),
          running_(true),
          worker_([this] { workerLoop(); }) {}

    ~BatchProcessor() {
        running_ = false;
        if (worker_.joinable()) worker_.join();
        flush();
    }

    void add(T item) {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            batch_.push_back(std::move(item));
        }
        cv_.notify_one();
    }

    void add(std::vector<T> items) {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            batch_.insert(batch_.end(),
                        std::make_move_iterator(items.begin()),
                        std::make_move_iterator(items.end()));
        }
        cv_.notify_one();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return batch_.size();
    }

private:
    void workerLoop() {
        while (running_) {
            std::vector<T> toProcess;
            {
                std::unique_lock<std::mutex> lk(mutex_);
                cv_.wait_for(lk, flushInterval_, [this] {
                    return !batch_.empty() || !running_;
                });

                if (!batch_.empty()) {
                    toProcess = std::move(batch_);
                    batch_.clear();
                }
            }

            if (!toProcess.empty()) {
                handler_(toProcess);
            }
        }
    }

    void flush() {
        std::vector<T> toProcess;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (!batch_.empty()) {
                toProcess = std::move(batch_);
                batch_.clear();
            }
        }
        if (!toProcess.empty()) {
            handler_(toProcess);
        }
    }

    Handler handler_;
    size_t batchSize_;
    std::chrono::milliseconds flushInterval_;
    std::vector<T> batch_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::thread worker_;
};

// 性能相关命名空间
namespace performance {
    using PerformanceMetrics = ::PerformanceMetrics;
}
