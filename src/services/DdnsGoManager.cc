#ifdef _WIN32
#include <winsock2.h>
#ifndef htonll
#define htonll(x) ((1==htonl(1))?(x):((uint64_t)htonl((x)&0xFFFFFFFF)<<32)|htonl((x)>>32))
#endif
#ifndef ntohll
#define ntohll(x) ((1==ntohl(1))?(x):((uint64_t)ntohl((x)&0xFFFFFFFF)<<32)|ntohl((x)>>32))
#endif
#endif

#include "DdnsGoManager.h"
#include <trantor/utils/Logger.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#endif

DdnsGoManager& DdnsGoManager::instance() {
    static DdnsGoManager inst;
    return inst;
}

DdnsGoManager::DdnsGoManager() {
#ifdef _WIN32
    hJob_ = CreateJobObjectA(nullptr, nullptr);
    if (hJob_) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob_, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
#endif
}

DdnsGoManager::~DdnsGoManager() {
    stop();
#ifdef _WIN32
    if (hJob_) { CloseHandle(hJob_); hJob_ = nullptr; }
#endif
}

bool DdnsGoManager::start(const DdnsGoConfig& cfg) {
    std::lock_guard<std::mutex> lk(mu_);
    cfg_ = cfg;

    if (!std::ifstream(cfg_.exePath).good()) {
        LOG_ERROR << "[DDNS] Executable not found: " << cfg_.exePath;
        return false;
    }

    if (!spawnProcess()) return false;
    startMonitor();
    return true;
}

bool DdnsGoManager::spawnProcess() {
#ifdef _WIN32
    std::ostringstream cmd;
    cmd << "\"" << cfg_.exePath << "\"";
    if (!cfg_.configPath.empty())
        cmd << " -c \"" << cfg_.configPath << "\"";
    cmd << " -f " << cfg_.frequency;
    cmd << " -l \"" << cfg_.listenAddr << "\"";
    if (cfg_.noWeb)       cmd << " -noweb";
    if (cfg_.skipVerify)  cmd << " -skipVerify";

    std::string cmdStr = cmd.str();
    LOG_INFO << "[DDNS] Launching: " << cmdStr;

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = cfg_.showWindow ? SW_SHOW : SW_HIDE;
    DWORD createFlags = CREATE_SUSPENDED;
    if (!cfg_.showWindow) createFlags |= CREATE_NO_WINDOW;
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(nullptr, const_cast<char*>(cmdStr.c_str()),
                        nullptr, nullptr, FALSE, createFlags,
                        nullptr, nullptr, &si, &pi)) {
        LOG_ERROR << "[DDNS] CreateProcess failed: " << GetLastError();
        return false;
    }

    if (hJob_) AssignProcessToJobObject(hJob_, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    hProc_   = pi.hProcess;
    pid_     = pi.dwProcessId;
    running_ = true;
    LOG_INFO << "[DDNS] Started pid=" << pid_
             << "  web=" << (cfg_.noWeb ? "disabled" : "http://localhost" + cfg_.listenAddr);
    return true;
#else
    pid_ = fork();
    if (pid_ == 0) {
        std::string freqStr = std::to_string(cfg_.frequency);
        std::vector<const char*> args = { cfg_.exePath.c_str(),
            "-f", freqStr.c_str(), "-l", cfg_.listenAddr.c_str() };
        if (!cfg_.configPath.empty()) { args.push_back("-c"); args.push_back(cfg_.configPath.c_str()); }
        if (cfg_.noWeb)      args.push_back("-noweb");
        if (cfg_.skipVerify) args.push_back("-skipVerify");
        args.push_back(nullptr);
        execv(cfg_.exePath.c_str(), const_cast<char* const*>(args.data()));
        _exit(1);
    }
    if (pid_ < 0) { LOG_ERROR << "[DDNS] fork failed"; return false; }
    running_ = true;
    LOG_INFO << "[DDNS] Started pid=" << pid_;
    return true;
#endif
}

void DdnsGoManager::stop() {
    stopMonitor();
    std::lock_guard<std::mutex> lk(mu_);
    if (!running_) return;
#ifdef _WIN32
    if (hProc_) {
        TerminateProcess(hProc_, 0);
        WaitForSingleObject(hProc_, 3000);
        CloseHandle(hProc_);
        hProc_ = nullptr;
        pid_   = 0;
    }
#else
    if (pid_ > 0) { kill(pid_, SIGTERM); pid_ = 0; }
#endif
    running_ = false;
    LOG_INFO << "[DDNS] Stopped";
}

bool DdnsGoManager::isRunning() const {
    if (!running_) return false;
#ifdef _WIN32
    if (!hProc_) return false;
    DWORD code = STILL_ACTIVE;
    GetExitCodeProcess(hProc_, &code);
    return code == STILL_ACTIVE;
#else
    return pid_ > 0 && waitpid(pid_, nullptr, WNOHANG) == 0;
#endif
}

void DdnsGoManager::startMonitor() {
    if (!cfg_.autoRestart) return;
    monRunning_ = true;
    monThread_ = std::thread([this]() {
        while (monRunning_) {
            for (int i = 0; i < 100 && monRunning_; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!monRunning_) break;
            if (!isRunning()) {
                LOG_WARN << "[DDNS] Process died, restarting...";
                std::lock_guard<std::mutex> lk(mu_);
                spawnProcess();
            }
        }
    });
}

void DdnsGoManager::stopMonitor() {
    monRunning_ = false;
    if (monThread_.joinable()) monThread_.join();
}
