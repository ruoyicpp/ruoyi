// =============================================================
// WorkerOrchestrator.h — 多进程编排器
//
// 当 config.json -> "app.worker_processes" > 1 时启用。
// 主进程模式：
//   - 不监听业务端口
//   - fork/CreateProcess N 个子进程（每个带 --worker-index k 参数）
//   - 用 JobObject 绑定，父进程退出时子进程自动被 kill
//   - watchdog 监控子进程，崩溃自动重启（背退指数）
//   - 收到 Ctrl+C 时优雅停所有子进程
//
// 子进程模式：
//   - 检测到环境变量 RUOYI_WORKER_INDEX 时不进入编排器
//   - 直接走正常 drogon 启动流程
//   - 依赖 app.reuse_port=true（drogon 走 SO_REUSEPORT）
//
// 与 NginxEmbedded 配合：
//   - 只有 worker-index=0 的子进程才启动 nginx（避免冲突）
//   - 或 nginx 配 reuseport（每个子进程都启 nginx 也行）
// =============================================================
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

class WorkerOrchestrator {
public:
    struct Config {
        bool enabled        = false;
        int  workerCount    = 1;        // worker 进程数（=1 等同禁用）
        int  maxRestarts    = 10;       // 每个 worker 累计崩溃重启上限
        int  restartBackoffMs = 2000;   // 单次重启间隔
        int  watchIntervalMs  = 1000;   // 监控周期
        std::string exePath;            // 子进程 exe 路径（默认本进程）
        std::vector<std::string> extraArgs;  // 透传给子进程的额外参数
    };

    static WorkerOrchestrator& instance();

    // 返回当前进程的 worker 索引：
    //   - 主进程（编排器）= -1
    //   - 子进程（worker） = 0..N-1
    static int currentWorkerIndex();

    // 主进程入口：启动所有 worker，进入监控循环；阻塞直到收到退出信号
    // 返回值即可作为 main() 的退出码
    int run(const Config& cfg);

    // 触发优雅退出（被信号处理程序或退出钩子调用）
    void requestShutdown();

private:
    WorkerOrchestrator();
    ~WorkerOrchestrator();
    WorkerOrchestrator(const WorkerOrchestrator&) = delete;
    WorkerOrchestrator& operator=(const WorkerOrchestrator&) = delete;

    struct WorkerSlot {
        int   index           = -1;
        int   restartCount    = 0;
        std::chrono::steady_clock::time_point lastStartedAt{};
#ifdef _WIN32
        HANDLE hProc = nullptr;
        HANDLE hStopEvent = nullptr;  // 命名事件：通知子进程优雅退出
        DWORD  pid   = 0;
#else
        int pid = 0;
#endif
        bool   isAlive() const;
    };

    // 子进程调用：监听父进程发来的退出事件，触发时调用 callback
    static void watchStopEvent(std::function<void()> callback);

    bool spawnWorker(WorkerSlot& slot);
    bool killWorker(WorkerSlot& slot, bool graceful, int waitMs);
    void watchdogLoop();

    Config              cfg_;
    std::vector<WorkerSlot> slots_;
    std::atomic<bool>   stopRequested_{false};
    std::mutex          mu_;

#ifdef _WIN32
    HANDLE              hJob_ = nullptr;
    static std::string stopEventName(DWORD pid);
#endif
};
