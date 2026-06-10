/**
 * @file NginxManager.h
 * @brief Nginx 管理器 — 嵌入式 Nginx 进程管理
 * 
 * 功能概述：
 *   - Nginx 启动和停止：管理嵌入式 Nginx 进程
 *   - 进程监控：监控 Nginx 进程状态，崩溃自动重启
 *   - 优雅重载：支持 SIGHUP 信号优雅重载配置
 *   - 跨平台支持：支持 Windows 和 Linux
 * 
 * 工作流程：
 *   1. 初始化配置（exe 路径、前缀、端口等）
 *   2. 启动 Nginx 进程
 *   3. 启动监控线程，定期检查进程状态
 *   4. 如果进程崩溃，自动重启（受 maxRestarts 限制）
 *   5. 停止时优雅关闭 Nginx
 * 
 * 配置示例（config.json）：
 *   {
 *     "nginx": {
 *       "enabled": true,
 *       "exe_path": "nginx/nginx.exe",
 *       "prefix": "nginx/",
 *       "port": 18081,
 *       "auto_restart": true,
 *       "max_restarts": 5
 *     }
 *   }
 * 
 * @see WorkerOrchestrator - 多进程编排器
 * @see AcmeManager - ACME 证书管理器
 */

#pragma once
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

/**
 * @struct NginxConfig
 * @brief Nginx 配置
 */
struct NginxConfig {
    std::string exePath   = "nginx/nginx.exe";     ///< Nginx 可执行文件路径
    std::string prefix    = "nginx/";              ///< Nginx 前缀目录
    int         port      = 18081;                 ///< 监听端口
    bool        enabled   = true;                  ///< 是否启用
    bool        autoRestart  = true;               ///< 是否自动重启
    int         maxRestarts  = 5;                  ///< 最大重启次数
};

/**
 * @enum NginxState
 * @brief Nginx 进程状态
 */
enum class NginxState { 
    Stopped,    ///< 已停止
    Running,    ///< 运行中
    Failed      ///< 失败
};

/**
 * @class NginxManager
 * @brief Nginx 管理器单例
 * 
 * 管理嵌入式 Nginx 进程的启动、停止和监控。
 * 采用单例模式，全局唯一实例。
 */
class NginxManager {
public:
    /**
     * @brief 获取单例实例
     * @return NginxManager 单例引用
     */
    static NginxManager& instance();

    /**
     * @brief 初始化 Nginx 管理器
     * @param cfg Nginx 配置
     */
    void   init(const NginxConfig& cfg);

    /**
     * @brief 启动 Nginx 进程
     * @return 是否启动成功
     */
    bool   start();

    /**
     * @brief 停止 Nginx 进程
     * @param graceful 是否优雅停止（发送 SIGTERM）
     * @return 是否停止成功
     */
    bool   stop(bool graceful = true);

    /**
     * @brief 检查 Nginx 是否运行中
     * @return 是否运行中
     */
    bool   isRunning() const;

    /**
     * @brief 获取 Nginx 状态
     * @return Nginx 状态
     */
    NginxState state() const { return state_; }

private:
    NginxManager();
    ~NginxManager();
    NginxManager(const NginxManager&) = delete;
    NginxManager& operator=(const NginxManager&) = delete;

    /**
     * @brief 发送信号给 Nginx
     * @param sig 信号名称（HUP、TERM、QUIT 等）
     * @return 是否发送成功
     */
    bool   sendSignal(const std::string& sig);

    /**
     * @brief 启动监控线程
     */
    void   startMonitor();

    /**
     * @brief 停止监控线程
     */
    void   stopMonitor();

    NginxConfig         cfg_;                      ///< Nginx 配置
    NginxState          state_ = NginxState::Stopped;  ///< 当前状态
    mutable std::mutex  mu_;                       ///< 互斥锁
    bool                inited_ = false;           ///< 是否已初始化

#ifdef _WIN32
    HANDLE  hProc_ = nullptr;
    HANDLE  hJob_  = nullptr;
    DWORD   pid_   = 0;
#else
    pid_t   pid_   = 0;
#endif

    std::atomic<bool> monRunning_{false};
    std::thread       monThread_;
    std::chrono::system_clock::time_point startTime_;
    int restartCount_ = 0;
};
