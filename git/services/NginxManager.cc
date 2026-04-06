#include "NginxManager.h"
#include <filesystem>
#include <iostream>
#include <trantor/utils/Logger.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <signal.h>
#include <sys/wait.h>
#endif

NginxManager& NginxManager::instance() {
    static NginxManager inst;
    return inst;
}

NginxManager::NginxManager() {
#ifdef _WIN32
    hJob_ = CreateJobObjectA(nullptr, nullptr);
    if (hJob_) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob_, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        LOG_INFO << "[NGINX] Job Object created (auto-cleanup on exit)";
    } else {
        LOG_WARN << "[NGINX] Failed to create Job Object (err=" << GetLastError() << ")";
    }
#endif
}

NginxManager::~NginxManager() {
    if (isRunning()) stop(false);
    stopMonitor();
#ifdef _WIN32
    if (hJob_) { CloseHandle(hJob_); hJob_ = nullptr; }
#endif
}

void NginxManager::init(const NginxConfig& cfg) {
    std::lock_guard<std::mutex> lk(mu_);
    cfg_    = cfg;
    inited_ = true;
}

bool NginxManager::start() {
    std::lock_guard<std::mutex> lk(mu_);
    if (!inited_ || !cfg_.enabled) return false;
    if (state_ == NginxState::Running) return true;
    if (!std::filesystem::exists(cfg_.exePath)) {
        LOG_INFO << "[NGINX] exe not found, skipped (" << cfg_.exePath << ")";
        return false;
    }

#ifdef _WIN32
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    // 构建命令: nginx.exe -p prefix/
    // 注意：Windows 路径末尾 \ 在引号内会转义引号，需去掉末尾分隔符
    std::string exeNative = cfg_.exePath;
    std::string prefNative = cfg_.prefix;
    for (auto& c : exeNative)  if (c == '/') c = '\\';
    for (auto& c : prefNative) if (c == '/') c = '\\';
    while (!prefNative.empty() && (prefNative.back() == '\\' || prefNative.back() == '/'))
        prefNative.pop_back();
    std::string cmd = "\"" + exeNative + "\" -p \"" + prefNative + "\"";

    if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                        nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED,
                        nullptr, nullptr, &si, &pi)) {
        LOG_ERROR << "[NGINX] CreateProcess failed err=" << GetLastError();
        state_ = NginxState::Failed;
        return false;
    }

    if (hJob_) {
        if (!AssignProcessToJobObject(hJob_, pi.hProcess))
            LOG_WARN << "[NGINX] AssignProcessToJobObject failed err=" << GetLastError()
                     << " (force-kill cleanup may not work if parent is already in a job)";
        else
            LOG_INFO << "[NGINX] Process assigned to Job Object";
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    hProc_ = pi.hProcess;
    pid_   = pi.dwProcessId;
#else
    pid_t pid = fork();
    if (pid < 0) { state_ = NginxState::Failed; return false; }
    if (pid == 0) {
        execl(cfg_.exePath.c_str(), cfg_.exePath.c_str(),
              "-p", cfg_.prefix.c_str(), nullptr);
        exit(1);
    }
    pid_ = pid;
#endif

    state_      = NginxState::Running;
    startTime_  = std::chrono::system_clock::now();
    restartCount_ = 0;
    std::cout << "[NGINX] Started pid=" << pid_ << "  http://localhost:" << cfg_.port << std::endl;
    LOG_INFO << "[NGINX] Started pid=" << pid_ << "  port=" << cfg_.port;

    if (cfg_.autoRestart) startMonitor();
    return true;
}

bool NginxManager::stop(bool graceful) {
    stopMonitor();
    std::lock_guard<std::mutex> lk(mu_);
    if (state_ != NginxState::Running) return true;

#ifdef _WIN32
    if (!hProc_) return true;
    if (graceful) {
        sendSignal("quit");
        if (WaitForSingleObject(hProc_, 5000) == WAIT_TIMEOUT) {
            LOG_WARN << "[NGINX] Graceful stop timed out, forcing";
            TerminateProcess(hProc_, 0);
        }
    } else {
        TerminateProcess(hProc_, 0);
    }
    CloseHandle(hProc_);
    hProc_ = nullptr;
    pid_   = 0;
#else
    if (pid_ > 0) {
        kill(pid_, graceful ? SIGQUIT : SIGKILL);
        int st, n = 0;
        while (n++ < 50 && waitpid(pid_, &st, WNOHANG) == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (kill(pid_, 0) == 0) kill(pid_, SIGKILL);
        pid_ = 0;
    }
#endif
    state_ = NginxState::Stopped;
    std::cout << "[NGINX] Stopped" << std::endl;
    LOG_INFO << "[NGINX] Stopped";
    return true;
}

bool NginxManager::isRunning() const {
#ifdef _WIN32
    if (!hProc_) return false;
    DWORD code;
    return GetExitCodeProcess(hProc_, &code) && code == STILL_ACTIVE;
#else
    return pid_ > 0 && kill(pid_, 0) == 0;
#endif
}

bool NginxManager::sendSignal(const std::string& sig) {
#ifdef _WIN32
    std::string exeN = cfg_.exePath, preN = cfg_.prefix;
    for (auto& c : exeN) if (c == '/') c = '\\';
    for (auto& c : preN) if (c == '/') c = '\\';
    while (!preN.empty() && (preN.back() == '\\' || preN.back() == '/'))
        preN.pop_back();
    std::string cmd = "\"" + exeN + "\" -p \"" + preN + "\" -s " + sig;
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                        nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return true;
#else
    if (pid_ <= 0) return false;
    int s = (sig=="stop") ? SIGTERM : (sig=="quit") ? SIGQUIT :
            (sig=="reload") ? SIGHUP : SIGUSR1;
    return kill(pid_, s) == 0;
#endif
}

void NginxManager::startMonitor() {
    if (monRunning_) return;
    monRunning_ = true;
    monThread_ = std::thread([this]() {
        while (monRunning_) {
            // 每100ms检查一次g_stopping，满5s才做健康检查
            for (int i = 0; i < 50 && monRunning_; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!monRunning_) break;
            if (state_ == NginxState::Running && !isRunning()) {
                LOG_WARN << "[NGINX] Died unexpectedly, restarting...";
                std::cout << "[NGINX] Died unexpectedly, restarting..." << std::endl;
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    state_ = NginxState::Stopped;
#ifdef _WIN32
                    if (hProc_) { CloseHandle(hProc_); hProc_ = nullptr; }
                    pid_ = 0;
#else
                    pid_ = 0;
#endif
                }
                if (cfg_.autoRestart && restartCount_ < cfg_.maxRestarts) {
                    ++restartCount_;
                    start();
                }
            }
        }
    });
}

void NginxManager::stopMonitor() {
    monRunning_ = false;
    if (monThread_.joinable()) monThread_.join();
}
