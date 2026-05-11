// =============================================================
// AcmeManager.h — ACME 证书自动续期
//
// 策略：
//   - 不在进程内实现 ACMEv2 协议（避免引入大量依赖）
//   - 调外部成熟工具子进程续期：win-acme (Windows) 或 acme.sh (Linux)
//   - 每 N 小时检查一次证书 notAfter；< renew_days_before 天时触发续期
//   - 续期成功后：复制 cert/key 到 nginx/ssl/ → 触发 NginxEmbedded::reload()
//
// 使用：
//   AcmeManager::Config c;
//   c.enabled = true;
//   c.domains = {"example.com", "www.example.com"};
//   c.email   = "admin@example.com";
//   c.acmeBin = "win-acme/wacs.exe";
//   c.certOut = "nginx/ssl/server.crt";
//   c.keyOut  = "nginx/ssl/server.key";
//   AcmeManager::instance().start(c);
//
// 配置 config.json -> "acme":
//   {
//     "enabled": false,
//     "domains": ["example.com"],
//     "email":   "admin@example.com",
//     "acme_bin": "win-acme/wacs.exe",
//     "staging": false,
//     "renew_days_before": 30,
//     "check_interval_hours": 24,
//     "cert_out": "nginx/ssl/server.crt",
//     "key_out":  "nginx/ssl/server.key"
//   }
// =============================================================
#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <json/json.h>

class AcmeManager {
public:
    struct Config {
        bool        enabled            = false;
        std::vector<std::string> domains;
        std::string email;
        std::string acmeBin            = "win-acme/wacs.exe";
        bool        staging            = false;   // staging=true 用 LE staging 服务器（避免 rate limit）
        int         renewDaysBefore    = 30;      // 证书剩余 < N 天才续期
        int         checkIntervalHours = 24;      // 检查周期
        std::string certOut            = "nginx/ssl/server.crt";
        std::string keyOut             = "nginx/ssl/server.key";
        // 续期成功后是否调用 NginxEmbedded::reload()
        bool        reloadNginxAfter   = true;
        // 工作目录（win-acme 需要写日志和缓存）
        std::string workDir            = "win-acme";

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

    static AcmeManager& instance();

    // 启动调度线程；若 enabled=false 立即返回
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
