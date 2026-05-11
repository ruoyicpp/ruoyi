// =============================================================
// NginxEmbedded.h — 进程内 nginx 集成（静态链接 libnginx.a）
//
// 注意：本类名为 NginxEmbedded，与 services/NginxManager.h
// （启动外部 nginx.exe 子进程的管理器）互不干扰。
// 二者处理不同场景：
//   - NginxManager      （services/）：外部子进程，实验性 / 快部署
//   - NginxEmbedded     （common/ 本文件）：静态链入同进程，生产推荐
//
// 把官方 nginx-1.29.8 编译为静态库后嵌入 ruoyi-cpp 进程。
// nginx 在独立线程内跑 ngx_main，IOCP 事件循环；
// 通过 nginx 全局 sig_atomic_t 变量控制生命周期：
//   ngx_quit       = 1   优雅退出（worker 处理完连接再退）
//   ngx_terminate  = 1   立即终止
//   ngx_reconfigure= 1   热重载 nginx.conf
//
// 默认在 RUOYI_USE_NGINX=ON 时通过 CMake 链接：
//   libnginx.a  +  libssl.a  +  libcrypto.a  +  libpcre2-8.a  +  libz.a
//
// 使用：
//   NginxEmbedded::Config c;
//   c.enabled    = true;
//   c.prefix     = "./nginx";        // 工作目录（含 conf/ logs/ html/）
//   c.confFile   = "conf/nginx.conf";
//   NginxEmbedded::instance().start(c);
//   ...
//   NginxEmbedded::instance().stop();   // 程序退出前
// =============================================================
#pragma once

#include <trantor/utils/Logger.h>
#include <json/json.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef RUOYI_USE_NGINX

// nginx 静态库导出的接口
//   src/sgs/ngx_sgs.h 中声明了 ngx_main，这里同时声明 nginx 的全局控制变量
//   （这些是 sig_atomic_t 类型的全局可写变量）
extern "C" {
    int  ngx_main(int argc, char *const *argv);

    // 注意：nginx 用 sig_atomic_t；Windows MinGW 下等价于 int
    extern volatile sig_atomic_t ngx_terminate;
    extern volatile sig_atomic_t ngx_quit;
    extern volatile sig_atomic_t ngx_reconfigure;
    extern volatile sig_atomic_t ngx_exiting;
}

class NginxEmbedded {
public:
    struct Config {
        bool        enabled       = false;
        // -p 工作目录前缀（绝对或相对路径，需含 conf/ logs/ html/）
        std::string prefix        = "./nginx";
        // -c 配置文件（相对 prefix）
        std::string confFile      = "conf/nginx.conf";
        // 优雅退出最大等待
        int         stopTimeoutMs = 5000;

        // P0-3 watchdog：nginx 异常退出后自动重启
        bool        autoRestart      = true;
        int         maxRestartCount  = 5;     // 连续失败上限（防 cycle）
        int         restartBackoffMs = 3000;  // 重启间隔
        int         watchIntervalMs  = 2000;  // watchdog 轮询周期

        // P1-7 conf 文件热重载
        bool        autoReloadOnChange = true; // 监听 conf 目录文件 mtime
        int         reloadDebounceMs   = 1500; // mtime 变化后等待稳定时间

        static Config fromJson(const Json::Value& c) {
            Config r;
            r.enabled            = c.get("enabled",              false).asBool();
            r.prefix             = c.get("prefix",               "./nginx").asString();
            r.confFile           = c.get("conf_file",            "conf/nginx.conf").asString();
            r.stopTimeoutMs      = c.get("stop_timeout_ms",      5000).asInt();
            r.autoRestart        = c.get("auto_restart",         true).asBool();
            r.maxRestartCount    = c.get("max_restart_count",    5).asInt();
            r.restartBackoffMs   = c.get("restart_backoff_ms",   3000).asInt();
            r.watchIntervalMs    = c.get("watch_interval_ms",    2000).asInt();
            r.autoReloadOnChange = c.get("auto_reload_on_change",true).asBool();
            r.reloadDebounceMs   = c.get("reload_debounce_ms",   1500).asInt();
            return r;
        }
    };

    static NginxEmbedded& instance() { static NginxEmbedded x; return x; }

    // 启动 nginx：在独立线程内调用 ngx_main，并启动 watchdog 监控
    // 失败原因（返回 false）：
    //   - 未启用
    //   - 工作目录不存在
    //   - 配置文件不存在
    //   - 已运行
    bool start(const Config& c) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!c.enabled) return false;
        if (running_.load()) return true;

        cfg_ = c;

        if (!startNginxThreadLocked()) return false;

        // 启动 watchdog（异常重启 + conf 热重载）
        if ((c.autoRestart || c.autoReloadOnChange) && !watchdog_.joinable()) {
            stopWatchdog_.store(false);
            restartCount_.store(0);
            // 初始化 conf 目录 mtime 快照
            snapshotConfMtimes();
            watchdog_ = std::thread([this]() { watchdogLoop(); });
        }

        // 等到一定时间，确认 nginx 已 listen 起来
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return true;
    }

    // 优雅退出（worker 把已建连接处理完）
    bool stop() {
        // 1) 先停 watchdog（避免它在 stopNginx 后又自动 restart）
        stopWatchdog_.store(true);
        watchdogCv_.notify_all();
        if (watchdog_.joinable()) watchdog_.join();

        // 2) 再停 nginx 本体
        std::lock_guard<std::mutex> lk(mu_);
        return stopNginxLocked();
    }

private:
    bool stopNginxLocked() {
        if (!running_.load()) return true;

        LOG_INFO << "[Nginx] 优雅退出 (ngx_quit=1)";
        ngx_quit = 1;

        // 等待 ngx_main 自然返回
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(cfg_.stopTimeoutMs);
        while (running_.load() &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // 超时未退则强制终止
        if (running_.load()) {
            LOG_WARN << "[Nginx] 优雅退出超时，强制 terminate";
            ngx_terminate = 1;
            auto deadline2 = std::chrono::steady_clock::now() +
                             std::chrono::milliseconds(2000);
            while (running_.load() &&
                   std::chrono::steady_clock::now() < deadline2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        if (worker_.joinable()) worker_.join();
        LOG_INFO << "[Nginx] 已停止";
        std::cout << "[Nginx] 已停止" << std::endl;
        return !running_.load();
    }

public:

    // 热重载 nginx.conf（不重启进程）
    bool reload() {
        if (!running_.load()) return false;
        LOG_INFO << "[Nginx] 热重载 (ngx_reconfigure=1)";
        ngx_reconfigure = 1;
        return true;
    }

    bool isRunning() const { return running_.load(); }
    const Config& config() const { return cfg_; }
    int  restartCount() const { return restartCount_.load(); }

private:
    NginxEmbedded()
        : running_(false),
          stopWatchdog_(false),
          restartCount_(0) {}
    ~NginxEmbedded() { try { stop(); } catch (...) {} }
    NginxEmbedded(const NginxEmbedded&) = delete;
    NginxEmbedded& operator=(const NginxEmbedded&) = delete;

    // 内部启动：调用前需持 mu_
    bool startNginxThreadLocked() {
        std::error_code ec;
        if (!std::filesystem::exists(cfg_.prefix, ec)) {
            LOG_WARN << "[Nginx] prefix 目录不存在，跳过: " << cfg_.prefix;
            return false;
        }
        std::filesystem::path confPath = std::filesystem::path(cfg_.prefix) / cfg_.confFile;
        if (!std::filesystem::exists(confPath, ec)) {
            LOG_WARN << "[Nginx] 配置文件不存在，跳过: " << confPath.string();
            return false;
        }

        LOG_INFO << "[Nginx] 启动 prefix=" << cfg_.prefix
                 << " conf=" << cfg_.confFile;
        std::cout << "[Nginx] 启动 prefix=" << cfg_.prefix
                  << " conf=" << cfg_.confFile << std::endl;

        // 重置全局控制变量（防止上次 stop 残留）
        ngx_terminate   = 0;
        ngx_quit        = 0;
        ngx_reconfigure = 0;
        ngx_exiting     = 0;

        // 构造 argv
        argvStorage_.clear();
        argvStorage_.push_back("nginx");
        argvStorage_.push_back("-p");
        argvStorage_.push_back(cfg_.prefix);
        argvStorage_.push_back("-c");
        argvStorage_.push_back(cfg_.confFile);

        argvPtrs_.clear();
        for (auto& s : argvStorage_) argvPtrs_.push_back(s.data());

        // 如有旧 worker 残留，先 join
        if (worker_.joinable()) worker_.join();

        running_.store(true);
        worker_ = std::thread([this]() {
            int rc = ngx_main((int)argvPtrs_.size(), argvPtrs_.data());
            running_.store(false);
            LOG_INFO << "[Nginx] ngx_main 已退出 rc=" << rc;
        });
        return true;
    }

    // 扫描 conf 目录下所有文件 mtime，作为基准快照
    void snapshotConfMtimes() {
        confMtimes_.clear();
        std::error_code ec;
        auto confDir = std::filesystem::path(cfg_.prefix) / "conf";
        if (!std::filesystem::exists(confDir, ec)) return;
        for (auto& entry : std::filesystem::recursive_directory_iterator(confDir, ec)) {
            if (ec) break;
            if (entry.is_regular_file(ec)) {
                confMtimes_[entry.path().string()] =
                    std::filesystem::last_write_time(entry.path(), ec);
            }
        }
    }

    // 检查 conf 目录是否有文件 mtime 变化；若有，返回 true 并更新快照
    bool checkConfChanged() {
        std::error_code ec;
        auto confDir = std::filesystem::path(cfg_.prefix) / "conf";
        if (!std::filesystem::exists(confDir, ec)) return false;

        bool changed = false;
        std::map<std::string, std::filesystem::file_time_type> current;
        for (auto& entry : std::filesystem::recursive_directory_iterator(confDir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;
            auto path = entry.path().string();
            auto mtime = std::filesystem::last_write_time(entry.path(), ec);
            current[path] = mtime;

            auto it = confMtimes_.find(path);
            if (it == confMtimes_.end() || it->second != mtime) {
                changed = true;
            }
        }
        // 检测删除（current 少于 snapshot）
        if (!changed && current.size() != confMtimes_.size()) changed = true;

        if (changed) confMtimes_.swap(current);
        return changed;
    }

    // watchdog 主循环：监控 ngx_main 退出 + conf 变更
    void watchdogLoop() {
        LOG_INFO << "[Nginx][watchdog] started";
        while (!stopWatchdog_.load()) {
            // 用 cv 等待，可被 stop() 立即唤醒
            {
                std::unique_lock<std::mutex> ul(watchdogMu_);
                watchdogCv_.wait_for(ul,
                    std::chrono::milliseconds(cfg_.watchIntervalMs),
                    [this]() { return stopWatchdog_.load(); });
            }
            if (stopWatchdog_.load()) break;

            // === 1) 异常退出自动重启 ===
            if (cfg_.autoRestart && !running_.load()) {
                int n = restartCount_.load();
                if (n >= cfg_.maxRestartCount) {
                    LOG_ERROR << "[Nginx][watchdog] 异常重启次数已达上限 (" << n
                              << "/" << cfg_.maxRestartCount << ")，停止尝试";
                    break;
                }
                LOG_WARN << "[Nginx][watchdog] 检测到 nginx 已退出，准备第 "
                         << (n + 1) << " 次重启 (上限 " << cfg_.maxRestartCount << ")";
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(cfg_.restartBackoffMs));
                if (stopWatchdog_.load()) break;

                std::lock_guard<std::mutex> lk(mu_);
                if (startNginxThreadLocked()) {
                    restartCount_.fetch_add(1);
                    LOG_INFO << "[Nginx][watchdog] 重启成功 (累计="
                             << restartCount_.load() << ")";
                }
                continue;
            }

            // === 2) conf 文件变化自动 reload ===
            if (cfg_.autoReloadOnChange && running_.load()) {
                if (checkConfChanged()) {
                    LOG_INFO << "[Nginx][watchdog] conf 文件变化，去抖 "
                             << cfg_.reloadDebounceMs << "ms 后 reload";
                    // debounce：等待文件写入稳定（避免编辑器中途保存触发多次）
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(cfg_.reloadDebounceMs));
                    if (stopWatchdog_.load()) break;
                    // 再吸收一次（去抖期间又改的合并进来）
                    checkConfChanged();
                    if (running_.load()) {
                        LOG_INFO << "[Nginx][watchdog] 触发热重载";
                        ngx_reconfigure = 1;
                    }
                }
            }
        }
        LOG_INFO << "[Nginx][watchdog] stopped";
    }

    Config cfg_;
    std::atomic<bool> running_;
    std::mutex mu_;
    std::thread worker_;

    // watchdog 状态
    std::thread             watchdog_;
    std::atomic<bool>       stopWatchdog_;
    std::atomic<int>        restartCount_;
    std::mutex              watchdogMu_;
    std::condition_variable watchdogCv_;
    std::map<std::string, std::filesystem::file_time_type> confMtimes_;

    // argv 必须保活到 ngx_main 返回（nginx 内部保留指针引用）
    std::vector<std::string> argvStorage_;
    std::vector<char*>       argvPtrs_;
};

#else  // !RUOYI_USE_NGINX

// 未启用 nginx 时提供空壳，让 main.cc 不必加 #ifdef
class NginxEmbedded {
public:
    struct Config {
        bool enabled = false;
        std::string prefix;
        std::string confFile;
        static Config fromJson(const Json::Value&) { return {}; }
    };
    static NginxEmbedded& instance() { static NginxEmbedded x; return x; }
    bool start(const Config&) { return false; }
    bool stop()  { return true; }
    bool reload(){ return false; }
    bool isRunning() const { return false; }
    int  restartCount() const { return 0; }
};

#endif  // RUOYI_USE_NGINX
