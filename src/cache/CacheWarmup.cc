/**
 * @file CacheWarmup.cc
 * @brief 缓存预热策略实现
 * 
 * 实现了缓存预热的核心逻辑，包括：
 *   - 单例模式管理
 *   - 配置初始化和关闭
 *   - 任务添加和执行
 *   - 优先级排序
 *   - 异步执行
 *   - 进度追踪
 */

#include "CacheWarmup.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>

namespace Cache {

/**
 * @brief 获取单例实例
 * @return CacheWarmup 单例引用
 */
CacheWarmup& CacheWarmup::instance() {
    static CacheWarmup warmup;
    return warmup;
}

/**
 * @brief 初始化预热管理器
 * 
 * 流程：
 *   1. 关闭之前的预热（如果正在运行）
 *   2. 加锁保护共享数据
 *   3. 保存配置
 *   4. 重置计数器和任务列表
 *   5. 如果启用且配置为启动时预热，则异步执行
 * 
 * @param config 预热配置
 */
void CacheWarmup::init(const WarmupConfig& config) {
    // 关闭之前的预热
    shutdown();

    // 加锁保护共享数据
    std::lock_guard<std::mutex> lock(taskMutex_);
    
    // 保存配置
    config_ = config;
    
    // 重置状态
    stopRequested_ = false;
    completedCount_ = 0;
    failedCount_ = 0;
    pendingTasks_.clear();
    completedKeys_.clear();

    // 如果启用且配置为启动时预热，则异步执行
    if (config_.enabled && config_.onStartup) {
        runAsync();
    }
}

/**
 * @brief 关闭预热管理器
 * 
 * 流程：
 *   1. 设置停止请求标志
 *   2. 等待工作线程完成
 *   3. 设置运行状态为 false
 */
void CacheWarmup::shutdown() {
    // 设置停止请求标志
    stopRequested_ = true;

    // 等待工作线程完成
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    // 设置运行状态为 false
    running_ = false;
}

/**
 * @brief 添加单个预热任务
 * 
 * 将任务添加到待执行队列。线程安全。
 * 
 * @param key 缓存键
 * @param loader 数据加载器函数
 * @param ttlSeconds 缓存 TTL（秒）
 */
void CacheWarmup::addTask(const std::string& key,
                          const std::function<CacheValue()>& loader,
                          int ttlSeconds) {
    // 加锁保护任务列表
    std::lock_guard<std::mutex> lock(taskMutex_);
    // 添加任务到队列
    pendingTasks_.push_back(WarmupTask{key, loader, ttlSeconds});
}

/**
 * @brief 批量添加预热任务
 * 
 * 将多个任务添加到待执行队列。线程安全。
 * 
 * @param tasks 任务列表
 */
void CacheWarmup::addTasks(const std::vector<WarmupTask>& tasks) {
    // 加锁保护任务列表
    std::lock_guard<std::mutex> lock(taskMutex_);
    // 批量插入任务
    pendingTasks_.insert(pendingTasks_.end(), tasks.begin(), tasks.end());
}

/**
 * @brief 同步执行预热
 * 
 * 流程：
 *   1. 检查是否启用
 *   2. 设置运行状态为 true
 *   3. 交换任务列表（避免长时间持有锁）
 *   4. 优先级排序
 *   5. 分批执行任务
 *   6. 设置运行状态为 false
 */
void CacheWarmup::run() {
    // 检查是否启用
    if (!config_.enabled) {
        return;
    }

    // 设置运行状态
    running_ = true;

    // 交换任务列表，避免长时间持有锁
    std::vector<WarmupTask> tasks;
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        tasks.swap(pendingTasks_);
    }

    // 优先级排序
    prioritizeTasks(tasks);

    // 计算批量大小
    const size_t batchSize = config_.batchSize > 0
        ? static_cast<size_t>(config_.batchSize)
        : static_cast<size_t>(1);

    // 分批执行任务
    for (size_t start = 0; start < tasks.size() && !stopRequested_.load(); start += batchSize) {
        const size_t end = std::min(start + batchSize, tasks.size());
        for (size_t i = start; i < end && !stopRequested_.load(); ++i) {
            executeTask(tasks[i]);
        }
    }

    // 设置运行状态为 false
    running_ = false;
}

/**
 * @brief 异步执行预热
 * 
 * 在后台线程中执行预热，不阻塞主线程。
 * 
 * 流程：
 *   1. 检查是否已在运行
 *   2. 等待之前的线程完成
 *   3. 重置停止标志
 *   4. 创建新的工作线程
 */
void CacheWarmup::runAsync() {
    // 如果已在运行，直接返回
    if (running_.load()) {
        return;
    }

    // 等待之前的线程完成
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    // 重置停止标志
    stopRequested_ = false;
    
    // 创建新的工作线程
    workerThread_ = std::thread([this]() {
        this->workerLoop();
    });
}

/**
 * @brief 获取待执行任务数
 * @return 任务数量
 */
size_t CacheWarmup::taskCount() const {
    // 加锁保护任务列表
    std::lock_guard<std::mutex> lock(taskMutex_);
    return pendingTasks_.size();
}

/**
 * @brief 获取已完成任务数
 * @return 完成的任务数量
 */
size_t CacheWarmup::completedCount() const {
    return completedCount_.load();
}

/**
 * @brief 获取失败任务数
 * @return 失败的任务数量
 */
size_t CacheWarmup::failedCount() const {
    return failedCount_.load();
}

/**
 * @brief 获取预热进度
 * 
 * 计算公式：已完成 / (已完成 + 待执行)
 * 
 * @return 进度百分比（0.0 - 1.0）
 */
double CacheWarmup::progress() const {
    // 获取已完成和待执行的任务数
    const auto completed = completedCount_.load();
    const auto pending = taskCount();
    const auto total = completed + pending;

    // 如果没有任务，返回 0
    if (total == 0) {
        return 0.0;
    }

    // 计算进度百分比
    return static_cast<double>(completed) / static_cast<double>(total);
}

/**
 * @brief 工作线程循环
 * 
 * 流程：
 *   1. 循环执行预热
 *   2. 检查停止标志
 *   3. 等待配置的间隔时间
 *   4. 重复直到收到停止请求
 */
void CacheWarmup::workerLoop() {
    // 循环执行预热
    while (!stopRequested_.load()) {
        // 执行一次预热
        run();

        // 检查停止标志
        if (stopRequested_.load()) {
            break;
        }

        // 等待配置的间隔时间
        const int interval = config_.intervalSeconds > 0 ? config_.intervalSeconds : 1;
        for (int i = 0; i < interval && !stopRequested_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

/**
 * @brief 对任务进行优先级排序
 * 
 * 根据配置的优先级键列表，对任务进行排序。
 * 优先级高的任务（在 priorityKeys 中的任务）会被排到前面。
 * 
 * 排序规则：
 *   1. 在优先级列表中的任务排在前面
 *   2. 优先级列表中的任务按照列表顺序排序
 *   3. 不在优先级列表中的任务排在后面
 * 
 * 使用稳定排序（stable_sort），保持相同优先级的任务相对顺序不变。
 * 
 * @param tasks 任务列表（会被排序）
 */
void CacheWarmup::prioritizeTasks(std::vector<WarmupTask>& tasks) const {
    // 如果任务列表为空或没有优先级配置，直接返回
    if (tasks.empty() || config_.priorityKeys.empty()) {
        return;
    }

    // 构建优先级映射表（键 → 优先级索引）
    std::unordered_map<std::string, size_t> priorityOrder;
    priorityOrder.reserve(config_.priorityKeys.size());
    for (size_t i = 0; i < config_.priorityKeys.size(); ++i) {
        priorityOrder.emplace(config_.priorityKeys[i], i);
    }

    // 使用稳定排序，按照优先级排序任务
    std::stable_sort(tasks.begin(), tasks.end(), [&priorityOrder](const WarmupTask& lhs, const WarmupTask& rhs) {
        // 查找左右任务的优先级
        const auto lhsIt = priorityOrder.find(lhs.key);
        const auto rhsIt = priorityOrder.find(rhs.key);
        
        // 检查是否在优先级列表中
        const bool lhsPriority = lhsIt != priorityOrder.end();
        const bool rhsPriority = rhsIt != priorityOrder.end();

        // 优先级高的任务排在前面
        if (lhsPriority != rhsPriority) {
            return lhsPriority;
        }
        
        // 如果都不在优先级列表中，保持原顺序
        if (!lhsPriority) {
            return false;
        }
        
        // 都在优先级列表中，按照优先级索引排序
        return lhsIt->second < rhsIt->second;
    });
}

/**
 * @brief 执行单个预热任务
 * 
 * 流程：
 *   1. 调用加载器函数获取缓存值
 *   2. 将值存储到缓存中
 *   3. 记录已完成的键（最多 1000 条）
 *   4. 增加完成计数
 *   5. 如果发生异常，增加失败计数
 * 
 * @param task 预热任务
 */
void CacheWarmup::executeTask(const WarmupTask& task) {
    try {
        // 调用加载器函数获取缓存值
        CacheValue value = task.loader ? task.loader() : CacheValue{std::monostate{}};
        
        // 将值存储到缓存中
        CacheStrategy::instance().set(task.key, value, task.ttlSeconds);

        // 记录已完成的键
        {
            std::lock_guard<std::mutex> lock(taskMutex_);
            completedKeys_.push_back(task.key);
            
            // 维护已完成键列表（最多 1000 条，超出时删除前 500 条）
            if (completedKeys_.size() > 1000) {
                completedKeys_.erase(completedKeys_.begin(), completedKeys_.begin() + 500);
            }
        }

        // 增加完成计数（原子操作，无需加锁）
        completedCount_.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
        // 发生异常，增加失败计数
        failedCount_.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace Cache
