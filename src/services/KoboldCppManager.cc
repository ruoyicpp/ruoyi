#ifdef _WIN32
#include <winsock2.h>
#ifndef htonll
#define htonll(x) ((1==htonl(1))?(x):((uint64_t)htonl((x)&0xFFFFFFFF)<<32)|htonl((x)>>32))
#endif
#ifndef ntohll
#define ntohll(x) ((1==ntohl(1))?(x):((uint64_t)ntohl((x)&0xFFFFFFFF)<<32)|ntohl((x)>>32))
#endif
#endif

#include "KoboldCppManager.h"
#include <trantor/utils/Logger.h>
#include <sstream>
#include <fstream>
#include <chrono>
#include <thread>
#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#endif

KoboldCppManager& KoboldCppManager::instance() {
    static KoboldCppManager inst;
    return inst;
}

KoboldCppManager::KoboldCppManager() {
#ifdef _WIN32
    hJob_ = CreateJobObjectA(nullptr, nullptr);
    if (hJob_) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob_, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
#endif
}

KoboldCppManager::~KoboldCppManager() {
    stop();
#ifdef _WIN32
    if (hJob_) { CloseHandle(hJob_); hJob_ = nullptr; }
#endif
}

bool KoboldCppManager::start(const KoboldCppConfig& cfg) {
    std::lock_guard<std::mutex> lk(mu_);
    cfg_ = cfg;

    if (cfg_.launchCmd.empty()) {
        // 脚本路径：相对路径时拼接 workDir 检查
        std::string scriptFull = cfg_.scriptPath;
        if (!cfg_.workDir.empty() && !cfg_.scriptPath.empty()) {
            std::ifstream rel(cfg_.workDir + "/" + cfg_.scriptPath);
            if (rel.good()) scriptFull = cfg_.workDir + "/" + cfg_.scriptPath;
        }
        if (!std::ifstream(scriptFull).good()) {
            LOG_ERROR << "[KoboldCpp] Script not found: " << scriptFull;
            return false;
        }
        if (!std::ifstream(cfg_.modelPath).good()) {
            LOG_ERROR << "[KoboldCpp] Model not found: " << cfg_.modelPath;
            return false;
        }
    }

    if (!spawnProcess()) return false;
    startMonitor();
    return true;
}

bool KoboldCppManager::spawnProcess() {
#ifdef _WIN32
    std::string cmdStr;
    if (!cfg_.launchCmd.empty()) {
        // 方式一：直接运行 bat 或 exe（用户自定义命令）
        // .bat 文件需要通过 cmd /c 运行
        if (cfg_.launchCmd.size() >= 4 &&
            cfg_.launchCmd.compare(cfg_.launchCmd.size()-4, 4, ".bat") == 0) {
            cmdStr = "cmd /c \"" + cfg_.launchCmd + "\"";
        } else {
            cmdStr = cfg_.launchCmd;
        }
    } else {
        // 方式二：python koboldcpp.py + 参数
        std::ostringstream cmd;
        cmd << "\"" << cfg_.pythonExe << "\" \""
            << cfg_.scriptPath << "\""
            << " --model \"" << cfg_.modelPath << "\""
            << " --port " << cfg_.port
            << " --threads " << cfg_.threads
            << " --contextsize " << cfg_.contextSize
            << " --blasbatchsize " << cfg_.blasBatch
            << " --quiet";
        if (cfg_.useGpu)
            cmd << " --usecublas --gpulayers " << cfg_.gpuLayers;
        if (!cfg_.whisperModel.empty() && std::ifstream(cfg_.whisperModel).good())
            cmd << " --whispermodel \"" << cfg_.whisperModel << "\"";
        cmdStr = cmd.str();
    }
    LOG_INFO << "[KoboldCpp] Launching: " << cmdStr;

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags    = STARTF_USESHOWWINDOW;
    si.wShowWindow = cfg_.showWindow ? SW_SHOW : SW_HIDE;
    DWORD createFlags = CREATE_SUSPENDED;
    if (!cfg_.showWindow) createFlags |= CREATE_NO_WINDOW;
    PROCESS_INFORMATION pi{};

    const char* workDir = cfg_.workDir.empty() ? nullptr : cfg_.workDir.c_str();
    if (!CreateProcessA(nullptr, const_cast<char*>(cmdStr.c_str()),
                        nullptr, nullptr, FALSE,
                        createFlags,
                        nullptr, workDir, &si, &pi)) {
        LOG_ERROR << "[KoboldCpp] CreateProcess failed: " << GetLastError();
        return false;
    }

    if (hJob_) AssignProcessToJobObject(hJob_, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    hProc_ = pi.hProcess;
    pid_   = pi.dwProcessId;
    running_ = true;
    LOG_INFO << "[KoboldCpp] Started pid=" << pid_ << "  http://localhost:" << cfg_.port;
    return true;
#else
    // Linux: fork + execl
    pid_ = fork();
    if (pid_ == 0) {
        std::string portStr    = std::to_string(cfg_.port);
        std::string threadsStr = std::to_string(cfg_.threads);
        std::string ctxStr     = std::to_string(cfg_.contextSize);
        execl(cfg_.pythonExe.c_str(), cfg_.pythonExe.c_str(),
              cfg_.scriptPath.c_str(),
              "--model",       cfg_.modelPath.c_str(),
              "--port",        portStr.c_str(),
              "--threads",     threadsStr.c_str(),
              "--contextsize", ctxStr.c_str(),
              "--quiet",       nullptr);
        _exit(1);
    }
    if (pid_ < 0) {
        LOG_ERROR << "[KoboldCpp] fork failed";
        return false;
    }
    running_ = true;
    LOG_INFO << "[KoboldCpp] Started pid=" << pid_ << "  http://localhost:" << cfg_.port;
    return true;
#endif
}

void KoboldCppManager::stop() {
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
    LOG_INFO << "[KoboldCpp] Stopped";
}

bool KoboldCppManager::isRunning() const {
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

void KoboldCppManager::startMonitor() {
    if (!cfg_.autoRestart) return;
    monRunning_ = true;
    monThread_ = std::thread([this]() {
        while (monRunning_) {
            for (int i = 0; i < 50 && monRunning_; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!monRunning_) break;
            if (!isRunning()) {
                LOG_WARN << "[KoboldCpp] Process died, restarting...";
                std::lock_guard<std::mutex> lk(mu_);
                spawnProcess();
            }
        }
    });
}

void KoboldCppManager::stopMonitor() {
    monRunning_ = false;
    if (monThread_.joinable()) monThread_.join();
}
