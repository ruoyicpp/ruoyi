/**
 * @file EarlyInit.cc
 * @brief 早期初始化阶段（原 main.cc 开头部分）
 *
 * 顺序与原文件一致：
 *   1. Windows 控制台 UTF-8 + 彩色输出 + RUOYI 横幅
 *   2. 工作目录切换到 exe 所在目录
 *   3. watchdog 移交（直接双击 exe → 转交 watchdog）
 *   4. 单实例锁（Windows Mutex / Linux PID 文件）
 *   5. WorkerOrchestrator 多进程编排分支
 *
 * 返回值语义：std::nullopt 继续启动；有值则该值即进程退出码
 * （对应原 main() 内的 return 语句）。
 */

#include "AppBootstrap.h"

namespace boot {

std::optional<int> earlyInit(int argc, char* argv[], AppContext& ctx) {
#ifdef _WIN32
    // Windows 控制台设置 UTF-8 编码（参考 config-center-gateway）
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    // 提前安装彩色输出（启用 VT，替换 std::cout streambuf），
    // 这样下面的 RUOYI 横幅的 ANSI 转义可以被 Windows 控制台正确解释为蓝色
    ColorLogger::install();
    std::cout <<
        "\x1b[1;34m"  // B_BLUE：RUOYI-CPP 横幅
        "\n"
        "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97      \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \n"
        "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\n"
        "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\n"
        "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d \n"
        "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \n"
        "  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d    \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d   \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d      \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d     \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d     \n"
        "\x1b[0m\n";  // 重置颜色
    // 将工作目录切换到 exe 所在目录，确保双击运行时能找到 config.json
#ifdef _WIN32
    {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        ctx.exePath = std::filesystem::path(buf).parent_path();
    }
#else
    {
        std::error_code ec;
        ctx.exePath = std::filesystem::canonical("/proc/self/exe", ec).parent_path();
    }
#endif
    if (!ctx.exePath.empty()) {
        std::filesystem::current_path(ctx.exePath);
    }
    auto& exePath = ctx.exePath;   // 保持与原代码一致的引用名

    // ── watchdog 移交：直接双击 exe 时自动转交给同目录 watchdog ──────────
    // watchdog 启动本进程时会追加 --launched-by-watchdog，没有该标记说明直接双击。
    {
        bool fromWatchdog = false;
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--launched-by-watchdog")
                { fromWatchdog = true; break; }

        // Orchestrator fork 的 worker 不应再移交 watchdog，否则每个 worker
        // 都会启动一个 watchdog → 新 ruoyi-cpp → 进入 orchestrator → 再 fork
        // worker → 无限递归，指数级进程爆炸。
        bool isOrchestratorWorker = (std::getenv("RUOYI_WORKER_INDEX") != nullptr);
        // --no-watchdog：显式跳过 watchdog 移交，直接运行服务器
        bool noWatchdog = false;
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--no-watchdog")
                { noWatchdog = true; break; }

        // 诊断日志：打印进程启动参数，便于排查 Linux 进程爆炸问题
        {
            std::string allArgs;
            for (int i = 0; i < argc; ++i) allArgs += std::string(argv[i]) + " ";
            const char* wIdx = std::getenv("RUOYI_WORKER_INDEX");
#ifdef _WIN32
            std::cerr << "[DIAG] pid=" << GetCurrentProcessId()
                      << " ppid=0"
#else
            std::cerr << "[DIAG] pid=" << getpid()
                      << " ppid=" << getppid()
#endif
                      << " args=[" << allArgs << "]"
                      << " fromWatchdog=" << fromWatchdog
                      << " isOrchestratorWorker=" << isOrchestratorWorker
                      << " noWatchdog=" << noWatchdog
                      << " workerIndex=" << (wIdx ? wIdx : "null")
                      << std::endl;
        }

        if (!fromWatchdog && !isOrchestratorWorker && !noWatchdog) {
#ifdef _WIN32
            const char* wdExe = "watchdog.exe";
#else
            const char* wdExe = "./watchdog";
#endif
            if (std::filesystem::exists(wdExe)) {
                std::cout << "[info] watchdog detected, handing off..." << std::endl;
#ifdef _WIN32
                std::string cmd = std::string("\"") + wdExe + "\"";
                STARTUPINFOA si{}; si.cb = sizeof(si);
                PROCESS_INFORMATION pi{};
                std::vector<char> buf(cmd.begin(), cmd.end()); buf.push_back('\0');
                if (CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE,
                                   0, nullptr, nullptr, &si, &pi)) {
                    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
                    return 0;
                }
#else
                pid_t p = ::fork();
                if (p == 0) { ::execl(wdExe, wdExe, nullptr); ::_exit(1); }
                if (p > 0) return 0;
#endif
            }
        }

        // ── 单实例锁（防止多实例堆损坏）────────
#ifdef _WIN32
        if (fromWatchdog && std::getenv("RUOYI_WORKER_INDEX") == nullptr) {
            static HANDLE s_singleInstance =
                CreateMutexA(nullptr, FALSE, "Local\\ruoyi-cpp-singleton");
            if (s_singleInstance && GetLastError() == ERROR_ALREADY_EXISTS) {
                std::cerr << "[FATAL] 已有一个 ruoyi-cpp 主进程在运行，请先关闭旧实例" << std::endl;
                if (std::getenv("RUOYI_NO_PAUSE") == nullptr && _isatty(_fileno(stdin)))
                    std::cin.get();
                return 2;
            }
        }
#else
        // Linux：用 PID 文件 + flock 防止多实例
        {
            std::string pidFile = ".ruoyi-cpp.pid";
            std::ifstream checkPid(pidFile);
            if (checkPid.is_open()) {
                long existingPid = 0;
                checkPid >> existingPid;
                checkPid.close();
                if (existingPid > 0 && kill(existingPid, 0) == 0) {
                    std::cerr << "[FATAL] 已有 ruoyi-cpp 主进程在运行 (pid="
                              << existingPid << ")，请先关闭旧实例" << std::endl;
                    return 2;
                }
            }
            std::ofstream ofs(pidFile);
            if (ofs.is_open()) {
                ofs << getpid();
                ofs.close();
                std::atexit([]{ std::filesystem::remove(".ruoyi-cpp.pid"); });
            }
        }
#endif
    }

    // ── 多进程编排器分支 ──────────────────────────────────────────────────
    // 1) 子进程（env 中已带 RUOYI_WORKER_INDEX）：直接走单进程流程
    // 2) 主进程 + app.worker_processes>1：进入 orchestrator，spawn 子进程并监控
    // 3) 主进程 + worker_processes<=1：正常单进程模式
    {
        int wkIdx = WorkerOrchestrator::currentWorkerIndex();
        if (wkIdx < 0) {
            // 这里是主进程，先读 config 判断是否进编排器
            int workerCount = 1;
            std::string preCfg = "config.json";
            for (int i = 1; i < argc - 1; ++i) {
                if (std::string(argv[i]) == "--config") { preCfg = argv[i+1]; break; }
            }
            std::ifstream cf(preCfg);
            if (cf.is_open()) {
                Json::Value pre; Json::CharReaderBuilder rb; std::string er;
                if (Json::parseFromStream(rb, cf, &pre, &er) && pre.isMember("app")) {
                    workerCount = pre["app"].get("worker_processes", 1).asInt();
                }
            }
#ifdef _WIN32
            std::cerr << "[DIAG] pid=" << GetCurrentProcessId()
#else
            std::cerr << "[DIAG] pid=" << getpid()
#endif
                      << " worker_processes=" << workerCount
                      << " config=" << preCfg << std::endl;
            if (workerCount > 1) {
                WorkerOrchestrator::Config oc;
                oc.enabled     = true;
                oc.workerCount = workerCount;
#ifdef _WIN32
                oc.exePath     = (exePath / "ruoyi-cpp.exe").string();
#else
                oc.exePath     = (exePath / "ruoyi-cpp").string();
#endif
                // 透传原始命令行（去掉 argv[0]）
                for (int i = 1; i < argc; ++i) oc.extraArgs.emplace_back(argv[i]);
                return WorkerOrchestrator::instance().run(oc);
            }
        } else {
            std::cout << "[Worker] index=" << wkIdx << " pid=" <<
#ifdef _WIN32
                GetCurrentProcessId()
#else
                getpid()
#endif
                << std::endl;
        }
    }

    return std::nullopt;
}

} // namespace boot
