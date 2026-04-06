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

struct NginxConfig {
    std::string exePath   = "nginx/nginx.exe";
    std::string prefix    = "nginx/";
    int         port      = 18081;
    bool        enabled   = true;
    bool        autoRestart  = true;
    int         maxRestarts  = 5;
};

enum class NginxState { Stopped, Running, Failed };

class NginxManager {
public:
    static NginxManager& instance();

    void   init(const NginxConfig& cfg);
    bool   start();
    bool   stop(bool graceful = true);
    bool   isRunning() const;
    NginxState state() const { return state_; }

private:
    NginxManager();
    ~NginxManager();
    NginxManager(const NginxManager&) = delete;
    NginxManager& operator=(const NginxManager&) = delete;

    bool   sendSignal(const std::string& sig);
    void   startMonitor();
    void   stopMonitor();

    NginxConfig         cfg_;
    NginxState          state_ = NginxState::Stopped;
    mutable std::mutex  mu_;
    bool                inited_ = false;

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
