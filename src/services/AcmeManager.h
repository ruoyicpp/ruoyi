/**
 * @file AcmeManager.h
 * @brief ACME 证书管理器 — 调用外部工具实现证书自动续期
 * 
 * 功能概述：
 *   - 证书监控：定期检查证书有效期
 *   - 自动续期：证书即将过期时自动触发续期
 *   - 外部工具集成：调用 win-acme（Windows）或 acme.sh（Linux）
 *   - Nginx 重载：续期成功后自动重载 Nginx
 * 
 * 设计理念：
 *   - 不在进程内实现 ACMEv2 协议（避免引入大量依赖）
 *   - 调用外部成熟工具子进程续期
 *   - 每 N 小时检查一次证书 notAfter
 *   - 证书剩余天数 < renew_days_before 时触发续期
 *   - 续期成功后：复制 cert/key 到 nginx/ssl/ → 触发 NginxEmbedded::reload()
 * 
 * 支持的工具：
 *   - Windows：win-acme（https://github.com/win-acme/win-acme）
 *   - Linux：acme.sh（https://github.com/acmesh-official/acme.sh）
 * 
 * 使用示例：
 *   AcmeManager::Config c;
 *   c.enabled = true;
 *   c.domains = {"example.com", "www.example.com"};
 *   c.email   = "admin@example.com";
 *   c.acmeBin = "win-acme/wacs.exe";
 *   c.certOut = "nginx/ssl/server.crt";
 *   c.keyOut  = "nginx/ssl/server.key";
 *   AcmeManager::instance().start(c);
 * 
 * 配置示例（config.json）：
 *   {
 *     "acme": {
 *       "enabled": true,
 *       "domains": ["example.com", "www.example.com"],
 *       "email": "admin@example.com",
 *       "acme_bin": "win-acme/wacs.exe",
 *       "staging": false,
 *       "renew_days_before": 30,
 *       "check_interval_hours": 24,
 *       "cert_out": "nginx/ssl/server.crt",
 *       "key_out": "nginx/ssl/server.key",
 *       "reload_nginx_after": true,
 *       "work_dir": "win-acme"
 *     }
 *   }
 * 
 * @see NginxManager - Nginx 管理器
 * @see CertManagerDriver - 证书管理驱动
 */

#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <json/json.h>

/**
 * @class AcmeManager
 * @brief ACME 证书管理器单例
 * 
 * 管理 Let's Encrypt 证书的自动申请和续期。
 * 采用单例模式，全局唯一实例。
 */
class AcmeManager {
public:
    /**
     * @struct Config
     * @brief ACME 管理器配置
     */
    struct Config {
        bool        enabled            = false;                    ///< 是否启用 ACME 管理
        std::vector<std::string> domains;                          ///< 域名列表
        std::string email;                                         ///< 联系邮箱
        std::string acmeBin            = "win-acme/wacs.exe";      ///< ACME 工具路径
        bool        staging            = false;                    ///< 是否使用 LE staging 服务器（避免 rate limit）
        int         renewDaysBefore    = 30;                       ///< 证书剩余天数 < N 时触发续期
        int         checkIntervalHours = 24;                       ///< 检查周期（小时）
        std::string certOut            = "nginx/ssl/server.crt";   ///< 证书输出路径
        std::string keyOut             = "nginx/ssl/server.key";   ///< 密钥输出路径
        bool        reloadNginxAfter   = true;                     ///< 续期成功后是否重载 Nginx
        std::string workDir            = "win-acme";               ///< 工作目录（ACME 工具的日志和缓存）

        /**
         * @brief 从 JSON 配置解析
         * @param c JSON 配置对象
         * @return 解析后的配置
         */
        static Config fromJson(const Json::Value& c) {
            Config r;
            r.enabled            = c.get("enabled", false).asBool();
            if (c.isMember("domains"))
                for (auto& d : c["domains"]) r.domains.push_back(d.asString());
            r.email              = c.get("email", "").asString();
            r.acmeBin            = c.get("acme_bin", "win-acme/wacs.exe").asString();
            r.staging            = c.get("staging", false).asBool();
            r.renewDaysBefore    = c.get("renew_days_before", 30).asInt();
            r.checkIntervalHours = c.get("check_interval_hours", 24).asInt();
            r.certOut            = c.get("cert_out", "nginx/ssl/server.crt").asString();
            r.keyOut             = c.get("key_out",  "nginx/ssl/server.key").asString();
            r.reloadNginxAfter   = c.get("reload_nginx_after", true).asBool();
            r.workDir            = c.get("work_dir", "win-acme").asString();
            return r;
        }
    };

    /**
     * @brief 获取单例实例
     * @return AcmeManager 单例引用
     */
    static AcmeManager& instance();

    /**
     * @brief 启动 ACME 管理器
     * 
     * 启动后台监控线程，定期检查证书有效期。
     * 如果 enabled=false，立即返回。
     * 
     * 流程：
     *   1. 验证配置有效性
     *   2. 启动后台监控线程
     *   3. 定期检查证书有效期
     *   4. 当证书剩余天数 < renewDaysBefore 时触发续期
     *   5. 续期成功后重载 Nginx
     * 
     * @param cfg ACME 管理器配置
     * @return 是否启动成功
     */
    bool start(const Config& cfg);
    // 停止调度（join 调度线程）
    void stop();

    // 手动触发一次检查/续期（同步阻塞，可在管理端点调用）
    bool checkAndRenewOnce();

    // 读取证书剩余天数；< 0 表示读取失败
    static int daysUntilExpiry(const std::string& certPath);

    bool isRunning() const { return running_.load(); }

private:
    AcmeManager()  = default;
    ~AcmeManager() { stop(); }
    AcmeManager(const AcmeManager&) = delete;
    AcmeManager& operator=(const AcmeManager&) = delete;

    // 调用 win-acme.exe 续期；返回 true 表示新证书已就位
    bool runAcmeRenew();
    // 调度循环
    void schedulerLoop();

    Config            cfg_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::thread       worker_;
    std::mutex        mu_;
};
