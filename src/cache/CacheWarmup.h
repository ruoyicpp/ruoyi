#pragma once

#include "CacheStrategy.h"

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Cache {

struct WarmupConfig {
    bool enabled = false;
    bool onStartup = true;
    int intervalSeconds = 3600;
    int batchSize = 100;
    int timeoutSeconds = 30;
    std::vector<std::string> priorityKeys;
};

struct WarmupTask {
    std::string key;
    std::function<CacheValue()> loader;
    int ttlSeconds = 0;
};

class CacheWarmup {
public:
    static CacheWarmup& instance();

    void init(const WarmupConfig& config);
    void shutdown();

    void addTask(const std::string& key,
                 const std::function<CacheValue()>& loader,
                 int ttlSeconds = 0);

    void addTasks(const std::vector<WarmupTask>& tasks);

    void run();
    void runAsync();

    size_t taskCount() const;
    size_t completedCount() const;
    size_t failedCount() const;
    double progress() const;

    bool isRunning() const { return running_.load(); }

private:
    CacheWarmup() = default;
    ~CacheWarmup() = default;
    CacheWarmup(const CacheWarmup&) = delete;
    CacheWarmup& operator=(const CacheWarmup&) = delete;

    void workerLoop();
    void executeTask(const WarmupTask& task);
    void prioritizeTasks(std::vector<WarmupTask>& tasks) const;

    WarmupConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::thread workerThread_;

    mutable std::mutex taskMutex_;
    std::vector<WarmupTask> pendingTasks_;
    std::vector<std::string> completedKeys_;
    std::atomic<size_t> completedCount_{0};
    std::atomic<size_t> failedCount_{0};
};

} // namespace Cache
