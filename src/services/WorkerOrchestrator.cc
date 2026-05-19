#include "WorkerOrchestrator.h"
#include <trantor/utils/Logger.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
    // 全局唯一实例指针（信号处理用）
    WorkerOrchestrator* g_singleton = nullptr;

#ifdef _WIN32
    BOOL WINAPI consoleCtrlHandler(DWORD ctrl) {
        if (ctrl == CTRL_C_EVENT || ctrl == CTRL_BREAK_EVENT ||
            ctrl == CTRL_CLOSE_EVENT || ctrl == CTRL_SHUTDOWN_EVENT) {
            if (g_singleton) g_singleton->requestShutdown();
            return TRUE;
        }
        return FALSE;
    }
#else
    void unixSignalHandler(int) {
        if (g_singleton) g_singleton->requestShutdown();
    }
#endif
}

WorkerOrchestrator& WorkerOrchestrator::instance() {
    static WorkerOrchestrator inst;
    return inst;
}

WorkerOrchestrator::WorkerOrchestrator() {
    g_singleton = this;
#ifdef _WIN32
    hJob_ = CreateJobObjectA(nullptr, nullptr);
    if (hJob_) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob_, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
#endif
}

WorkerOrchestrator::~WorkerOrchestrator() {
#ifdef _WIN32
    if (hJob_) { CloseHandle(hJob_); hJob_ = nullptr; }
#endif
    g_singleton = nullptr;
}

int WorkerOrchestrator::currentWorkerIndex() {
    const char* v = std::getenv("RUOYI_WORKER_INDEX");
    if (!v || !*v) return -1;
    try {
        int idx = std::stoi(v);
        return idx >= 0 ? idx : -1;
    } catch (...) { return -1; }
}

bool WorkerOrchestrator::WorkerSlot::isAlive() const {
#ifdef _WIN32
    if (!hProc) return false;
    DWORD code = STILL_ACTIVE;
    if (!GetExitCodeProcess(hProc, &code)) return false;
    return code == STILL_ACTIVE;
#else
    if (pid <= 0) return false;
    int status;
    pid_t r = waitpid(pid, &status, WNOHANG);
    return r == 0;  // 0=still running, >0=exited, <0=error
#endif
}

bool WorkerOrchestrator::spawnWorker(WorkerSlot& slot) {
    slot.lastStartedAt = std::chrono::steady_clock::now();

#ifdef _WIN32
    // 构造命令行：当前 exe + extraArgs
    std::string cmd = "\"" + cfg_.exePath + "\"";
    for (auto& a : cfg_.extraArgs) cmd += " " + a;

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    // 子进程共享父进程 stdout/stderr，方便日志聚合
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION pi{};

    // 准备环境变量：复制当前环境 + 追加 RUOYI_WORKER_INDEX
    // 简化做法：直接修改本进程 env，spawn 后再恢复（CreateProcess 继承父 env）
    std::string envKey = "RUOYI_WORKER_INDEX";
    char prevBuf[32]; DWORD prevLen = GetEnvironmentVariableA(envKey.c_str(), prevBuf, sizeof(prevBuf));
    std::string idxStr = std::to_string(slot.index);
    SetEnvironmentVariableA(envKey.c_str(), idxStr.c_str());

    DWORD flags = CREATE_SUSPENDED;
    BOOL ok = CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                             nullptr, nullptr, TRUE /* 继承 handle */,
                             flags, nullptr, nullptr, &si, &pi);

    // 恢复父进程的环境变量
    if (prevLen == 0) SetEnvironmentVariableA(envKey.c_str(), nullptr);
    else              SetEnvironmentVariableA(envKey.c_str(), prevBuf);

    if (!ok) {
        LOG_ERROR << "[Orchestrator] CreateProcess failed err=" << GetLastError()
                  << " cmd=" << cmd;
        return false;
    }
    if (hJob_) AssignProcessToJobObject(hJob_, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    slot.hProc = pi.hProcess;
    slot.pid   = pi.dwProcessId;

    // 创建命名事件用于优雅退出通知
    std::string evName = stopEventName(slot.pid);
    slot.hStopEvent = CreateEventA(nullptr, TRUE, FALSE, evName.c_str());

    LOG_INFO << "[Orchestrator] worker[" << slot.index << "] spawned pid=" << slot.pid;
    std::cout << "[Orchestrator] worker[" << slot.index << "] spawned pid=" << slot.pid << std::endl;
    return true;
#else
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程
        setenv("RUOYI_WORKER_INDEX", std::to_string(slot.index).c_str(), 1);
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(cfg_.exePath.c_str()));
        for (auto& a : cfg_.extraArgs) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(cfg_.exePath.c_str(), argv.data());
        _exit(127);
    }
    if (pid < 0) {
        LOG_ERROR << "[Orchestrator] fork failed errno=" << errno;
        return false;
    }
    slot.pid = pid;
    LOG_INFO << "[Orchestrator] worker[" << slot.index << "] spawned pid=" << slot.pid;
    return true;
#endif
}

bool WorkerOrchestrator::killWorker(WorkerSlot& slot, bool graceful, int waitMs) {
#ifdef _WIN32
    if (!slot.hProc) return true;
    if (graceful && slot.hStopEvent) {
        // 通过命名事件通知子进程优雅退出
        SetEvent(slot.hStopEvent);
        DWORD rc = WaitForSingleObject(slot.hProc, waitMs);
        if (rc == WAIT_TIMEOUT) {
            LOG_WARN << "[Orchestrator] worker[" << slot.index
                     << "] did not exit gracefully, force kill";
            TerminateProcess(slot.hProc, 1);
            WaitForSingleObject(slot.hProc, 1000);
        }
    } else {
        TerminateProcess(slot.hProc, 1);
        WaitForSingleObject(slot.hProc, waitMs);
    }
    if (slot.hStopEvent) { CloseHandle(slot.hStopEvent); slot.hStopEvent = nullptr; }
    CloseHandle(slot.hProc);
    slot.hProc = nullptr;
    slot.pid = 0;
    return true;
#else
    if (slot.pid <= 0) return true;
    kill(slot.pid, graceful ? SIGTERM : SIGKILL);
    int status, elapsed = 0;
    while (elapsed < waitMs && waitpid(slot.pid, &status, WNOHANG) == 0) {
        usleep(100 * 1000);
        elapsed += 100;
    }
    if (waitpid(slot.pid, &status, WNOHANG) == 0) kill(slot.pid, SIGKILL);
    slot.pid = 0;
    return true;
#endif
}

void WorkerOrchestrator::requestShutdown() {
    stopRequested_.store(true);
    std::cout << "[Orchestrator] shutdown requested" << std::endl;
}

int WorkerOrchestrator::run(const Config& cfg) {
    cfg_ = cfg;
    if (!cfg_.enabled || cfg_.workerCount < 1) {
        LOG_WARN << "[Orchestrator] disabled or workerCount<1, exiting";
        return 0;
    }

    // 注册信号处理
#ifdef _WIN32
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#else
    struct sigaction sa{};
    sa.sa_handler = unixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    signal(SIGCHLD, SIG_IGN);  // 防止僵尸（也可 waitpid）
#endif

    // 初始化 slot
    slots_.clear();
    slots_.resize(cfg_.workerCount);
    for (int i = 0; i < cfg_.workerCount; ++i) {
        slots_[i].index = i;
    }

    std::cout << "[Orchestrator] starting " << cfg_.workerCount << " worker(s) of: "
              << cfg_.exePath << std::endl;
    LOG_INFO << "[Orchestrator] starting " << cfg_.workerCount << " worker(s) of: "
             << cfg_.exePath;

    // 全部 spawn
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& s : slots_) spawnWorker(s);
    }

    // 监控循环
    while (!stopRequested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.watchIntervalMs));
        if (stopRequested_.load()) break;

        std::lock_guard<std::mutex> lk(mu_);
        for (auto& s : slots_) {
            if (s.isAlive()) continue;
            // 子进程已死，清理 handle
#ifdef _WIN32
            if (s.hProc) { CloseHandle(s.hProc); s.hProc = nullptr; }
            s.pid = 0;
#else
            s.pid = 0;
#endif
            if (s.restartCount >= cfg_.maxRestarts) {
                LOG_ERROR << "[Orchestrator] worker[" << s.index
                          << "] exceeded max restarts (" << s.restartCount
                          << "), giving up";
                continue;
            }
            LOG_WARN << "[Orchestrator] worker[" << s.index
                     << "] died, restart " << (s.restartCount + 1)
                     << "/" << cfg_.maxRestarts;
            std::cout << "[Orchestrator] worker[" << s.index
                      << "] died, restart " << (s.restartCount + 1) << std::endl;
            // backoff
            std::this_thread::sleep_for(
                std::chrono::milliseconds(cfg_.restartBackoffMs));
            if (stopRequested_.load()) break;
            if (spawnWorker(s)) s.restartCount++;
        }
    }

    // 退出：通知所有子进程退出
    LOG_INFO << "[Orchestrator] stopping all workers";
    std::cout << "[Orchestrator] stopping all workers" << std::endl;
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& s : slots_) killWorker(s, true, 5000);
    }

    LOG_INFO << "[Orchestrator] all workers stopped, exiting";
    return 0;
}

#ifdef _WIN32
std::string WorkerOrchestrator::stopEventName(DWORD pid) {
    return "Global\\RUOYI_WORKER_STOP_" + std::to_string(pid);
}
#endif

void WorkerOrchestrator::watchStopEvent(std::function<void()> callback) {
#ifdef _WIN32
    DWORD myPid = GetCurrentProcessId();
    std::string evName = "Global\\RUOYI_WORKER_STOP_" + std::to_string(myPid);
    HANDLE hEv = OpenEventA(SYNCHRONIZE, FALSE, evName.c_str());
    if (!hEv) {
        // 如果事件还不存在（非编排器启动），创建它以便后续可以被 signal
        hEv = CreateEventA(nullptr, TRUE, FALSE, evName.c_str());
    }
    if (!hEv) return;

    std::thread([hEv, cb = std::move(callback)]() {
        WaitForSingleObject(hEv, INFINITE);
        CloseHandle(hEv);
        if (cb) cb();
    }).detach();
#else
    // Linux 用 SIGTERM，已在信号处理中覆盖，无需额外操作
    (void)callback;
#endif
}
