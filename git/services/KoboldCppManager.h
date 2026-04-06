#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif

struct KoboldCppConfig {
    bool        enabled      = false;

    // ── 方式一：直接指定 bat / exe（优先）─────────────────────────
    std::string launchCmd;   // cmd /c xxx.bat  或  xxx.exe  直接运行

    // ── 方式二：python 脚本（launchCmd 为空时使用）────────────────
    std::string pythonExe    = "python";
    std::string scriptPath;          // koboldcpp.py 路径
    std::string modelPath;
    std::string whisperModel;
    int         port         = 5001;
    int         threads      = 4;
    int         contextSize  = 2048;
    int         blasBatch    = 512;
    bool        useGpu       = false;
    int         gpuLayers    = 99;

    // ── 公共选项 ───────────────────────────────────────────────────
    std::string workDir;     // 子进程工作目录（为空时继承父进程）
    bool        autoRestart  = true;
    bool        showWindow   = false;
};

class KoboldCppManager {
public:
    static KoboldCppManager& instance();

    bool start(const KoboldCppConfig& cfg);
    void stop();
    bool isRunning() const;
    int  port() const { return cfg_.port; }

private:
    KoboldCppManager();
    ~KoboldCppManager();
    KoboldCppManager(const KoboldCppManager&) = delete;
    KoboldCppManager& operator=(const KoboldCppManager&) = delete;

    bool        spawnProcess();
    void        startMonitor();
    void        stopMonitor();

    KoboldCppConfig     cfg_;
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
