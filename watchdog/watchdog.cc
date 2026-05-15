// watchdog.cc — 跨平台守护进程（零依赖，纯 C++17 标准库）
// 编译：
//   Windows: g++ -std=c++17 -O2 -static-libgcc -static-libstdc++ -o watchdog.exe watchdog.cc
//   Linux:   g++ -std=c++17 -O2 -o watchdog watchdog.cc
//
// 用法：
//   watchdog.exe [选项]
//   watchdog.exe --config watchdog.ini      # 读配置文件
//   watchdog.exe --exe ruoyi-cpp.exe --workdir . --args "--config config.json"
//
// 配置文件 watchdog.ini（key=value，# 注释）：
//   exe             = ruoyi-cpp.exe
//   workdir         = .
//   args            = --config config.json
//   restart_delay   = 3
//   max_restarts    = 0
//   log_file        = watchdog.log
//   check_interval  = 1000

#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <tlhelp32.h>
#else
#  include <cerrno>
#  include <csignal>
#  include <cstring>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

// ── 全局退出信号 ────────────────────────────────────────────────────────────
static std::atomic<bool> g_quit{false};

#ifdef _WIN32
static BOOL WINAPI ctrlHandler(DWORD) {
    g_quit.store(true);
    return TRUE;
}
#else
static void sigHandler(int) { g_quit.store(true); }
#endif

// ── 日志 ────────────────────────────────────────────────────────────────────
static std::ofstream g_logFile;

static std::string g_logPath;
static long long   g_maxLogSize  = 10 * 1024 * 1024;
static int         g_maxLogFiles = 5;

static void rotateLog() {
    if (g_logPath.empty() || g_maxLogSize <= 0) return;
    std::error_code ec;
    auto sz = (long long)std::filesystem::file_size(g_logPath, ec);
    if (ec || sz < g_maxLogSize) return;
    g_logFile.close();
    for (int i = g_maxLogFiles - 1; i >= 1; --i) {
        std::string from = g_logPath + "." + std::to_string(i);
        std::string to   = g_logPath + "." + std::to_string(i + 1);
        std::filesystem::rename(from, to, ec);
    }
    std::filesystem::rename(g_logPath, g_logPath + ".1", ec);
    g_logFile.open(g_logPath, std::ios::app);
}

// 格式："2026-05-15 21:00:01 [INFO ] 消息  key=value ..."
static void wlog(const std::string& tag, const std::string& msg,
                 const std::string& level = "INFO", long long pid = -1) {
    // 级别对齐到 5 字符
    std::string lvl = level;
    while (lvl.size() < 5) lvl += ' ';

    // 时间改用空格分隔（更可读）
    auto t = std::time(nullptr);
    char tbuf[24];
    std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    std::string kv;
    if (!tag.empty()) kv += " tag=" + tag;
    if (pid  >= 0)    kv += " pid=" + std::to_string(pid);

    std::string line = std::string(tbuf) + " [" + lvl + "] " + msg
                       + (kv.empty() ? "" : "  " + kv.substr(1));
    std::cout << line << std::endl;
    if (g_logFile.is_open()) {
        rotateLog();
        g_logFile << line << "\n" << std::flush;
    }
}

// ── 配置 ────────────────────────────────────────────────────────────────────
struct Config {
    std::string exe            = "ruoyi-cpp.exe";
    std::string workdir        = ".";
    std::string args;
    int         restartDelay   = 3;     // 秒
    int         maxRestarts    = 0;     // 0=无限
    std::string logFile        = "watchdoglogs/watchdog.hjson";
    int         checkInterval  = 1000;  // ms
    int         count             = 1;     // 同时监控的进程数（多实例模式）
    int         heartbeatTimeout  = 10;    // 心跳超时秒数，0=不检查
    std::string heartbeatFile     = ".watchdog_heartbeat";
    int         graceSeconds      = 30;    // 进程刚启动后的宽限期，期间不检查心跳
    long long   maxLogSize        = 10 * 1024 * 1024; // 单文件上限字节，0=不限
    int         maxLogFiles       = 5;                // 最多保留几个备份
};

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static Config loadIni(const std::string& path) {
    Config c;
    std::ifstream f(path);
    if (!f.is_open()) return c;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trim(line.substr(0, eq));
        std::string v = trim(line.substr(eq + 1));
        if      (k == "exe")            c.exe           = v;
        else if (k == "workdir")        c.workdir       = v;
        else if (k == "args")           c.args          = v;
        else if (k == "restart_delay")  c.restartDelay  = std::stoi(v);
        else if (k == "max_restarts")   c.maxRestarts   = std::stoi(v);
        else if (k == "log_file")       c.logFile       = v;
        else if (k == "check_interval") c.checkInterval = std::stoi(v);
        else if (k == "count")              c.count            = std::stoi(v);
        else if (k == "heartbeat_timeout")  c.heartbeatTimeout = std::stoi(v);
        else if (k == "heartbeat_file")     c.heartbeatFile    = v;
        else if (k == "grace_seconds")      c.graceSeconds     = std::stoi(v);
        else if (k == "max_log_size")       c.maxLogSize       = std::stoll(v);
        else if (k == "max_log_files")      c.maxLogFiles      = std::stoi(v);
    }
    return c;
}

static Config parseCli(int argc, char* argv[]) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
        if      (a == "--config")         c = loadIni(next());
        else if (a == "--exe")            c.exe          = next();
        else if (a == "--workdir")        c.workdir      = next();
        else if (a == "--args")           c.args         = next();
        else if (a == "--restart-delay")  c.restartDelay = std::stoi(next());
        else if (a == "--max-restarts")   c.maxRestarts  = std::stoi(next());
        else if (a == "--log-file")       c.logFile      = next();
        else if (a == "--check-interval") c.checkInterval= std::stoi(next());
        else if (a == "--count")              c.count           = std::stoi(next());
        else if (a == "--heartbeat-timeout")  c.heartbeatTimeout= std::stoi(next());
        else if (a == "--heartbeat-file")     c.heartbeatFile   = next();
        else if (a == "--grace-seconds")      c.graceSeconds    = std::stoi(next());
        else if (a == "--max-log-size")       c.maxLogSize      = std::stoll(next());
        else if (a == "--max-log-files")      c.maxLogFiles     = std::stoi(next());
    }
    return c;
}

// ── 平台进程封装 ─────────────────────────────────────────────────────────────
#ifdef _WIN32

struct Process {
    HANDLE hProcess = INVALID_HANDLE_VALUE;
    HANDLE hThread  = INVALID_HANDLE_VALUE;
    DWORD  pid      = 0;

    bool start(const Config& cfg) {
        // 自动加标记，让子进程知道自己由 watchdog 启动（避免双击 exe 时递归启动）
        std::string extraArgs = cfg.args;
        if (extraArgs.find("--launched-by-watchdog") == std::string::npos)
            extraArgs += (extraArgs.empty() ? "" : " ") + std::string("--launched-by-watchdog");
        std::string cmd = "\"" + cfg.exe + "\"";
        if (!extraArgs.empty()) cmd += " " + extraArgs;
        std::vector<char> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back('\0');

        STARTUPINFOA si{}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessA(
                nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
                0, nullptr,
                cfg.workdir.empty() ? nullptr : cfg.workdir.c_str(),
                &si, &pi)) {
            wlog("", "启动失败 CreateProcess error=" + std::to_string(GetLastError()), "ERROR");
            return false;
        }
        hProcess = pi.hProcess;
        hThread  = pi.hThread;
        pid      = pi.dwProcessId;
        CloseHandle(hThread); hThread = INVALID_HANDLE_VALUE;
        return true;
    }

    // 等待退出，返回 true=已退出，false=超时
    bool waitFor(int ms) {
        if (hProcess == INVALID_HANDLE_VALUE) return true;
        return WaitForSingleObject(hProcess, ms) == WAIT_OBJECT_0;
    }

    int exitCode() {
        DWORD code = 0;
        GetExitCodeProcess(hProcess, &code);
        return (int)code;
    }

    void kill() {
        if (hProcess != INVALID_HANDLE_VALUE)
            TerminateProcess(hProcess, 1);
    }

    void close() {
        if (hProcess != INVALID_HANDLE_VALUE) {
            CloseHandle(hProcess);
            hProcess = INVALID_HANDLE_VALUE;
        }
        pid = 0;
    }

    bool running() {
        if (hProcess == INVALID_HANDLE_VALUE) return false;
        DWORD code = STILL_ACTIVE;
        GetExitCodeProcess(hProcess, &code);
        return code == STILL_ACTIVE;
    }
};

#else // Linux/macOS

struct Process {
    pid_t pid = -1;
    int   lastExit = 0;

    bool start(const Config& cfg) {
        // 自动加标记，让子进程知道自己由 watchdog 启动
        std::string extraArgs = cfg.args;
        if (extraArgs.find("--launched-by-watchdog") == std::string::npos)
            extraArgs += (extraArgs.empty() ? "" : " ") + std::string("--launched-by-watchdog");
        pid_t p = fork();
        if (p < 0) {
            wlog("", std::string("启动失败 fork: ") + strerror(errno), "ERROR");
            return false;
        }
        if (p == 0) {
            if (!cfg.workdir.empty()) chdir(cfg.workdir.c_str());
            std::vector<std::string> parts;
            std::istringstream ss(cfg.exe + (extraArgs.empty() ? "" : " " + extraArgs));
            std::string tok;
            while (ss >> tok) parts.push_back(tok);
            std::vector<char*> argv;
            for (auto& s : parts) argv.push_back(s.data());
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            std::cerr << "[watchdog] execvp failed: " << strerror(errno) << std::endl;
            _exit(127);
        }
        pid = p;
        return true;
    }

    bool waitFor(int ms) {
        if (pid < 0) return true;
        auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < deadline) {
            int status;
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) {
                lastExit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                pid = -1;
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }

    int exitCode() { return lastExit; }

    void kill() {
        if (pid > 0) ::kill(pid, SIGTERM);
    }

    void close() { pid = -1; }

    bool running() {
        if (pid < 0) return false;
        int status;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            lastExit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            pid = -1;
            return false;
        }
        return true;
    }
};

#endif

// ── 主循环 ──────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleCtrlHandler(ctrlHandler, TRUE);
#else
    signal(SIGINT,  sigHandler);
    signal(SIGTERM, sigHandler);
    signal(SIGHUP,  sigHandler);
#endif

    // 默认先尝试读 watchdog.ini
    Config cfg = loadIni("watchdog.ini");
    // 命令行参数覆盖
    if (argc > 1) cfg = parseCli(argc, argv);

    if (!cfg.logFile.empty()) {
        g_logPath     = cfg.logFile;
        g_maxLogSize  = cfg.maxLogSize;
        g_maxLogFiles = cfg.maxLogFiles;
        // 自动创建日志目录
        std::filesystem::path lp(cfg.logFile);
        if (lp.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(lp.parent_path(), ec);
        }
        g_logFile.open(cfg.logFile, std::ios::app);
    }

    int n = std::max(1, cfg.count);
    wlog("", "守护进程启动，监控: " + cfg.exe
         + "  实例数=" + std::to_string(n)
         + "  重启延迟=" + std::to_string(cfg.restartDelay) + "s"
         + (cfg.maxRestarts ? "  最大重启=" + std::to_string(cfg.maxRestarts) : ""));

    // 每个实例一个独立监控线程
    std::vector<Process>     procs(n);
    std::vector<std::thread> threads;
    threads.reserve(n);

    for (int idx = 0; idx < n; ++idx) {
        threads.emplace_back([idx, n, &cfg, &procs]() {
            std::string tag = (n > 1) ? ("[#" + std::to_string(idx) + "]") : "";
            Process& proc = procs[idx];
            int restarts = 0;
            auto procStartTime = std::chrono::steady_clock::now();

            while (!g_quit.load()) {
                if (!proc.running()) {
                    if (restarts > 0)
                        wlog(tag, "进程退出，退出码=" + std::to_string(proc.exitCode()), "WARN");

                    if (cfg.maxRestarts > 0 && restarts >= cfg.maxRestarts) {
                        wlog(tag, "已达最大重启次数（" + std::to_string(cfg.maxRestarts) + "），停止监控");
                        break;
                    }

                    if (restarts > 0) {
                        wlog(tag, std::to_string(cfg.restartDelay) + " 秒后重启...");
                        for (int i = 0; i < cfg.restartDelay * 10 && !g_quit.load(); ++i)
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        if (g_quit.load()) break;
                    }

                    proc.close();
                    // 删除旧心跳文件，防止残留文件触发立即超时
                    { std::error_code ec; std::filesystem::remove(cfg.heartbeatFile, ec); }
                    if (!proc.start(cfg)) {
                        wlog(tag, "启动失败，" + std::to_string(cfg.restartDelay) + " 秒后重试...", "ERROR");
                        std::this_thread::sleep_for(std::chrono::seconds(cfg.restartDelay));
                        ++restarts;
                        continue;
                    }
                    procStartTime = std::chrono::steady_clock::now();
                    wlog(tag, "已启动", "INFO", (long long)proc.pid);
                    ++restarts;
                }
                // ── 心跳超时检查（宽限期内不检查，防止残留旧文件误判）────────
                if (cfg.heartbeatTimeout > 0 && proc.running()) {
                    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - procStartTime).count();
                    if (uptime >= cfg.graceSeconds) {
                        std::error_code ec;
                        auto mtime = std::filesystem::last_write_time(cfg.heartbeatFile, ec);
                        if (!ec) {
                            auto now = std::filesystem::file_time_type::clock::now();
                            int age = (int)std::chrono::duration_cast<std::chrono::seconds>(now - mtime).count();
                            if (age > cfg.heartbeatTimeout) {
                                wlog(tag, "心跳超时（" + std::to_string(age) + "s），强制重启", "WARN");
                                proc.kill();
                                proc.waitFor(3000);
                                proc.close();
                            }
                        }
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(cfg.checkInterval));
            }

            proc.kill();
            proc.waitFor(5000);
            proc.close();
            wlog(tag, "监控已停止");
        });
    }

    for (auto& t : threads) t.join();
    wlog("", "守护进程已退出。");
    return 0;
}
