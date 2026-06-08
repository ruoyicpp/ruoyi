#include "CacheWarmup.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>

namespace Cache {

CacheWarmup& CacheWarmup::instance() {
    static CacheWarmup warmup;
    return warmup;
}

void CacheWarmup::init(const WarmupConfig& config) {
    shutdown();

    std::lock_guard<std::mutex> lock(taskMutex_);
    config_ = config;
    stopRequested_ = false;
    completedCount_ = 0;
    failedCount_ = 0;
    pendingTasks_.clear();
    completedKeys_.clear();

    if (config_.enabled && config_.onStartup) {
        runAsync();
    }
}

void CacheWarmup::shutdown() {
    stopRequested_ = true;

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    running_ = false;
}

void CacheWarmup::addTask(const std::string& key,
                          const std::function<CacheValue()>& loader,
                          int ttlSeconds) {
    std::lock_guard<std::mutex> lock(taskMutex_);
    pendingTasks_.push_back(WarmupTask{key, loader, ttlSeconds});
}

void CacheWarmup::addTasks(const std::vector<WarmupTask>& tasks) {
    std::lock_guard<std::mutex> lock(taskMutex_);
    pendingTasks_.insert(pendingTasks_.end(), tasks.begin(), tasks.end());
}

void CacheWarmup::run() {
    if (!config_.enabled) {
        return;
    }

    running_ = true;

    std::vector<WarmupTask> tasks;
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        tasks.swap(pendingTasks_);
    }

    prioritizeTasks(tasks);

    const size_t batchSize = config_.batchSize > 0
        ? static_cast<size_t>(config_.batchSize)
        : static_cast<size_t>(1);

    for (size_t start = 0; start < tasks.size() && !stopRequested_.load(); start += batchSize) {
        const size_t end = std::min(start + batchSize, tasks.size());
        for (size_t i = start; i < end && !stopRequested_.load(); ++i) {
            executeTask(tasks[i]);
        }
    }

    running_ = false;
}

void CacheWarmup::runAsync() {
    if (running_.load()) {
        return;
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    stopRequested_ = false;
    workerThread_ = std::thread([this]() {
        this->workerLoop();
    });
}

size_t CacheWarmup::taskCount() const {
    std::lock_guard<std::mutex> lock(taskMutex_);
    return pendingTasks_.size();
}

size_t CacheWarmup::completedCount() const {
    return completedCount_.load();
}

size_t CacheWarmup::failedCount() const {
    return failedCount_.load();
}

double CacheWarmup::progress() const {
    const auto completed = completedCount_.load();
    const auto pending = taskCount();
    const auto total = completed + pending;

    if (total == 0) {
        return 0.0;
    }

    return static_cast<double>(completed) / static_cast<double>(total);
}

void CacheWarmup::workerLoop() {
    while (!stopRequested_.load()) {
        run();

        if (stopRequested_.load()) {
            break;
        }

        const int interval = config_.intervalSeconds > 0 ? config_.intervalSeconds : 1;
        for (int i = 0; i < interval && !stopRequested_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void CacheWarmup::prioritizeTasks(std::vector<WarmupTask>& tasks) const {
    if (tasks.empty() || config_.priorityKeys.empty()) {
        return;
    }

    std::unordered_map<std::string, size_t> priorityOrder;
    priorityOrder.reserve(config_.priorityKeys.size());
    for (size_t i = 0; i < config_.priorityKeys.size(); ++i) {
        priorityOrder.emplace(config_.priorityKeys[i], i);
    }

    std::stable_sort(tasks.begin(), tasks.end(), [&priorityOrder](const WarmupTask& lhs, const WarmupTask& rhs) {
        const auto lhsIt = priorityOrder.find(lhs.key);
        const auto rhsIt = priorityOrder.find(rhs.key);
        const bool lhsPriority = lhsIt != priorityOrder.end();
        const bool rhsPriority = rhsIt != priorityOrder.end();

        if (lhsPriority != rhsPriority) {
            return lhsPriority;
        }
        if (!lhsPriority) {
            return false;
        }
        return lhsIt->second < rhsIt->second;
    });
}

void CacheWarmup::executeTask(const WarmupTask& task) {
    try {
        CacheValue value = task.loader ? task.loader() : CacheValue{std::monostate{}};
        CacheStrategy::instance().set(task.key, value, task.ttlSeconds);

        {
            std::lock_guard<std::mutex> lock(taskMutex_);
            completedKeys_.push_back(task.key);
            if (completedKeys_.size() > 1000) {
                completedKeys_.erase(completedKeys_.begin(), completedKeys_.begin() + 500);
            }
        }

        completedCount_.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
        failedCount_.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace Cache
