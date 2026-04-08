#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <sys/stat.h>
#include <fstream>
#include <json/json.h>
#include "JwtUtils.h"

// 配置热重载：每5秒检测 config.json 修改时间，变化则重新加载并回调
class HotConfig {
public:
    static HotConfig &instance() { static HotConfig h; return h; }

    void start(const std::string &path, std::function<void()> onReload, int intervalSec = 5) {
        path_      = path;
        onReload_  = std::move(onReload);
        interval_  = intervalSec;
        lastMtime_ = mtime();
        running_   = true;
        thread_    = std::thread([this]{ run(); });
        thread_.detach();
    }

    void stop() { running_ = false; }

    // 手动触发重载（供 POST /actuator/reload 调用）
    bool reload() {
        try {
            JwtUtils::loadConfig(); // 重新从 config.json 加载 jwt 配置
            lastMtime_ = mtime();
            if (onReload_) onReload_();
            return true;
        } catch (...) { return false; }
    }

    const std::string &path() const { return path_; }

private:
    std::string path_;
    std::function<void()> onReload_;
    int interval_ = 5;
    std::atomic<bool> running_{false};
    std::thread thread_;
    time_t lastMtime_ = 0;

    time_t mtime() {
        struct stat st{};
        return (stat(path_.c_str(), &st) == 0) ? st.st_mtime : 0;
    }

    void run() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(interval_));
            if (!running_) break;
            time_t cur = mtime();
            if (cur && cur != lastMtime_) {
                lastMtime_ = cur;
                try {
                    JwtUtils::loadConfig();
                    if (onReload_) onReload_();
                } catch (...) {}
            }
        }
    }
};
