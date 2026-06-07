#include "TaskQueue.h"
#include "../common/TokenCache.h"
#include <ctime>
#include <iomanip>
#include <sstream>

void TaskQueue::init(const Json::Value& cfg) {
    if (!cfg.isMember("taskQueue")) {
        LOG_WARN << "[TaskQueue] No taskQueue config found, disabled";
        return;
    }

    auto& tc = cfg["taskQueue"];
    enabled_ = tc.get("enabled", false).asBool();
    workerCount_ = tc.get("workers", 4).asInt();
    pollInterval_ = tc.get("pollInterval", 1000).asInt();
    maxRetries_ = tc.get("maxRetries", 3).asInt();
    retryBackoff_ = tc.get("retryBackoff", 60).asInt();
    taskTimeout_ = tc.get("taskTimeout", 300000).asInt();

    if (tc.isMember("queueTypes") && tc["queueTypes"].isArray()) {
        for (auto& qt : tc["queueTypes"]) {
            queueTypes_.push_back(qt.asString());
        }
    }

    if (enabled_) {
        LOG_INFO << "[TaskQueue] Initialized: workers=" << workerCount_
                 << " pollInterval=" << pollInterval_ << "ms"
                 << " maxRetries=" << maxRetries_;
    }
}

bool TaskQueue::enqueue(const Task& task) {
    if (!enabled_) {
        LOG_WARN << "[TaskQueue] Task queue disabled, task not enqueued: " << task.id;
        return false;
    }

    try {
        // Generate task ID if not provided
        Task t = task;
        if (t.id.empty()) {
            t.id = generateTaskId(t.type);
        }
        if (t.createdAt == 0) {
            t.createdAt = std::time(nullptr);
        }
        t.status = TaskStatus::PENDING;

        // Store task details in Redis hash
        auto taskKey = getTaskKey(t.id);
        auto taskJson = t.toJson();
        
        // In real implementation, use Redis:
        // redis->hset(taskKey, "data", Json::writeString(builder, taskJson));
        // redis->expire(taskKey, 86400);  // 24 hours TTL
        
        // Push to queue
        auto queueKey = getQueueKey(t.type);
        // redis->lpush(queueKey, t.id);
        
        LOG_INFO << "[TaskQueue] Task enqueued: id=" << t.id << " type=" << t.type;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "[TaskQueue] Failed to enqueue task: " << e.what();
        return false;
    }
}

bool TaskQueue::enqueueBatch(const std::vector<Task>& tasks) {
    bool allSuccess = true;
    for (const auto& task : tasks) {
        if (!enqueue(task)) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

TaskQueue::TaskStatus TaskQueue::getStatus(const std::string& taskId) {
    try {
        auto taskKey = getTaskKey(taskId);
        // In real implementation, fetch from Redis:
        // auto data = redis->hget(taskKey, "data");
        // Task t = Task::fromJson(parseJson(data));
        // return t.status;
        return TaskStatus::PENDING;
    } catch (...) {
        return TaskStatus::PENDING;
    }
}

TaskQueue::Task TaskQueue::getTask(const std::string& taskId) {
    Task t;
    try {
        auto taskKey = getTaskKey(taskId);
        // In real implementation, fetch from Redis:
        // auto data = redis->hget(taskKey, "data");
        // t = Task::fromJson(parseJson(data));
    } catch (...) {}
    return t;
}

std::vector<std::string> TaskQueue::getTaskLog(const std::string& taskId) {
    std::vector<std::string> logs;
    try {
        auto logKey = getTaskLogKey(taskId);
        // In real implementation, fetch from Redis:
        // logs = redis->lrange(logKey, 0, -1);
    } catch (...) {}
    return logs;
}

void TaskQueue::start(int workerCount) {
    if (!enabled_) {
        LOG_WARN << "[TaskQueue] Task queue disabled, not starting workers";
        return;
    }

    if (running_) {
        LOG_WARN << "[TaskQueue] Workers already running";
        return;
    }

    if (workerCount <= 0) {
        workerCount = workerCount_;
    }

    running_ = true;
    std::lock_guard<std::mutex> lk(workerMu_);

    for (int i = 0; i < workerCount; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }

    LOG_INFO << "[TaskQueue] Started " << workerCount << " worker threads";
}

void TaskQueue::stop() {
    if (!running_) return;

    running_ = false;
    
    // Wait for all workers to finish
    {
        std::lock_guard<std::mutex> lk(workerMu_);
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        workers_.clear();
    }

    LOG_INFO << "[TaskQueue] All worker threads stopped";
}

void TaskQueue::workerLoop() {
    while (running_) {
        try {
            // Process tasks from each queue type
            for (const auto& type : queueTypes_) {
                auto queueKey = getQueueKey(type);
                
                // In real implementation, fetch from Redis:
                // auto taskId = redis->rpop(queueKey);
                // if (taskId.empty()) continue;
                
                // Task t = getTask(taskId);
                // processTask(t);
            }

            // Check retry queue
            for (const auto& type : queueTypes_) {
                auto retryKey = getRetryQueueKey(type);
                // In real implementation:
                // auto taskIds = redis->zrangebyscore(retryKey, 0, now);
                // for (const auto& taskId : taskIds) {
                //     Task t = getTask(taskId);
                //     processTask(t);
                // }
            }

            // Sleep before next poll
            std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval_));
        } catch (const std::exception& e) {
            LOG_ERROR << "[TaskQueue] Worker error: " << e.what();
            std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval_));
        }
    }
}

bool TaskQueue::processTask(const Task& task) {
    try {
        // Find handler
        std::lock_guard<std::mutex> lk(handlerMu_);
        auto it = handlers_.find(task.type);
        if (it == handlers_.end()) {
            LOG_WARN << "[TaskQueue] No handler for task type: " << task.type;
            return false;
        }

        // Update status to PROCESSING
        logTaskExecution(task.id, "Processing started");

        // Execute handler
        bool success = it->second(task);

        if (success) {
            logTaskExecution(task.id, "Processing completed successfully");
            LOG_INFO << "[TaskQueue] Task completed: id=" << task.id;
            return true;
        } else {
            logTaskExecution(task.id, "Processing failed, will retry");
            LOG_WARN << "[TaskQueue] Task failed: id=" << task.id;
            retryTask(task);
            return false;
        }
    } catch (const std::exception& e) {
        logTaskExecution(task.id, std::string("Exception: ") + e.what());
        LOG_ERROR << "[TaskQueue] Task exception: id=" << task.id << " error=" << e.what();
        retryTask(task);
        return false;
    }
}

void TaskQueue::retryTask(const Task& task) {
    try {
        Task t = task;
        t.retries++;

        if (t.retries >= t.maxRetries) {
            moveToDeadLetter(t);
            return;
        }

        // Calculate backoff: exponential backoff with jitter
        long long retryTime = std::time(nullptr) + (retryBackoff_ * (1 << t.retries));

        auto retryKey = getRetryQueueKey(t.type);
        // In real implementation:
        // redis->zadd(retryKey, retryTime, t.id);
        // redis->hset(getTaskKey(t.id), "data", Json::writeString(builder, t.toJson()));

        LOG_INFO << "[TaskQueue] Task moved to retry queue: id=" << t.id
                 << " retries=" << t.retries << " nextRetry=" << retryTime;
    } catch (const std::exception& e) {
        LOG_ERROR << "[TaskQueue] Failed to retry task: " << e.what();
    }
}

void TaskQueue::moveToDeadLetter(const Task& task) {
    try {
        Task t = task;
        t.status = TaskStatus::DEAD;
        t.error = "Exceeded maximum retry attempts";

        auto dlKey = getDeadLetterKey(t.type);
        // In real implementation:
        // redis->lpush(dlKey, t.id);
        // redis->hset(getTaskKey(t.id), "data", Json::writeString(builder, t.toJson()));

        logTaskExecution(t.id, "Moved to dead letter queue");
        LOG_ERROR << "[TaskQueue] Task moved to DLQ: id=" << t.id;
    } catch (const std::exception& e) {
        LOG_ERROR << "[TaskQueue] Failed to move task to DLQ: " << e.what();
    }
}

std::string TaskQueue::generateTaskId(const std::string& type) {
    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);
    
    std::ostringstream oss;
    oss << type << "_"
        << std::put_time(tm, "%Y%m%d_%H%M%S") << "_"
        << std::rand() % 10000;
    
    return oss.str();
}

void TaskQueue::logTaskExecution(const std::string& taskId, const std::string& message) {
    try {
        auto logKey = getTaskLogKey(taskId);
        auto now = std::time(nullptr);
        auto tm = std::localtime(&now);
        
        std::ostringstream oss;
        oss << "[" << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "] " << message;
        
        // In real implementation:
        // redis->lpush(logKey, oss.str());
        // redis->ltrim(logKey, 0, 999);  // Keep last 1000 logs
    } catch (...) {}
}

TaskQueue::Stats TaskQueue::getStats(const std::string& type) {
    Stats stats = {0, 0, 0, 0, 0};
    try {
        // In real implementation, fetch from Redis:
        // stats.pendingCount = redis->llen(getQueueKey(type));
        // stats.failedCount = redis->zcard(getRetryQueueKey(type));
        // stats.deadLetterCount = redis->llen(getDeadLetterKey(type));
    } catch (...) {}
    return stats;
}

bool TaskQueue::requeueDeadLetter(const std::string& taskId) {
    try {
        Task t = getTask(taskId);
        if (t.id.empty()) {
            LOG_WARN << "[TaskQueue] Task not found: " << taskId;
            return false;
        }

        t.retries = 0;
        t.status = TaskStatus::PENDING;
        t.error = "";

        // In real implementation:
        // auto dlKey = getDeadLetterKey(t.type);
        // redis->lrem(dlKey, 1, taskId);
        // redis->lpush(getQueueKey(t.type), taskId);
        // redis->hset(getTaskKey(taskId), "data", Json::writeString(builder, t.toJson()));

        LOG_INFO << "[TaskQueue] Task requeued from DLQ: " << taskId;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "[TaskQueue] Failed to requeue task: " << e.what();
        return false;
    }
}

void TaskQueue::clearDeadLetter(const std::string& type) {
    try {
        auto dlKey = getDeadLetterKey(type);
        // In real implementation:
        // redis->del(dlKey);
        LOG_INFO << "[TaskQueue] Dead letter queue cleared: " << type;
    } catch (const std::exception& e) {
        LOG_ERROR << "[TaskQueue] Failed to clear DLQ: " << e.what();
    }
}
