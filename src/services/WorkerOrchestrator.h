/**
 * @file WorkerOrchestrator.h
 * @brief 多进程编排器 — 支持多进程负载均衡和进程管理
 * 
 * 功能概述：
 *   - 多进程支持：支持启动多个 worker 进程
 *   - 进程监控：watchdog 监控子进程，崩溃自动重启
 *   - 优雅退出：收到信号时优雅停止所有子进程
 *   - 跨平台支持：支持 Linux（fork）和 Windows（CreateProcess）
 *   - 端口复用：支持 SO_REUSEPORT 实现多进程共享端口
 * 
 * 工作模式：
 * 
 * 主进程模式（编排器）：
 *   - 不监听业务端口
 *   - fork/CreateProcess N 个子进程（每个带 --worker-index k 参数）
 *   - 用 JobObject 绑定，父进程退出时子进程自动被 kill
 *   - watchdog 监控子进程，崩溃自动重启（指数退避）
 *   - 收到 Ctrl+C 时优雅停所有子进程
 * 
 * 子进程模式（worker）：
 *   - 检测到环境变量 RUOYI_WORKER_INDEX 时不进入编排器
 *   - 直接走正常 drogon 启动流程
 *   - 依赖 app.reuse_port=true（drogon 走 SO_REUSEPORT）
 * 
 * 与 NginxEmbedded 配合：
 *   - 只有 worker-index=0 的子进程才启动 nginx（避免冲突）
 *   - 或 nginx 配 reuseport（每个子进程都启 nginx 也行）
 * 
 * 启用条件：
 *   - config.json -> "app.worker_processes" > 1
 *   - 或通过 Config 结构体配置
 * 
 * 配置示例（config.json）：
 *   {
 *     "app": {
 *       "worker_processes": 4,
 *       "reuse_port": true
 *     }
 *   }
 * 
 * @see DatabaseService - 数据库服务
 * @see NginxManager - Nginx 管理器
 */

#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @class WorkerOrchestrator
 * @brief 多进程编排器单例
 * 
 * 管理多个 worker 进程的启动、监控和退出。
 * 采用单例模式，全局唯一实例。
 */
class WorkerOrchestrator {
public:
    /**
     * @struct Config
     * @brief 编排器配置
     */
    struct Config {
        bool enabled        = false;                    ///< 是否启用多进程模式
        int  workerCount    = 1;                        ///< worker 进程数（=1 等同禁用）
        int  maxRestarts    = 10;                       ///< 每个 worker 累计崩溃重启上限
        int  restartBackoffMs = 2000;                   ///< 单次重启间隔（毫秒）
        int  watchIntervalMs  = 1000;                   ///< 监控周期（毫秒）
        std::string exePath;                            ///< 子进程 exe 路径（默认本进程）
        std::vector<std::string> extraArgs;             ///< 透传给子进程的额外参数
    };

    /**
     * @brief 获取单例实例
     * @return WorkerOrchestrator 单例引用
     */
    static WorkerOrchestrator& instance();

    /**
     * @brief 获取当前进程的 worker 索引
     * 
     * @return 
     *   - -1：主进程（编排器）
     *   - 0..N-1：子进程（worker）
     */
    static int currentWorkerIndex();

    /**
     * @brief 主进程入口
     * 
     * 启动所有 worker，进入监控循环。
     * 此函数会阻塞直到收到退出信号。
     * 
     * 流程：
     *   1. 验证配置（workerCount > 1）
     *   2. 创建 worker 进程槽位
     *   3. 启动所有 worker 子进程
     *   4. 进入 watchdog 监控循环
     *   5. 监控子进程健康状态，崩溃时自动重启
     *   6. 收到退出信号时优雅停止所有子进程
     * 
     * @param cfg 编排器配置
     * @return 退出码（可作为 main() 的返回值）
     */
    int run(const Config& cfg);

    /**
     * @brief 触发优雅退出
     * 
     * 被信号处理程序或退出钩子调用。
     * 会通知所有子进程优雅退出。
     */
    void requestShutdown();

private:
    WorkerOrchestrator();
    ~WorkerOrchestrator();
    WorkerOrchestrator(const WorkerOrchestrator&) = delete;
    WorkerOrchestrator& operator=(const WorkerOrchestrator&) = delete;

    /**
     * @struct WorkerSlot
     * @brief Worker 进程槽位
     * 
     * 记录单个 worker 进程的信息和状态。
     */
    struct WorkerSlot {
        int   index           = -1;                     ///< Worker 索引（0..N-1）
        int   restartCount    = 0;                      ///< 重启次数
        std::chrono::steady_clock::time_point lastStartedAt{};  ///< 最后启动时间
        
#ifdef _WIN32
        HANDLE hProc = nullptr;                         ///< 进程句柄
        HANDLE hStopEvent = nullptr;                    ///< 命名事件：通知子进程优雅退出
        DWORD  pid   = 0;                               ///< 进程 ID
#else
        int pid = 0;                                    ///< 进程 ID
#endif
        
        /**
         * @brief 检查进程是否活跃
         * @return 是否活跃
         */
        bool   isAlive() const;
    };

    /**
     * @brief 子进程调用：监听父进程发来的退出事件
     * 
     * 在子进程中调用，监听来自父进程的退出信号。
     * 当父进程发送退出事件时，调用 callback。
     * 
     * @param callback 退出时的回调函数
     */
    static void watchStopEvent(std::function<void()> callback);

    /**
     * @brief 启动 worker 子进程
     * 
     * @param slot Worker 槽位
     * @return 是否启动成功
     */
    bool spawnWorker(WorkerSlot& slot);

    /**
     * @brief 停止 worker 子进程
     * 
     * @param slot Worker 槽位
     * @param graceful 是否优雅停止
     * @param waitMs 等待时间（毫秒）
     * @return 是否停止成功
     */
    bool killWorker(WorkerSlot& slot, bool graceful, int waitMs);

    /**
     * @brief Watchdog 监控循环
     * 
     * 在主进程中运行，监控所有子进程的健康状态。
     * 如果子进程崩溃，自动重启（受 maxRestarts 限制）。
     */
    void watchdogLoop();

    Config              cfg_;                           ///< 编排器配置
    std::vector<WorkerSlot> slots_;                     ///< Worker 槽位列表
    std::atomic<bool>   stopRequested_{false};          ///< 是否请求停止
    std::mutex          mu_;                            ///< 互斥锁

#ifdef _WIN32
    HANDLE              hJob_ = nullptr;                ///< Windows Job 对象
    
    /**
     * @brief 生成停止事件名称
     * @param pid 进程 ID
     * @return 事件名称
     */
    static std::string stopEventName(DWORD pid);
#endif
};
