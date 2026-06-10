/**
 * @file CacheWarmup.h
 * @brief 缓存预热策略 — 应用启动时预加载常用数据到缓存
 * 
 * 功能概述：
 *   - 应用启动时自动预热缓存
 *   - 定期重新预热缓存（防止过期）
 *   - 支持优先级加载（重要数据优先）
 *   - 批量加载，避免数据库压力
 *   - 异步执行，不阻塞应用启动
 * 
 * 预热策略：
 *   - 启动预热：应用启动时加载配置、字典、菜单等
 *   - 定期预热：每隔一段时间重新加载（默认 1 小时）
 *   - 优先级预热：重要数据优先加载
 *   - 批量加载：分批加载，避免一次性加载过多
 * 
 * 使用场景：
 *   - 应用启动时预加载系统配置
 *   - 预加载字典数据
 *   - 预加载菜单和权限
 *   - 预加载热点数据
 * 
 * @see Cache::CacheStrategy - 缓存策略
 * @see Cache::WarmupConfig - 预热配置
 * @see Cache::WarmupTask - 预热任务
 */

#pragma once

#include "CacheStrategy.h"

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/**
 * @namespace Cache
 * @brief 缓存管理命名空间
 */
namespace Cache {

/**
 * @struct WarmupConfig
 * @brief 缓存预热配置
 * 
 * 配置缓存预热的各项参数，包括启用状态、执行间隔、批量大小等。
 */
struct WarmupConfig {
    bool enabled = false;                          ///< 是否启用缓存预热
    bool onStartup = true;                         ///< 应用启动时是否执行预热
    int intervalSeconds = 3600;                    ///< 定期预热间隔（秒，默认 1 小时）
    int batchSize = 100;                           ///< 批量加载大小（每批加载的任务数）
    int timeoutSeconds = 30;                       ///< 预热超时时间（秒）
    std::vector<std::string> priorityKeys;         ///< 优先级键列表（优先加载这些键）
};

/**
 * @struct WarmupTask
 * @brief 缓存预热任务
 * 
 * 表示一个需要预热的缓存项，包含键、数据加载器和 TTL。
 */
struct WarmupTask {
    std::string key;                               ///< 缓存键
    std::function<CacheValue()> loader;            ///< 数据加载器（返回缓存值）
    int ttlSeconds = 0;                            ///< 缓存 TTL（秒，0 表示使用默认值）
};

/**
 * @class CacheWarmup
 * @brief 缓存预热管理器
 * 
 * 单例模式，管理缓存预热的执行。支持：
 *   - 应用启动时预热
 *   - 定期预热
 *   - 优先级加载
 *   - 异步执行
 *   - 进度追踪
 */
class CacheWarmup {
public:
    /**
     * @brief 获取单例实例
     * @return CacheWarmup 单例引用
     */
    static CacheWarmup& instance();

    /**
     * @brief 初始化预热管理器
     * @param config 预热配置
     */
    void init(const WarmupConfig& config);

    /**
     * @brief 关闭预热管理器
     */
    void shutdown();

    /**
     * @brief 添加单个预热任务
     * @param key 缓存键
     * @param loader 数据加载器
     * @param ttlSeconds 缓存 TTL（秒）
     */
    void addTask(const std::string& key,
                 const std::function<CacheValue()>& loader,
                 int ttlSeconds = 0);

    /**
     * @brief 批量添加预热任务
     * @param tasks 任务列表
     */
    void addTasks(const std::vector<WarmupTask>& tasks);

    /**
     * @brief 同步执行预热（阻塞）
     */
    void run();

    /**
     * @brief 异步执行预热（非阻塞）
     */
    void runAsync();

    /**
     * @brief 获取待执行任务数
     * @return 任务数量
     */
    size_t taskCount() const;

    /**
     * @brief 获取已完成任务数
     * @return 完成的任务数量
     */
    size_t completedCount() const;

    /**
     * @brief 获取失败任务数
     * @return 失败的任务数量
     */
    size_t failedCount() const;

    /**
     * @brief 获取预热进度
     * @return 进度百分比（0-100）
     */
    double progress() const;

    /**
     * @brief 检查预热是否正在运行
     * @return true 如果正在运行，false 否则
     */
    bool isRunning() const { return running_.load(); }

private:
    CacheWarmup() = default;
    ~CacheWarmup() = default;
    CacheWarmup(const CacheWarmup&) = delete;
    CacheWarmup& operator=(const CacheWarmup&) = delete;

    /**
     * @brief 工作线程循环
     */
    void workerLoop();

    /**
     * @brief 执行单个预热任务
     * @param task 预热任务
     */
    void executeTask(const WarmupTask& task);

    /**
     * @brief 对任务进行优先级排序
     * @param tasks 任务列表（会被排序）
     */
    void prioritizeTasks(std::vector<WarmupTask>& tasks) const;

    WarmupConfig config_;                          ///< 预热配置
    std::atomic<bool> running_{false};             ///< 是否正在运行
    std::atomic<bool> stopRequested_{false};       ///< 是否请求停止
    std::thread workerThread_;                     ///< 工作线程

    mutable std::mutex taskMutex_;                 ///< 任务列表互斥锁
    std::vector<WarmupTask> pendingTasks_;         ///< 待执行任务列表
    std::vector<std::string> completedKeys_;       ///< 已完成的键列表
    std::atomic<size_t> completedCount_{0};        ///< 已完成任务计数
    std::atomic<size_t> failedCount_{0};           ///< 失败任务计数
};

} // namespace Cache
