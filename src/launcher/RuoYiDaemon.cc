// =============================================================
// RuoYiDaemon.cc — 独立守护进程
//
// 用法：
//   Windows: RuoYiDaemon.exe [--exe path/to/ruoyi-cpp.exe] [--work-dir ./]
//   Linux:   RuoYiDaemon     [--exe path/to/ruoyi-cpp]     [--work-dir ./]
// 功能：
//   - 启动并监控 ruoyi-cpp
//   - 若进程崩溃，自动重启（指数退避）
//   - 收到 Ctrl+C 时优雅关闭子进程
// =============================================================

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
    std::atomic<bool> g_shutdown{false};

#ifdef _WIN32
    BOOL WINAPI consoleCtrlHandler(DWORD ctrl) {
        if (ctrl == CTRL_C_EVENT || ctrl == CTRL_BREAK_EVENT ||
            ctrl == CTRL_CLOSE_EVENT || ctrl == CTRL_SHUTDOWN_EVENT) {
            g_shutdown.store(true);
            return TRUE;
        }
        return FALSE;
    }
#else
    void unixSignalHandler(int) {
        g_shutdown.store(true);
    }
#endif
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    std::string exePath = "ruoyi-cpp.exe";
#else
    std::string exePath = "./ruoyi-cpp";
#endif
    std::string workDir = "./";
    int maxRestarts = 10;
    int restartBackoffMs = 2000;

    // 解析命令行参数
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--exe") {
            exePath = argv[++i];
        } else if (std::string(argv[i]) == "--work-dir") {
            workDir = argv[++i];
        }
    }

    std::cout << "[RuoYiDaemon] Starting daemon for: " << exePath << std::endl;
    std::cout << "[RuoYiDaemon] Work directory: " << workDir << std::endl;

    // 注册信号处理
#ifdef _WIN32
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#else
    struct sigaction sa{};
    sa.sa_handler = unixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    signal(SIGCHLD, SIG_IGN);
#endif

    int restartCount = 0;

#ifdef _WIN32
    HANDLE hProc = nullptr;
    DWORD pid = 0;

    while (!g_shutdown.load()) {
        // 启动子进程
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION pi{};

        BOOL ok = CreateProcessA(nullptr, const_cast<char*>(exePath.c_str()),
                                 nullptr, nullptr, TRUE,
                                 0, nullptr, workDir.c_str(), &si, &pi);
        if (!ok) {
            std::cerr << "[RuoYiDaemon] CreateProcess failed: " << GetLastError() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(restartBackoffMs));
            continue;
        }

        hProc = pi.hProcess;
        pid = pi.dwProcessId;
        CloseHandle(pi.hThread);

        std::cout << "[RuoYiDaemon] Process started: pid=" << pid << std::endl;
        restartCount = 0;

        // 等待进程退出
        WaitForSingleObject(hProc, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(hProc, &exitCode);
        CloseHandle(hProc);
        hProc = nullptr;

        if (g_shutdown.load()) {
            std::cout << "[RuoYiDaemon] Shutdown requested, exiting" << std::endl;
            break;
        }

        std::cout << "[RuoYiDaemon] Process exited with code: " << exitCode << std::endl;

        if (restartCount >= maxRestarts) {
            std::cerr << "[RuoYiDaemon] Max restarts exceeded, giving up" << std::endl;
            break;
        }

        restartCount++;
        int backoffMs = restartBackoffMs * (1 << (restartCount - 1));
        std::cout << "[RuoYiDaemon] Restarting in " << backoffMs << "ms (attempt "
                  << restartCount << "/" << maxRestarts << ")" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
    }

#else
    pid_t pid = 0;

    while (!g_shutdown.load()) {
        pid = fork();
        if (pid == 0) {
            // 子进程：执行目标主程序
            chdir(workDir.c_str());
            execl(exePath.c_str(), exePath.c_str(), nullptr);
            _exit(127);
        }
        if (pid < 0) {
            std::cerr << "[RuoYiDaemon] fork failed" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(restartBackoffMs));
            continue;
        }

        std::cout << "[RuoYiDaemon] Process started: pid=" << pid << std::endl;
        restartCount = 0;

        // 等待子进程
        int status;
        waitpid(pid, &status, 0);

        if (g_shutdown.load()) {
            std::cout << "[RuoYiDaemon] Shutdown requested, exiting" << std::endl;
            break;
        }

        std::cout << "[RuoYiDaemon] Process exited with status: " << status << std::endl;

        if (restartCount >= maxRestarts) {
            std::cerr << "[RuoYiDaemon] Max restarts exceeded, giving up" << std::endl;
            break;
        }

        restartCount++;
        int backoffMs = restartBackoffMs * (1 << (restartCount - 1));
        std::cout << "[RuoYiDaemon] Restarting in " << backoffMs << "ms (attempt "
                  << restartCount << "/" << maxRestarts << ")" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
    }
#endif

    std::cout << "[RuoYiDaemon] Daemon stopped" << std::endl;
    return 0;
}
