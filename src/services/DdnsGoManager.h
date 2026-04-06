#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif

struct DdnsGoConfig {
    bool        enabled      = false;
    std::string exePath;             // ddns-go.exe 绝对路径
    std::string configPath;          // .ddns_go_config.yaml 路径
    int         frequency    = 300;  // 更新频率（秒）
    std::string listenAddr   = ":9876"; // Web 管理界面
    bool        noWeb        = false;   // 禁用 Web 界面
    bool        skipVerify   = false;
    bool        autoRestart  = true;
    bool        showWindow   = false;
};

class DdnsGoManager {
public:
    static DdnsGoManager& instance();

    bool start(const DdnsGoConfig& cfg);
    void stop();
    bool isRunning() const;

private:
    DdnsGoManager();
    ~DdnsGoManager();
    DdnsGoManager(const DdnsGoManager&) = delete;
    DdnsGoManager& operator=(const DdnsGoManager&) = delete;

    bool spawnProcess();
    void startMonitor();
    void stopMonitor();

    DdnsGoConfig        cfg_;
    std::atomic<bool>   running_{false};
    std::atomic<bool>   monRunning_{false};
    std::thread         monThread_;
    mutable std::mutex  mu_;

#ifdef _WIN32
    HANDLE hProc_ = nullptr;
    HANDLE hJob_  = nullptr;
    DWORD  pid_   = 0;
#else
    pid_t  pid_   = 0;
#endif
};
