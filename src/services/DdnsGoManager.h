/**
 * @file DdnsGoManager.h
 * @brief DDNS 管理器 — 动态域名解析和 IP 更新
 * 
 * 功能概述：
 *   - 动态域名解析：自动更新域名指向的 IP 地址
 *   - 多 DNS 提供商支持：支持 Aliyun、Cloudflare、DNSPod 等
 *   - Web 管理界面：提供 Web UI 管理 DDNS 配置
 *   - 自动监控：定期检查 IP 变化，自动更新
 *   - 跨平台支持：支持 Windows 和 Linux
 * 
 * ddns-go 说明：
 *   - 开源 DDNS 工具（https://github.com/jeessy2/ddns-go）
 *   - 支持多种 DNS 提供商
 *   - 轻量级，占用资源少
 *   - 可在 NAS、树莓派等设备上运行
 * 
 * 工作流程：
 *   1. 启动 ddns-go 子进程
 *   2. 定期检查公网 IP 地址
 *   3. 如果 IP 变化，自动更新 DNS 记录
 *   4. 支持 Web 管理界面配置
 * 
 * 配置示例（config.json）：
 *   {
 *     \"ddns_go\": {
 *       \"enabled\": true,
 *       \"exe_path\": \"ddns-go/ddns-go.exe\",
 *       \"config_path\": \".ddns_go_config.yaml\",
 *       \"frequency\": 300,
 *       \"listen_addr\": \":9876\",
 *       \"no_web\": false,
 *       \"auto_restart\": true
 *     }
 *   }
 * 
 * @see NginxManager - Nginx 管理器
 */

#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @struct DdnsGoConfig
 * @brief DDNS 管理器配置
 */
struct DdnsGoConfig {
    bool        enabled      = false;                ///< 是否启用 DDNS
    std::string exePath;                            ///< ddns-go 可执行文件路径
    std::string configPath;                         ///< DDNS 配置文件路径
    int         frequency    = 300;                 ///< 更新频率（秒）
    std::string listenAddr   = ":9876";             ///< Web 管理界面监听地址
    bool        noWeb        = false;               ///< 是否禁用 Web 界面
    bool        skipVerify   = false;               ///< 是否跳过 SSL 验证
    bool        autoRestart  = true;                ///< 是否自动重启
    bool        showWindow   = false;               ///< 是否显示窗口（Windows）
};

/**
 * @class DdnsGoManager
 * @brief DDNS 管理器单例
 * 
 * 管理 ddns-go 进程的启动、停止和监控。
 * 采用单例模式，全局唯一实例。
 */
class DdnsGoManager {
public:
    /**
     * @brief 获取单例实例
     * @return DdnsGoManager 单例引用
     */
    static DdnsGoManager& instance();

    /**
     * @brief 启动 DDNS 管理器
     * 
     * 启动 ddns-go 子进程，开始定期更新 DNS 记录。
     * 
     * @param cfg DDNS 管理器配置
     * @return 是否启动成功
     */
    bool start(const DdnsGoConfig& cfg);

    /**
     * @brief 停止 DDNS 管理器
     */
    void stop();

    /**
     * @brief 检查 DDNS 是否运行中
     * @return 是否运行中
     */
    bool isRunning() const;

private:
    DdnsGoManager();
    ~DdnsGoManager();
    DdnsGoManager(const DdnsGoManager&) = delete;
    DdnsGoManager& operator=(const DdnsGoManager&) = delete;

    /**
     * @brief 启动 ddns-go 子进程
     * @return 是否启动成功
     */
    bool spawnProcess();

    /**
     * @brief 启动监控线程
     */
    void startMonitor();

    /**
     * @brief 停止监控线程
     */
    void stopMonitor();

    DdnsGoConfig        cfg_;                       ///< DDNS 配置
    std::atomic<bool>   running_{false};            ///< 是否运行中
    std::atomic<bool>   monRunning_{false};         ///< 监控线程是否运行中
    std::thread         monThread_;                 ///< 监控线程
    mutable std::mutex  mu_;                        ///< 互斥锁

#ifdef _WIN32
    HANDLE hProc_ = nullptr;                        ///< 进程句柄
    HANDLE hJob_  = nullptr;                        ///< Job 对象
    DWORD  pid_   = 0;                              ///< 进程 ID
#else
    pid_t  pid_   = 0;                              ///< 进程 ID
#endif
};
