/**
 * @file ExternalServiceManager.h
 * @brief 外部服务管理器 — 通用进程状态监控与反向代理配置
 *
 * 功能概述：
 *   - 进程状态监控：检测独立运行的二进制服务是否存活
 *   - 自动代理路由：根据配置自动生成 nginx_like 路由
 *   - 前端状态展示：提供 API 让前端显示各服务运行状态
 *   - 通用扩展：新增服务只需在 config.json 添加配置
 *
 * 工作流程：
 *   1. 从配置文件加载外部服务列表
 *   2. 定期检测各服务进程状态
 *   3. 自动注册 nginx_like proxy_pass 路由
 *   4. 提供 /api/system/services/status 接口给前端
 *
 * 配置示例（config.json）：
 *   {
 *     "external_services": {
 *       "enabled": true,
 *       "check_interval_ms": 5000,
 *       "services": [
 *         {
 *           "name": "sub2api",
 *           "display_name": "订阅转换",
 *           "exe_name": "sub2api.exe",
 *           "port": 25500,
 *           "path_prefix": "/sub2/",
 *           "upstream": "http://127.0.0.1:25500",
 *           "strip_prefix": true,
 *           "enabled": true
 *         },
 *         {
 *           "name": "ddns-go",
 *           "display_name": "DDNS",
 *           "exe_name": "ddns-go.exe",
 *           "port": 9876,
 *           "path_prefix": "/ddns/",
 *           "upstream": "http://127.0.0.1:9876",
 *           "strip_prefix": true,
 *           "enabled": true
 *         }
 *       ]
 *     }
 *   }
 *
 * API 接口：
 *   GET /api/system/services/status
 *   返回：{ "services": [{ "name": "sub2api", "running": true, "port": 25500, ... }] }
 *
 * nginx_like 路由（自动生成）：
 *   所有 enabled=true 的服务自动注册到 nginx_like.proxy_pass
 *
 * @see NginxLikeFeatures - nginx 风格功能（proxy_pass）
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
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

namespace ruoyi {

/**
 * @struct ExternalServiceConfig
 * @brief 单个外部服务配置
 */
struct ExternalServiceConfig {
    std::string name;         ///< 服务标识名（英文，用于 API）
    std::string displayName;  ///< 显示名称（中文，用于前端）
    std::string exeName;      ///< 进程名（如 sub2api.exe）
    int         port = 0;     ///< 监听端口
    std::string pathPrefix;   ///< 代理路径前缀（如 /sub2/）
    std::string upstream;     ///< 上游地址（默认 http://127.0.0.1:{port}）
    bool        stripPrefix = true;  ///< 代理时是否去除路径前缀
    bool        enabled = true;      ///< 是否启用
};

/**
 * @struct ExternalServiceStatus
 * @brief 单个外部服务状态
 */
struct ExternalServiceStatus {
    std::string name;           ///< 服务标识名
    std::string displayName;    ///< 显示名称
    int         port = 0;       ///< 监听端口
    std::string pathPrefix;     ///< 代理路径前缀
    bool        enabled = true; ///< 是否启用
    bool        running = false; ///< 是否运行中
    int         pid = 0;        ///< 进程 ID（0 表示未找到）
};

/**
 * @class ExternalServiceManager
 * @brief 外部服务管理器单例
 *
 * 管理所有外部服务的状态监控和代理路由。
 */
class ExternalServiceManager {
public:
    static ExternalServiceManager& instance();

    /**
     * @brief 初始化管理器
     * @param services 外部服务配置列表
     */
    void init(const std::vector<ExternalServiceConfig>& services);

    /**
     * @brief 启动监控
     */
    void start();

    /**
     * @brief 停止监控
     */
    void stop();

    /**
     * @brief 获取所有服务状态
     * @return 服务状态列表
     */
    std::vector<ExternalServiceStatus> getAllStatus() const;

    /**
     * @brief 获取单个服务状态
     * @param name 服务标识名
     * @return 服务状态（不存在返回默认状态）
     */
    ExternalServiceStatus getStatus(const std::string& name) const;

    /**
     * @brief 获取启用的服务配置（用于生成 nginx_like 路由）
     * @return 启用的服务配置列表
     */
    std::vector<ExternalServiceConfig> getEnabledServices() const;

    /**
     * @brief 检查是否已初始化
     */
    bool isInited() const { return inited_; }

private:
    ExternalServiceManager();
    ~ExternalServiceManager();
    ExternalServiceManager(const ExternalServiceManager&) = delete;
    ExternalServiceManager& operator=(const ExternalServiceManager&) = delete;

    /**
     * @brief 检查单个进程是否运行
     * @param exeName 进程名
     * @return 进程 ID（0 表示未运行）
     */
    int checkProcess(const std::string& exeName);

    /**
     * @brief 监控线程主函数
     */
    void monitorLoop();

    std::vector<ExternalServiceConfig> configs_;
    std::vector<ExternalServiceStatus> statuses_;
    mutable std::mutex mu_;
    std::atomic<bool> running_{false};
    std::atomic<bool> inited_{false};
    std::thread monThread_;
    int checkIntervalMs_ = 5000;
};

} // namespace ruoyi
