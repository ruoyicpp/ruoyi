#include "AppIncludes.h"
#include "common/ColorLogger.h"
#ifdef RUOYI_USE_EMBEDDED_FRONTEND
#  include "common/EmbeddedFrontend.h"
#endif
#include "common/NginxLikeFeatures.h"
#include "common/NginxEmbedded.h"
#include "services/WorkerOrchestrator.h"
#include "services/AcmeManager.h"
#include "services/CertManagerDriver.h"
#ifdef _WIN32
#  include <io.h>      // _isatty / _fileno
#endif

// 从 config.json 的 database 节构造 libpq 连接串（兼容旧调用）
static std::string buildDbConnStr(const std::string& cfgFile, int timeout = 5) {
    std::ifstream f(cfgFile);
    if (!f.is_open()) return {};
    Json::Value root; Json::CharReaderBuilder rb; std::string errs;
    if (!Json::parseFromStream(rb, f, &root, &errs)) return {};
    if (!root.isMember("database")) return {};
    auto& d = root["database"];
    return "host="     + d.get("host",   "127.0.0.1").asString()
         + " port="   + std::to_string(d.get("port", 5432).asInt())
         + " dbname=" + d.get("dbname", "ruoyi").asString()
         + " user="   + d.get("user",   "postgres").asString()
         + " password=" + d.get("passwd", "").asString()
         + " connect_timeout=" + std::to_string(timeout);
}

// ConfigLoader 版本：passwd 若为空则从 Vault 补全
static std::string buildDbConnStr(const ConfigLoader& loader, int timeout = 5) {
    auto& d = loader.raw()["database"];
    return "host="     + loader.get("database", "host",   "127.0.0.1")
         + " port="   + std::to_string(d.get("port", 5432).asInt())
         + " dbname=" + loader.get("database", "dbname", "ruoyi")
         + " user="   + loader.get("database", "user",   "postgres")
         + " password=" + loader.get("database", "passwd", "")
         + " connect_timeout=" + std::to_string(timeout);
}

// SecurityUtils.cc 暴露的 OpenSSL 3.x default provider 早期初始化函数
extern "C" void ruoyi_init_openssl_provider();

int main(int argc, char* argv[]) {
    ColorLogger::install(); // 开启彩色控制台输出（Windows VT + trantor 拦截）
    // 最先安装崩溃日志捕获，确保任何时刻崩溃都有记录
    CrashHandler::install("./logs");

    // ── 主线程单线程下完成 OpenSSL 全局 init ─────────────────────
    // 不能延迟到 drogon worker 线程 lazy 加载，否则与 libpq.dll 间接依赖的
    // system OpenSSL DLL 在多线程并发时引发 RtlReAllocateHeap 堆损坏崩溃
    ruoyi_init_openssl_provider();

#ifdef _WIN32
    // ── 单实例锁：防止多个 ruoyi-cpp 主进程同时启动导致堆损坏 ─────
    // 多进程 worker 模式（RUOYI_WORKER_INDEX 已设）时跳过，让 Orchestrator
    // 主进程持锁、各子进程仍能正常初始化各自的资源
    if (std::getenv("RUOYI_WORKER_INDEX") == nullptr) {
        // Local namespace 锁，避免污染 Global 命名空间需要管理员权限
        static HANDLE s_singleInstance =
            CreateMutexA(nullptr, FALSE, "Local\\ruoyi-cpp-singleton");
        if (s_singleInstance && GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cerr << "[FATAL] 已有一个 ruoyi-cpp 主进程在运行（互斥锁 "
                         "Local\\ruoyi-cpp-singleton 已被占用），"
                         "请先关闭旧实例再启动" << std::endl;
            // 仅当从控制台交互运行时才等回车；服务/脚本场景直接退出
            if (std::getenv("RUOYI_NO_PAUSE") == nullptr && _isatty(_fileno(stdin))) {
                std::cout << "按回车键退出..." << std::endl;
                std::cin.get();
            }
            return 2;
        }
    }
#endif

    try {
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
            "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\n"
            "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d \n"
            "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \n"
            "  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d    \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d   \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d      \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d     \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d     \n"
            "\x1b[0m\n";  // 重置颜色
        // 将工作目录切换到 exe 所在目录，确保双击运行时能找到 config.json
        std::filesystem::path exePath;
#ifdef _WIN32
        {
            wchar_t buf[MAX_PATH];
            GetModuleFileNameW(nullptr, buf, MAX_PATH);
            exePath = std::filesystem::path(buf).parent_path();
        }
#else
        {
            std::error_code ec;
            exePath = std::filesystem::canonical("/proc/self/exe", ec).parent_path();
        }
#endif
        if (!exePath.empty()) {
            std::filesystem::current_path(exePath);
        }

        // ── watchdog 移交：直接双击 exe 时自动转交给同目录 watchdog ──────────
        // watchdog 启动本进程时会追加 --launched-by-watchdog，没有该标记说明直接双击。
        {
            bool fromWatchdog = false;
            for (int i = 1; i < argc; ++i)
                if (std::string(argv[i]) == "--launched-by-watchdog")
                    { fromWatchdog = true; break; }

            if (!fromWatchdog) {
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
                if (workerCount > 1) {
                    WorkerOrchestrator::Config oc;
                    oc.enabled     = true;
                    oc.workerCount = workerCount;
                    oc.exePath     = (exePath / "ruoyi-cpp.exe").string();
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

        // ── 许可证校验（必须在加载配置前完成，工作目录已切换到 exe 目录）──────
        LicenseManager::checkAndPrint();
        LicenseWatcher::instance().start(LicenseManager::_licPath());

        // ── 解析命令行参数 --config <file> ─────────────────────────────────────
        std::string configFile = "config.json";
        for (int i = 1; i < argc - 1; ++i) {
            if (std::string(argv[i]) == "--config") {
                configFile = argv[i + 1];
                break;
            }
        }

        if (!std::filesystem::exists(configFile)) {
            std::cerr << "[错误] 找不到 " << configFile << "，请将其放到 exe 同目录下" << std::endl;
            std::cerr << "当前目录: " << std::filesystem::current_path() << std::endl;
            std::cout << "按回车键退出..." << std::endl;
            std::cin.get();
            return 1;
        }

        // ── 预读集群配置（决定实例角色）─────────────────────────────────────────
        std::string instanceRole = "primary"; // primary | worker
        std::vector<std::string> clusterBackends;
        std::string nginxPrefix = "nginx/";
        {
            std::ifstream ccf(configFile);
            if (ccf.is_open()) {
                Json::Value root; Json::CharReaderBuilder rb; std::string errs;
                if (Json::parseFromStream(rb, ccf, &root, &errs)) {
                    if (root.isMember("cluster")) {
                        instanceRole = root["cluster"].get("instance_role", "primary").asString();
                        if (root["cluster"].isMember("backends"))
                            for (auto& b : root["cluster"]["backends"])
                                clusterBackends.push_back(b.asString());
                    }
                    if (root.isMember("nginx"))
                        nginxPrefix = root["nginx"].get("prefix", "nginx/").asString();
                }
            }
        }
        bool isPrimary = (instanceRole == "primary");
        std::cout << "[Cluster] role=" << instanceRole
                  << " backends=" << clusterBackends.size() << std::endl;

        // ── 主实例：生成 nginx upstream.conf ─────────────────────────────────────
        if (isPrimary && !clusterBackends.empty()) {
            std::string upstreamPath = nginxPrefix + "conf/upstream.conf";
            std::filesystem::create_directories(nginxPrefix + "conf");
            std::ofstream uf(upstreamPath);
            if (uf.is_open()) {
                uf << "# 由 ruoyi-cpp 主实例自动生成，勿手动修改\n";
                uf << "upstream ruoyi_backend {\n";
                uf << "    least_conn;\n";
                for (auto& b : clusterBackends)
                    uf << "    server " << b << " weight=1 max_fails=3 fail_timeout=30s;\n";
                uf << "    keepalive 32;\n";
                uf << "}\n";
                LOG_INFO << "[Cluster] upstream.conf written: " << upstreamPath;
            }
        }

        // 手动注册 JwtAuthFilter 中间件（isAutoCreation=false）
        drogon::app().registerMiddleware(std::make_shared<JwtAuthFilter>());

        // 加载配置
        drogon::app().loadConfigFile(configFile);

        // ── Config Schema 校验（启动时立即检测，防止静默错误）────────────────
        {
            std::ifstream vcf(configFile);
            Json::Value root; Json::CharReaderBuilder rb; std::string errs;
            bool parseOk = vcf.is_open() && Json::parseFromStream(rb, vcf, &root, &errs);
            auto fatal = [](const std::string& msg) {
                std::cerr << "[CONFIG ERROR] " << msg << std::endl;
                std::cout << "请检查 config.json，按回车键退出..." << std::endl;
                std::cin.get();
                std::exit(1);
            };
            if (!parseOk) fatal("config.json 解析失败: " + errs);

            // listeners
            if (!root.isMember("listeners") || !root["listeners"].isArray() || root["listeners"].empty())
                fatal("缺少 listeners 配置（需包含至少一个监听端口）");
            for (auto& l : root["listeners"]) {
                if (!l.isMember("port")) fatal("listeners 中缺少 port 字段");
            }
            // database
            if (!root.isMember("database")) fatal("缺少 database 配置段");
            for (auto& f : {"host","port","dbname","user","passwd"})
                if (!root["database"].isMember(f))
                    fatal(std::string("database 缺少必填字段: ") + f);
            // jwt
            if (!root.isMember("jwt")) fatal("缺少 jwt 配置段");
            if (!root["jwt"].isMember("secret")) fatal("jwt 缺少 secret 字段");
            {
                auto s = root["jwt"].get("secret","").asString();
                if (!s.empty() && s.size() < 16)
                    fatal("jwt.secret 长度不足 16 位，存在安全风险");
            }

            std::cout << "[Config] Schema 校验通过" << std::endl;
        }

        // 日志时间显示为本地时间（默认是 UTC）
        trantor::Logger::setDisplayLocalTime(true);

        // ── 初始化 JSON 日志（覆盖 Drogon 的文本输出函数）────────────────────
        {
            std::string logDir  = "./logs";
            std::string logBase = "ruoyi";
            std::ifstream jcf(configFile);
            if (jcf.is_open()) {
                Json::Value root; Json::CharReaderBuilder rb; std::string errs;
                if (Json::parseFromStream(rb, jcf, &root, &errs)
                    && root.isMember("app") && root["app"].isMember("log")) {
                    auto& lg = root["app"]["log"];
                    logDir  = lg.get("log_path",          "./logs").asString();
                    logBase = lg.get("logfile_base_name",  "ruoyi").asString();
                }
            }
            // log_path 留空表示禁用 drogon AsyncFileLogger；
            // JsonLogger 仍需一个有效目录，回退到默认 ./logs
            if (logDir.empty()) logDir = "./logs";
            size_t maxSizeBytes = 100ULL * 1024 * 1024; // 默认 100MB
            int keepFiles = 5;
            {
                std::ifstream lcf(configFile);
                Json::Value r2; Json::CharReaderBuilder rb2; std::string e2;
                if (lcf.is_open() && Json::parseFromStream(rb2, lcf, &r2, &e2)
                    && r2.isMember("app") && r2["app"].isMember("log")) {
                    auto& lg = r2["app"]["log"];
                    if (lg.isMember("log_size_limit"))
                        maxSizeBytes = (size_t)lg["log_size_limit"].asInt64();
                    keepFiles = lg.get("log_keep_files", 5).asInt();
                }
            }
            std::error_code ec;
            std::filesystem::create_directories(logDir, ec);
            if (ec) {
                std::cerr << "[警告] 无法创建日志目录 '" << logDir
                          << "': " << ec.message() << "，将使用 ./logs" << std::endl;
                logDir = "./logs";
                std::filesystem::create_directories(logDir, ec);
            }
            JsonLogger::instance().init(logDir, logBase, maxSizeBytes, keepFiles);
        }

        // ── 控制台输出重定向（默认重定向到 logs/console.log）──────────────────
        // 项目中有 172 处 std::cout 直接打印，会阻塞 drogon 主事件循环。
        // freopen 让 std::cout 指向的 stdout 流改为文件追加，零代码改动覆盖全部。
        // 想看实时控制台输出请在 config.json 设 app.console_output=true，或 tail -f logs/console.log。
        // 注意：本步骤在启动 banner 之后执行，banner 仍可见。
        {
            bool consoleOutput = false;
            std::string logDirForConsole = "./logs";
            try {
                std::ifstream cf(configFile);
                if (cf.is_open()) {
                    Json::Value root; Json::CharReaderBuilder rb; std::string err;
                    if (Json::parseFromStream(rb, cf, &root, &err)) {
                        consoleOutput = root["app"].get("console_output", false).asBool();
                        if (root["log"].isObject() && root["log"].isMember("log_path")) {
                            auto p = root["log"]["log_path"].asString();
                            if (!p.empty()) logDirForConsole = p;
                        }
                    }
                }
            } catch (...) {}
            if (!consoleOutput) {
                std::error_code ec;
                std::filesystem::create_directories(logDirForConsole, ec);
                const std::string outPath = logDirForConsole + "/console.log";
                // freopen 改变 stdout 内部 fd 指向，所有走 stdout / std::cout / ColorStreambuf 的写入都进入此文件
                if (std::freopen(outPath.c_str(), "a", stdout) == nullptr) {
                    LOG_WARN << "[Log] freopen stdout failed, console output remains on terminal";
                } else {
                    std::setvbuf(stdout, nullptr, _IOLBF, 4096);  // 行缓冲，便于 tail
                    LOG_INFO << "[Log] stdout redirected to " << outPath
                             << " (set app.console_output=true to keep terminal output)";
                }
            } else {
                LOG_INFO << "[Log] console_output=true, stdout stays on terminal";
            }
        }

        LOG_INFO << "Cache backend: " << MemCache::backendInfo();
        std::cout << "[Cache] backend: " << MemCache::backendInfo() << std::endl;

        // ── 启动 Vault 子进程（如已运行则跳过，等就绪后自动解封）──────────────
        {
            std::ifstream vcfF(configFile);
            if (vcfF.is_open()) {
                Json::Value vr; Json::CharReaderBuilder vrb; std::string ve;
                if (Json::parseFromStream(vrb, vcfF, &vr, &ve) && vr.isMember("vault")) {
                    auto& vt = vr["vault"];
                    VaultManagerConfig vmc;
                    vmc.enabled       = vt.get("enabled",      false).asBool();
                    vmc.exePath       = vt.get("exe_path",     "").asString();
                    vmc.configFile    = vt.get("config_file",  "").asString();
                    vmc.addr          = vt.get("addr",         "http://127.0.0.1:8200").asString();
                    vmc.token         = vt.get("token",        "").asString();
                    vmc.unsealKey     = vt.get("unseal_key",   "").asString();
                    // 多 key 模式：unseal_keys 数组（Shamir threshold>1）
                    if (vt.isMember("unseal_keys") && vt["unseal_keys"].isArray()) {
                        for (auto& k : vt["unseal_keys"])
                            vmc.unsealKeys.push_back(k.asString());
                    }
                    vmc.autoStart     = vt.get("auto_start",   true).asBool();
                    vmc.startTimeoutS = vt.get("start_timeout",60).asInt();
                    vmc.psqlExe       = vt.get("psql_exe",      "").asString();
                    vmc.autoInit      = vt.get("auto_init",     true).asBool();
                    vmc.initKeysFile  = vt.get("init_keys_file","vault-init-keys.json").asString();
                    vmc.secretPath    = vt.get("secret_path","secret/ruoyi-cpp").asString();
                    // 收集敏感字段作为首次初始化的 seed
                    auto& jr = vr; // 完整 config JSON
                    auto addSeed = [&](const std::string& sec, const std::string& key) {
                        std::string v = jr.get(sec, Json::Value())[key].asString();
                        if (!v.empty()) vmc.seedSecrets[sec + "_" + key] = v;
                    };
                    addSeed("database", "passwd");
                    addSeed("jwt",      "secret");
                    addSeed("redis",    "password");
                    // sign_verify app secrets
                    if (jr["security"]["sign_verify"]["apps"].isArray()) {
                        for (auto& app : jr["security"]["sign_verify"]["apps"]) {
                            std::string id  = app.get("app_id","").asString();
                            std::string sec2 = app.get("secret","").asString();
                            if (!id.empty() && !sec2.empty())
                                vmc.seedSecrets["sign_" + id + "_secret"] = sec2;
                        }
                    }
                    if (vmc.enabled) {
                        VaultManager::instance().start(vmc);
                        std::atexit([]{ VaultManager::instance().stop(); });
                    }
                }
            }
        }

        // ── 构建 ConfigLoader（config.json + Vault 回退）────────────────────────
        auto cfgLoader = std::make_shared<ConfigLoader>([&]() -> Json::Value {
            std::ifstream clf(configFile);
            Json::Value r; Json::CharReaderBuilder rb; std::string e;
            if (clf.is_open()) Json::parseFromStream(rb, clf, &r, &e);
            // 若 autoInit 产生了新 token，覆盖 JSON 里的旧值
            auto newTok = VaultManager::instance().getToken();
            if (!newTok.empty() && r.isMember("vault"))
                r["vault"]["token"] = newTok;
            return r;
        }());

        // 加载 JWT 配置（secret 若为空则从 Vault 补全）
        JwtUtils::loadConfig();
        {
            auto sec = cfgLoader->get("jwt", "secret", "");
            if (!sec.empty() && sec != JwtUtils::config().secret) {
                JwtUtils::config().secret = sec;
                std::cout << "[ConfigLoader] jwt.secret 已从 Vault 补全" << std::endl;
            }
        }

        // ── 安全配置加载 ────────────────────────────────────────────────────────
        {
            std::ifstream scf("config.json");
            if (scf.is_open()) {
                Json::Value root; Json::CharReaderBuilder rb; std::string errs;
                if (Json::parseFromStream(rb, scf, &root, &errs) && root.isMember("security")) {
                    auto& sec = root["security"];
                    // 限流配置
                    if (sec.isMember("rate_limit")) {
                        auto& rl = sec["rate_limit"];
                        RateLimiter::Config cfg;
                        cfg.enabled       = rl.get("enabled", true).asBool();
                        cfg.maxRequests   = rl.get("max_requests", 200).asInt();
                        cfg.windowSeconds = rl.get("window_seconds", 60).asInt();
                        cfg.banSeconds    = rl.get("ban_seconds", 300).asInt();
                        if (rl.isMember("whitelist"))
                            for (auto& ip : rl["whitelist"])
                                cfg.whitelist.push_back(ip.asString());
                        RateLimiter::instance().configure(cfg);

                        // ── P3-12: 注入 Redis 后端，实现跨进程限流计数 ─────
                        // Redis 不可用时 RateLimiter 自动降级到内存
                        RateLimiter::RedisBackend rb;
                        rb.incrAndExpire = [](const std::string& key, int sec) -> long {
                            auto& rc = RedisConn::instance();
                            auto* c = rc.ctx();
                            if (!c) return -1;
                            const auto k = rc.prefixKey(key);
                            auto* r = (redisReply*)redisCommand(c, "INCR %s", k.c_str());
                            if (!r) { rc.markBad(); return -1; }
                            long n = (r->type == REDIS_REPLY_INTEGER) ? r->integer : -1;
                            freeReplyObject(r);
                            // 首次设 EXPIRE（n==1 时）
                            if (n == 1 && sec > 0) {
                                auto* r2 = (redisReply*)redisCommand(c, "EXPIRE %s %d",
                                                                     k.c_str(), sec);
                                if (r2) freeReplyObject(r2);
                            }
                            return n;
                        };
                        rb.setBan = [](const std::string& key, int sec) -> bool {
                            return redisSetEx(key, "1", sec);
                        };
                        rb.isBanned = [](const std::string& key) -> bool {
                            return redisGet(key).has_value();
                        };
                        rb.delKey = [](const std::string& key) {
                            redisDel(key);
                        };
                        if (RedisConn::instance().enabledByConfig()) {
                            RateLimiter::instance().setRedisBackend(rb);
                        }

                        LOG_INFO << "[RateLimit] enabled=" << cfg.enabled
                                 << " max=" << cfg.maxRequests
                                 << "/" << cfg.windowSeconds << "s"
                                 << " backend=" << (RedisConn::instance().enabledByConfig()
                                                    ? "redis(+memory fallback)"
                                                    : "memory");
                    }
                    // 签名验签配置
                    if (sec.isMember("sign_verify")) {
                        auto& sv = sec["sign_verify"];
                        if (sv.get("enabled", false).asBool() && sv.isMember("apps")) {
                            int tol = sv.get("timestamp_tolerance", 300).asInt();
                            std::vector<SignUtils::AppInfo> apps;
                            for (auto& a : sv["apps"])
                                apps.push_back({a["app_id"].asString(),
                                                a["secret"].asString(), true});
                            SignUtils::instance().configure(apps, tol);
                            LOG_INFO << "[Sign] " << apps.size() << " app(s) registered";
                        }
                    }
                }
            }
        }

        // ── CORS（从 config.json cors 段读取，无需重新编译）─────────────────────
        {
            struct CorsCfg {
                std::vector<std::string> origins;
                std::string methods;
                std::string headers;
                std::string expose;
                bool credentials = false;
            };
            auto corsCfg = std::make_shared<CorsCfg>();
            std::ifstream ccf("config.json");
            if (ccf.is_open()) {
                Json::Value root; Json::CharReaderBuilder rb; std::string errs;
                if (Json::parseFromStream(rb, ccf, &root, &errs) && root.isMember("cors")) {
                    auto& c = root["cors"];
                    if (c.isMember("allow_origins"))
                        for (auto& o : c["allow_origins"]) corsCfg->origins.push_back(o.asString());
                    if (c.isMember("allow_methods")) {
                        std::string m;
                        for (auto& v : c["allow_methods"]) { if (!m.empty()) m+=','; m+=v.asString(); }
                        corsCfg->methods = m;
                    }
                    if (c.isMember("allow_headers")) {
                        std::string h;
                        for (auto& v : c["allow_headers"]) { if (!h.empty()) h+=','; h+=v.asString(); }
                        corsCfg->headers = h;
                    }
                    if (c.isMember("expose_headers")) {
                        std::string e;
                        for (auto& v : c["expose_headers"]) { if (!e.empty()) e+=','; e+=v.asString(); }
                        corsCfg->expose = e;
                    }
                    corsCfg->credentials = c.get("allow_credentials", false).asBool();
                }
            }
            // 若 menu.api_base_url 已配置，自动将其 origin 加入 CORS 白名单
            // 避免生产域名只填了 api_base_url 而忘了在 cors.allow_origins 重复填写
            {
                std::ifstream maf("config.json");
                if (maf.is_open()) {
                    Json::Value mr; Json::CharReaderBuilder rb2; std::string err2;
                    if (Json::parseFromStream(rb2, maf, &mr, &err2)
                        && mr.isMember("menu") && mr["menu"].isMember("api_base_url")) {
                        std::string abu = mr["menu"]["api_base_url"].asString();
                        if (!abu.empty()) {
                            // 提取 scheme://host[:port]
                            auto pos = abu.find("://");
                            if (pos != std::string::npos) {
                                auto rest = abu.substr(pos + 3);
                                auto slash = rest.find('/');
                                std::string origin = abu.substr(0, pos + 3)
                                    + (slash != std::string::npos ? rest.substr(0, slash) : rest);
                                // 避免重复
                                bool found = false;
                                for (auto& o : corsCfg->origins) if (o == origin) { found = true; break; }
                                if (!found) {
                                    corsCfg->origins.push_back(origin);
                                    LOG_INFO << "[CORS] auto-added origin from api_base_url: " << origin;
                                }
                            }
                        }
                    }
                }
            }
            if (corsCfg->origins.empty()) corsCfg->origins.push_back("*");
            if (corsCfg->methods.empty())  corsCfg->methods  = "GET,POST,PUT,DELETE,OPTIONS";
            if (corsCfg->headers.empty())  corsCfg->headers  = "*";
            LOG_INFO << "[CORS] origins=" << corsCfg->origins[0]
                     << " credentials=" << corsCfg->credentials;

            // 解析 Origin → allowOrigin 的公共逻辑
            auto resolveOrigin = [corsCfg](const std::string& origin) -> std::string {
                if (origin.empty()) return "";
                bool wildcard = (corsCfg->origins.size() == 1 && corsCfg->origins[0] == "*");
                if (wildcard) return "*";
                for (auto& o : corsCfg->origins)
                    if (o == origin) return o;
                return "";
            };

            // Preflight (OPTIONS) → 直接返回 CORS 头
            drogon::app().registerPreRoutingAdvice(
                [corsCfg, resolveOrigin](const drogon::HttpRequestPtr &req,
                          drogon::AdviceCallback &&acb,
                          drogon::AdviceChainCallback &&accb) {
                    if (req->method() != drogon::Options) { accb(); return; }
                    std::string allowOrigin = resolveOrigin(req->getHeader("Origin"));
                    if (allowOrigin.empty()) { accb(); return; }
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->addHeader("Access-Control-Allow-Origin",  allowOrigin);
                    resp->addHeader("Access-Control-Allow-Methods", corsCfg->methods);
                    resp->addHeader("Access-Control-Allow-Headers", corsCfg->headers);
                    resp->addHeader("Access-Control-Max-Age",       "86400");
                    if (!corsCfg->expose.empty())
                        resp->addHeader("Access-Control-Expose-Headers", corsCfg->expose);
                    if (corsCfg->credentials && allowOrigin != "*")
                        resp->addHeader("Access-Control-Allow-Credentials", "true");
                    if (allowOrigin != "*")
                        resp->addHeader("Vary", "Origin");
                    resp->setStatusCode(drogon::k204NoContent);
                    acb(resp);
                });

            // 实际请求 → postHandling 追加 CORS 头
            drogon::app().registerPostHandlingAdvice(
                [corsCfg, resolveOrigin](const drogon::HttpRequestPtr &req,
                                         const drogon::HttpResponsePtr &resp) {
                    std::string allowOrigin = resolveOrigin(req->getHeader("Origin"));
                    if (allowOrigin.empty()) return;
                    resp->addHeader("Access-Control-Allow-Origin", allowOrigin);
                    if (!corsCfg->expose.empty())
                        resp->addHeader("Access-Control-Expose-Headers", corsCfg->expose);
                    if (corsCfg->credentials && allowOrigin != "*")
                        resp->addHeader("Access-Control-Allow-Credentials", "true");
                    if (allowOrigin != "*")
                        resp->addHeader("Vary", "Origin");
                });
        }

        // ── 前端托管（参考 wepay-cpp 设计；两种模式二选一）────────────────────
        //   模式 A: "frontend"           — 外部 ./web 目录（方便热更新）
        //   模式 B: "embedded_frontend"  — 编译进 exe（单文件分发，需 cmake -DRUOYI_EMBED_FRONTEND=ON）
        // 共享状态供后续错误处理器/限流器使用：
        //   feHosted=true  → 启用 SPA 404 回退 + 限流跳过静态资源
        //   feApiPrefix    → 用于区分 API 与静态资源
        //   feIndexPath    → 外部模式下的 index.html 绝对路径
        static bool        feHosted    = false;
        static bool        feSpaMode   = false;
        static bool        feEmbedded  = false;
        static std::string feApiPrefix;
        static std::string feIndexPath;
        try {
            Json::Value cfgRoot;
            {
                std::ifstream _fcf(configFile);
                if (_fcf.is_open()) {
                    Json::CharReaderBuilder _rb; std::string _err;
                    Json::parseFromStream(_rb, _fcf, &cfgRoot, &_err);
                }
            }
            bool extEnabled = cfgRoot.isMember("frontend") &&
                              cfgRoot["frontend"].get("enabled", false).asBool();
            bool embEnabled = cfgRoot.isMember("embedded_frontend") &&
                              cfgRoot["embedded_frontend"].get("enabled", false).asBool();

            if (extEnabled && embEnabled) {
                std::cerr << "\n[错误] frontend 与 embedded_frontend 不能同时启用，"
                             "请在 config.json 中只启用其中一个。\n" << std::endl;
                return 1;
            }

            // 模式 A: 外部目录托管
            if (extEnabled) {
                const auto& fc       = cfgRoot["frontend"];
                std::string distPath = fc.get("dist_path", "./web").asString();
                bool        spaMode  = fc.get("spa_mode", true).asBool();
                std::string apiPrefix= fc.get("api_prefix", "/prod-api").asString();
                int         cacheSec = fc.get("cache_seconds", 3600).asInt();

                if (std::filesystem::exists(distPath)
                    && std::filesystem::exists(distPath + "/index.html")) {

                    drogon::app().setDocumentRoot(distPath);
                    drogon::app().setStaticFilesCacheTime(cacheSec);
                    // 内置压缩支持（drogon 默认即开启，此处显式声明语义）：
                    //   enableGzip       — 实时压缩响应（非二进制 + >1KB）
                    //   setGzipStatic    — 客户端 Accept-Encoding: gzip 时，
                    //                       优先发同目录 .gz 预压缩文件（Vue dist
                    //                       的 compression-webpack-plugin 输出已带 .gz）
                    //   setBrStatic      — 同理优先发 .br Brotli 预压缩
                    drogon::app().enableGzip(true);
                    drogon::app().setGzipStatic(true);
                    drogon::app().setBrStatic(true);
                    LOG_INFO << "[Frontend] external dir: "
                             << std::filesystem::absolute(distPath).string()
                             << " | SPA=" << spaMode
                             << " | API=" << apiPrefix
                             << " | cache=" << cacheSec << "s";
                    std::cout << "[Frontend] 外部前端: "
                              << std::filesystem::absolute(distPath).string()
                              << "  api_prefix=" << apiPrefix << std::endl;

                    // API 路径剥离前缀（前端一般经 /prod-api/* 调用）
                    if (!apiPrefix.empty() && apiPrefix != "/") {
                        drogon::app().registerPreRoutingAdvice(
                            [apiPrefix](const drogon::HttpRequestPtr& req,
                                        drogon::AdviceCallback&&,
                                        drogon::AdviceChainCallback&& ccb) {
                                std::string p = req->path();
                                if (p.rfind(apiPrefix, 0) == 0) {
                                    std::string np = p.substr(apiPrefix.size());
                                    if (np.empty()) np = "/";
                                    req->setPath(np);
                                }
                                ccb();
                            });
                    }

                    // 注意：不在此处 setCustom404Page。统一由后面的 setCustomErrorHandler
                    // 智能判断：API 路径返 JSON 404；SPA 路径才回退到 index.html。
                    // 这样可以避免 API 调用 typo 路径时被误回 HTML 导致前端 axios JSON 解析失败。
                    feHosted    = true;
                    feSpaMode   = spaMode;
                    feApiPrefix = apiPrefix;
                    feIndexPath = std::filesystem::absolute(distPath + "/index.html").string();
                } else {
                    LOG_WARN << "[Frontend] dist_path 不存在或缺少 index.html: "
                             << distPath << "（已跳过托管）";
                    std::cout << "[Frontend] 警告: " << distPath
                              << " 不存在或缺少 index.html，已跳过托管" << std::endl;
                }
            }

            // 模式 B: 嵌入式（需在编译期 -DRUOYI_EMBED_FRONTEND=ON）
            if (embEnabled) {
#ifdef RUOYI_USE_EMBEDDED_FRONTEND
                const auto& ec = cfgRoot["embedded_frontend"];
                bool        spaMode  = ec.get("spa_mode", true).asBool();
                std::string apiPrefix= ec.get("api_prefix", "/prod-api").asString();
                EmbeddedFrontend::registerHandlers(apiPrefix, spaMode);
                feHosted    = true;
                feEmbedded  = true;
                feSpaMode   = spaMode;
                feApiPrefix = apiPrefix;
#else
                std::cerr << "\n[错误] embedded_frontend 已启用，但本次编译未嵌入前端！\n"
                          << "  请用 cmake -DRUOYI_EMBED_FRONTEND=ON -DRUOYI_EMBED_FRONTEND_DIR=./web 重新编译。\n"
                          << std::endl;
                return 1;
#endif
            }
        } catch (const std::exception& e) {
            LOG_WARN << "[Frontend] 加载配置失败: " << e.what();
        }

        // ── 可观测性：/actuator/* 端点 + HTTP 自动打点 advice ─────────────
        // /actuator/health  /actuator/info  /actuator/metrics  /actuator/db
        // POST /actuator/reload（仅 loopback）
        try {
            MetricsCollector::instance().registerActuator();
            MetricsCollector::instance().attachAdvice();
            // 把 DB 查询打点钩到 Metrics（DatabaseService 不直接依赖 MetricsCollector）
            DbMetricsHook::hook = [](long ms, bool ok, bool isWrite) {
                MetricsCollector::instance().onDbQuery(ms, ok, isWrite);
            };
            LOG_INFO << "[Metrics] /actuator/* endpoints registered, "
                        "HTTP duration histogram + DB hook attached";
            std::cout << "[Metrics] /actuator/metrics 已启用 (含 DB 慢查询计数)" << std::endl;
        } catch (const std::exception& e) {
            LOG_WARN << "[Metrics] 启用失败: " << e.what();
        }

        // ── HotConfig 文件监视器（5s 间隔检查 config.json mtime） ─────────
        try {
            HotConfig::instance().start(configFile, []{
                LOG_INFO << "[HotConfig] config.json reloaded";
                std::cout << "[HotConfig] config.json reloaded" << std::endl;
            });
        } catch (const std::exception& e) {
            LOG_WARN << "[HotConfig] 启动失败: " << e.what();
        }

        // ── nginx 风格功能集成（proxy_pass / allow-deny / limit_conn / access_log）──
        // 优先于通用限流：proxy 命中后直接转发上游，不会进入 drogon 路由
        try {
            ruoyi::nginx_like::registerAll(drogon::app().getCustomConfig());
        } catch (const std::exception& e) {
            LOG_WARN << "[NginxLike] 加载失败: " << e.what();
        }

        // ── IP 限流 (DDoS 防御) ─────────────────────────────────────────────────
        // 当合并部署托管前端时，跳过静态资源（带扩展名且非 API 前缀），
        // 避免单个用户加载几十个 JS/CSS 触发 200/min 限流误封。
        drogon::app().registerPreRoutingAdvice(
            [](const drogon::HttpRequestPtr &req,
               drogon::AdviceCallback &&acb,
               drogon::AdviceChainCallback &&accb) {
                if (feHosted) {
                    std::string p = req->path();
                    bool isApi = !feApiPrefix.empty() && feApiPrefix != "/" &&
                                 p.rfind(feApiPrefix, 0) == 0;
                    // 静态资源：路径含扩展名且非 API
                    auto slash = p.find_last_of('/');
                    auto seg   = (slash == std::string::npos) ? p : p.substr(slash + 1);
                    bool hasExt = seg.find('.') != std::string::npos;
                    if (!isApi && hasExt) { accb(); return; }
                }
                std::string ip = IpUtils::getIpAddr(req);
                if (!RateLimiter::instance().allow(ip)) {
                    MetricsCollector::instance().onRateLimited();
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode((drogon::HttpStatusCode)429);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody("{\"code\":429,\"msg\":\"请求过于频繁，请稍后重试\"}");
                    resp->addHeader("Retry-After", "300");
                    acb(resp);
                    return;
                }
                accb();
            });

        // ── Bot UA 拦截（绕过 nginx 直连后端时的第二道防线）─────────────────────
        drogon::app().registerPreRoutingAdvice(
            [](const drogon::HttpRequestPtr &req,
               drogon::AdviceCallback &&acb,
               drogon::AdviceChainCallback &&accb) {
                // OPTIONS 预检请求放行
                if (req->method() == drogon::Options) { accb(); return; }
                std::string ua = req->getHeader("User-Agent");
                if (isBotUserAgent(ua)) {
                    LOG_WARN << "[Security] Bot UA blocked (global): "
                             << ua.substr(0, 80) << " ip=" << IpUtils::getIpAddr(req)
                             << " path=" << req->path();
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k403Forbidden);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody("{\"code\":403,\"msg\":\"非法请求\"}");
                    acb(resp);
                    return;
                }
                accb();
            });

        // ── 自定义默认错误响应：404/405/500 等也走 AjaxResult JSON 格式 ──
        // 否则 drogon 默认返回 HTML，前端 axios 解析失败后报"未知错误"
        // 同时支持 SPA fallback：托管前端时，非 API 且无扩展名路径回退到 index.html
        drogon::app().setCustomErrorHandler(
            [](drogon::HttpStatusCode code,
               const drogon::HttpRequestPtr &req) -> drogon::HttpResponsePtr {
                // 404/405 SPA 回退：仅对前端路由路径生效，避免误伤 API typo
                // 405 也需要回退：drogon 对无扩展名路径可能因方法不匹配返回 405
                bool isSpaCode = (code == drogon::k404NotFound ||
                                  code == drogon::k405MethodNotAllowed);
                if (isSpaCode && feHosted && feSpaMode) {
                    std::string p = req->path();
                    bool isApi = !feApiPrefix.empty() && feApiPrefix != "/" &&
                                 p.rfind(feApiPrefix, 0) == 0;
                    auto slash = p.find_last_of('/');
                    auto seg   = (slash == std::string::npos) ? p : p.substr(slash + 1);
                    bool hasExt = seg.find('.') != std::string::npos;
                    // 非 API + 无扩展名 → 视作 vue-router 前端路径，回退 index.html
                    if (!isApi && !hasExt) {
                        if (!feEmbedded && !feIndexPath.empty()) {
                            return drogon::HttpResponse::newFileResponse(feIndexPath);
                        }
                        // 嵌入式：EmbeddedFrontend 的 advice 已处理，几乎不会到这里；
                        // 兜底返回简单 200 让前端继续加载（极少触发）
                    }
                }
                std::string msg;
                int bodyCode = (int)code;
                switch (code) {
                    case drogon::k400BadRequest:          msg = "请求参数错误";   break;
                    case drogon::k401Unauthorized:        msg = "认证失败";       break;
                    case drogon::k403Forbidden:           msg = "无权限访问";     break;
                    case drogon::k404NotFound:            msg = "资源不存在";     break;
                    case drogon::k405MethodNotAllowed:    msg = "方法不允许";     break;
                    case drogon::k413RequestEntityTooLarge: msg = "请求体过大"; break;
                    case drogon::k500InternalServerError: msg = "服务器内部错误"; break;
                    case drogon::k502BadGateway:          msg = "网关错误";       break;
                    case drogon::k503ServiceUnavailable:  msg = "服务不可用";     break;
                    default:                               msg = "请求失败";      break;
                }
                Json::Value j;
                j["code"] = bodyCode;
                j["msg"]  = msg;
                auto resp = drogon::HttpResponse::newHttpJsonResponse(j);
                resp->setStatusCode(code);
                return resp;
            });

        // ── 安全响应头（XSS/点击劫持/内容嗅探防御）────────────────────────────
        drogon::app().registerPostHandlingAdvice(
            [](const drogon::HttpRequestPtr& req,
               const drogon::HttpResponsePtr& resp) {
                resp->addHeader("X-Content-Type-Options",  "nosniff");
                resp->addHeader("X-XSS-Protection",        "1; mode=block");
                resp->addHeader("Referrer-Policy",         "strict-origin-when-cross-origin");
                // /ai/* 页面允许被跨域 iframe 嵌入（前端内嵌 AI 会话页）
                const std::string& p = req->path();
                bool isAiPage = (p == "/ai" || p == "/ai/" ||
                                 (p.size() > 4 && p.compare(0, 4, "/ai/") == 0));
                if (!isAiPage) {
                    resp->addHeader("X-Frame-Options", "SAMEORIGIN");
                    resp->addHeader("Content-Security-Policy",
                        "default-src 'self'; script-src 'self' 'unsafe-inline'; "
                        "style-src 'self' 'unsafe-inline'; img-src 'self' data:");
                }
            });

        // ── XSS 过滤（POST/PUT 请求 JSON body 净化）──────────────────────────
        drogon::app().registerPreHandlingAdvice(
            [](const drogon::HttpRequestPtr& req,
               drogon::AdviceCallback&&,
               drogon::AdviceChainCallback&& accb) {
                if (req->method() == drogon::Post || req->method() == drogon::Put) {
                    auto body = req->getJsonObject();
                    if (body) {
                        // SQL 注入特征告警（不阻断，已有参数化查询防御）
                        auto& bv = *body;
                        for (auto& key : bv.getMemberNames()) {
                            if (bv[key].isString()) {
                                const std::string& val = bv[key].asString();
                                if (XssUtils::hasSqlSignature(val)) {
                                    LOG_WARN << "[SQLi] suspicious input key=" << key
                                             << " ip=" << req->peerAddr().toIp()
                                             << " path=" << req->path();
                                }
                            }
                        }
                    }
                }
                accb();
            });

        // ── 接口验证（公开接口 + server-to-server）────────────────────────────
        // 规则：
        //   /challenge /forgotPassword /resetPassword → 完全放行（自带鉴权机制）
        //   公开接口(/login /captchaImage /register /forgotPassword)：
        //     浏览器客户端 → 必须携带 X-Challenge-Token（后端颁发，60s 一次性）
        //     server-to-server → 携带 X-App-Id + X-Sign（HMAC-SHA256）
        //   其他接口携带 X-App-Id → server-to-server 验签
        //   其他接口不带任何签名头 → 走 JWT 中间件
        drogon::app().registerPreHandlingAdvice(
            [](const drogon::HttpRequestPtr& req,
               drogon::AdviceCallback&& acb,
               drogon::AdviceChainCallback&& accb) {
                auto& sv = SignUtils::instance();
                const std::string& path = req->path();

                // ── WebSocket 路径早期 token 预检 ──────────────────────
                // drogon 框架在 router 匹配后才回调 handleNewConnection，
                // 此时 HTTP 已完成 WS 升级。若到 handleNewConnection 才发现
                // 缺 token 再 shutdown，攻击者可借机消耗 socket/内存资源。
                // 这里在 PreHandling 阶段先拒掉缺 token 的 WS 请求。
                // /ws/ticket 是普通 HTTP 接口（签发 WS 票据），走正常 JWT 流程
                // 只对真正的 WebSocket 升级路径（/ws/notify 等）做 token 参数预检
                if (path.size() >= 4 && path.compare(0, 4, "/ws/") == 0
                    && path != "/ws/ticket") {
                    if (req->getParameter("token").empty()
                        && req->getParameter("ticket").empty()) {
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k401Unauthorized);
                        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                        resp->setBody(R"({"code":401,"msg":"缺少 token/ticket 参数"})");
                        acb(resp); return;
                    }
                    // 有 token/ticket 时继续走 WebSocketController 流程
                    accb(); return;
                }

                // 引导接口、重置接口、健康检查、版本接口完全放行
                if (path == "/challenge" || path == "/resetPassword" || path == "/forgotPassword"
                    || path == "/health" || path == "/version" || path == "/ssl-config") {
                    accb(); return;
                }
                // AI 内置助手页 + AI 健康检查 + AI 聊天后端：放行
                // /ai/page 是 InnerLink iframe 嵌入的内置 HTML 页面
                // /ai/chat 由该页面 fetch 调用，已通过 fallback 集成讯飞星火外部 API
                if (path == "/ai/page" || path == "/ai/chat"
                    || path == "/ai/health" || path == "/ai/generate") {
                    accb(); return;
                }

                std::string appId          = req->getHeader("X-App-Id");
                std::string challengeToken = req->getHeader("X-Challenge-Token");
                bool isPublicRoute = (path == "/login" || path == "/captchaImage"
                                   || path == "/register" || path == "/sendRegCode");
                bool hasSignHeader = !appId.empty();
                bool hasChallengeToken = !challengeToken.empty();

                // 公开接口验证
                if (isPublicRoute) {
                    // 方式1：浏览器挑战令牌
                    if (hasChallengeToken) {
                        auto cacheKey = "challenge:" + challengeToken;
                        auto cached   = MemCache::instance().getString(cacheKey);
                        if (!cached) {
                            auto resp = drogon::HttpResponse::newHttpResponse();
                            resp->setStatusCode(drogon::k403Forbidden);
                            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                            resp->setBody("{\"code\":403,\"msg\":\"挑战令牌无效或已过期\"}");
                            acb(resp); return;
                        }
                        MemCache::instance().remove(cacheKey); // 一次性使用
                        accb(); return;
                    }
                    // 方式2：server-to-server HMAC 签名
                    if (sv.hasApps() && hasSignHeader) {
                        std::string errMsg;
                        if (!sv.verify(req, errMsg)) {
                            LOG_WARN << "[Sign] 验签失败: " << errMsg << " path=" << path
                                     << " ip=" << IpUtils::getIpAddr(req);
                            auto resp = drogon::HttpResponse::newHttpResponse();
                            resp->setStatusCode(drogon::k403Forbidden);
                            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                            resp->setBody("{\"code\":403,\"msg\":\"" + errMsg + "\"}");
                            acb(resp); return;
                        }
                        accb(); return;
                    }
                    // 没有配置任何 server-to-server 应用时，公开接口直接放行
                    if (!sv.hasApps()) { accb(); return; }
                    // 两种方式都没有 → 拒绝
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k403Forbidden);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody("{\"code\":403,\"msg\":\"缺少访问凭证 (X-Challenge-Token 或 X-App-Id)\"}");
                    acb(resp); return;
                }

                // 非公开接口：带 X-App-Id → server-to-server 验签，否则放行走 JWT
                if (!hasSignHeader || !sv.hasApps()) { accb(); return; }
                std::string errMsg;
                if (!sv.verify(req, errMsg)) {
                    LOG_WARN << "[Sign] 验签失败: " << errMsg << " path=" << path
                             << " ip=" << IpUtils::getIpAddr(req);
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k403Forbidden);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody("{\"code\":403,\"msg\":\"" + errMsg + "\"}");
                    acb(resp); return;
                }
                accb();
            });

        // ── 定期清理限流器过期记录（每2分钟）────────────────────────────────
        drogon::app().getLoop()->runEvery(120.0, []{
            RateLimiter::instance().cleanup();
        });

        // 静态文件服务：/profile/{dir}/{file} → uploads/{dir}/{file}
        // 用于头像(/profile/avatar/xxx)、通用上传(/profile/upload/xxx)等
        auto serveUpload = [](const drogon::HttpRequestPtr &,
                              std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                              const std::string &dir, const std::string &file) {
            std::string filePath = "uploads/" + dir + "/" + file;
            if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath)) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k404NotFound);
                cb(resp);
                return;
            }
            cb(drogon::HttpResponse::newFileResponse(filePath));
        };
        drogon::app().registerHandler("/profile/{dir}/{file}", serveUpload, {drogon::Get});


        // ── iconfont 字体文件路由 ──────────────────────────────────────
        drogon::app().registerHandler("/iconfont-sys.woff2",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                static std::string fontData;
                static std::once_flag once;
                std::call_once(once, []() {
                    std::ifstream f("iconfont-sys.woff2", std::ios::binary);
                    if (f) fontData = std::string(std::istreambuf_iterator<char>(f), {});
                });
                auto resp = drogon::HttpResponse::newHttpResponse();
                if (fontData.empty()) {
                    resp->setStatusCode(drogon::k404NotFound);
                } else {
                    resp->setContentTypeString("font/woff2");
                    resp->addHeader("Cache-Control", "public,max-age=86400");
                    resp->setBody(fontData);
                }
                cb(resp);
            }, {drogon::Get});

        // ── /health 健康检查（nginx upstream_check / k8s liveness probe）──────
        drogon::app().registerHandler("/health",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                auto& db = DatabaseService::instance();
                bool dbOk = db.isConnected() || db.isUsingSqlite();
                Json::Value j;
                j["status"] = dbOk ? "UP" : "DEGRADED";
                j["db"]     = db.backendInfo();
                j["cache"]  = MemCache::backendInfo();
                auto resp = drogon::HttpResponse::newHttpJsonResponse(j);
                resp->setStatusCode(dbOk ? drogon::k200OK : drogon::k503ServiceUnavailable);
                cb(resp);
            }, {drogon::Get});

        // ── /version 版本信息 ──────────────────────────────────────────────
        drogon::app().registerHandler("/version",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                Json::Value j;
                j["app"]     = "ruoyi-cpp";
                j["version"] = "1.0.0";
                cb(drogon::HttpResponse::newHttpJsonResponse(j));
            }, {drogon::Get});

        // ── 随机视频开关（无需登录，前端用于决定是否显示菜单）──────────────
        // GET /api/video/enabled → {"enabled":true}
        drogon::app().registerHandler("/api/video/enabled",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                auto& db = DatabaseService::instance();
                auto res = db.queryParams(
                    "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1",
                    {"sys.video.enabled"});
                bool enabled = true;
                if (res.ok() && res.rows() > 0) {
                    std::string val = res.str(0, 0);
                    enabled = !(val == "false" || val == "0");
                }
                Json::Value j;
                j["enabled"] = enabled;
                auto r = drogon::HttpResponse::newHttpJsonResponse(j);
                r->addHeader("Access-Control-Allow-Origin", "*");
                cb(r);
            }, {drogon::Get});

        // ── 随机视频接口 ───────────────────────────────────────────────────
        // GET /api/video/random  → {"url":"https://...mp4"}
        drogon::app().registerHandler("/api/video/random",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                // 用 drogon HttpClient 跟随外部 API 的 302 跳转，取最终 mp4 URL
                auto client = drogon::HttpClient::newHttpClient("http://api.yujn.cn");
                auto extReq = drogon::HttpRequest::newHttpRequest();
                extReq->setPath("/api/zzxjj.php");
                extReq->setParameter("type", "video");
                extReq->setMethod(drogon::Get);
                client->sendRequest(extReq,
                    [cb](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
                        Json::Value j;
                        if (result == drogon::ReqResult::Ok) {
                            // 302 Location 就是 mp4 直链
                            std::string url = resp->getHeader("location");
                            if (url.empty()) url = std::string(resp->body());
                            j["url"] = url;
                            j["ok"]  = true;
                        } else {
                            j["ok"]  = false;
                            j["url"] = "";
                        }
                        auto r = drogon::HttpResponse::newHttpJsonResponse(j);
                        r->addHeader("Access-Control-Allow-Origin", "*");
                        cb(r);
                    });
            }, {drogon::Get});

        // GET /api/video/player  → 内嵌 HTML 播放器页面
        drogon::app().registerHandler("/api/video/player",
            [](const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                static const std::string html = R"html(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>随机视频</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d0d0d;display:flex;flex-direction:column;align-items:center;
     justify-content:center;min-height:100vh;font-family:sans-serif;color:#fff}
h2{margin-bottom:16px;font-size:1.2rem;opacity:.7;letter-spacing:2px}
.wrap{position:relative;width:min(420px,95vw);background:#1a1a1a;border-radius:16px;
      overflow:hidden;box-shadow:0 8px 40px #0008}
video{width:100%;display:block;max-height:75vh;background:#000;object-fit:contain}
.ctrl{display:flex;gap:12px;padding:12px;background:#111}
button{flex:1;padding:10px;border:none;border-radius:8px;cursor:pointer;
       font-size:.95rem;font-weight:600;transition:.2s}
#btnNext{background:#e94560;color:#fff}
#btnNext:hover{background:#c73652}
#btnDl{background:#2a2a2a;color:#aaa}
#btnDl:hover{background:#3a3a3a;color:#fff}
#status{font-size:.75rem;opacity:.5;padding:4px 12px 8px;text-align:center}
</style>
</head>
<body>
<h2>随机视频</h2>
<div class="wrap">
  <video id="v" autoplay playsinline muted loop></video>
  <div class="ctrl">
    <button id="btnNext" onclick="next()">▶ 下一个</button>
    <button id="btnDl" onclick="dl()">⬇ 下载</button>
  </div>
  <div id="status">加载中...</div>
</div>
<script>
let cur='';
async function next(){
  document.getElementById('status').textContent='加载中...';
  try{
    const r=await fetch('/api/video/random');
    const d=await r.json();
    if(d.ok&&d.url){
      cur=d.url;
      const v=document.getElementById('v');
      v.src=cur;
      v.load();
      v.play().catch(()=>{});
      document.getElementById('status').textContent='';
    }else{
      document.getElementById('status').textContent='获取失败，请重试';
    }
  }catch(e){
    document.getElementById('status').textContent='网络错误: '+e.message;
  }
}
function dl(){
  if(!cur)return;
  const a=document.createElement('a');
  a.href=cur;a.download='video.mp4';a.target='_blank';a.click();
}
next();
</script>
</body>
</html>)html";
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setContentTypeCode(drogon::CT_TEXT_HTML);
                resp->setBody(html);
                cb(resp);
            }, {drogon::Get});

        // ── SSL/HTTPS 配置管理页（无需前端，浏览器直接访问）─────────────────
        drogon::app().registerHandler("/ssl-config",
            [](const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                // 鉴权：Authorization header / Cookie Admin-Token / query param / localhost
                auto token = SecurityUtils::getToken(req);
                if (token.empty()) {
                    // 标准 RuoYi Vue2 前端将 JWT 存在 Cookie 'Admin-Token'
                    const std::string& cookieHdr = req->getHeader("cookie");
                    const std::string key = "Admin-Token=";
                    auto pos = cookieHdr.find(key);
                    if (pos != std::string::npos) {
                        pos += key.size();
                        auto end = cookieHdr.find(';', pos);
                        token = cookieHdr.substr(pos, end == std::string::npos ? end : end - pos);
                    }
                }
                if (token.empty()) token = req->getParameter("token");
                bool ok = false;
                if (!token.empty()) {
                    try {
                        auto uuid    = JwtUtils::parseUuid(token);
                        auto userKey = SecurityUtils::getTokenKey(uuid);
                        ok = (bool)TokenCache::instance().get(userKey);
                    } catch (...) {}
                }
                if (!ok) {
                    const auto& peer = req->getPeerAddr().toIp();
                    ok = (peer == "127.0.0.1" || peer == "::1" || peer == "0.0.0.0");
                }
                if (!ok) {
                    auto r = drogon::HttpResponse::newHttpResponse();
                    r->setStatusCode(drogon::k401Unauthorized);
                    r->setContentTypeCode(drogon::CT_TEXT_HTML);
                    r->setBody("<html><body style='font-family:sans-serif;text-align:center;padding:60px'>"
                               "<h2>&#128274; 请先登录后携带 token 访问</h2>"
                               "<p>示例：/ssl-config?token=eyJhbG...</p></body></html>");
                    cb(r); return;
                }
                std::string tok = token;
                std::string html = R"html(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HTTPS / SSL 配置</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#0f1117;color:#e2e8f0;padding:16px}
h1{font-size:1.4rem;margin-bottom:20px;color:#7dd3fc;display:flex;align-items:center;gap:8px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:16px;margin-bottom:20px}
.card{background:#1e2330;border-radius:12px;padding:20px;border:1px solid #2d3748}
.card h2{font-size:.95rem;color:#94a3b8;margin-bottom:14px;text-transform:uppercase;letter-spacing:.05em}
.badge{display:inline-block;padding:2px 10px;border-radius:20px;font-size:.8rem;font-weight:600}
.ok{background:#065f46;color:#6ee7b7}.fail{background:#7f1d1d;color:#fca5a5}
label{display:block;font-size:.85rem;color:#94a3b8;margin-bottom:4px;margin-top:10px}
input[type=text],input[type=number]{width:100%;padding:8px 10px;background:#0f1117;border:1px solid #374151;
  border-radius:6px;color:#e2e8f0;font-size:.9rem}
input[type=file]{width:100%;padding:6px;background:#0f1117;border:1px solid #374151;
  border-radius:6px;color:#94a3b8;font-size:.85rem}
.toggle{display:flex;align-items:center;gap:10px;margin-top:10px}
.toggle input{width:40px;height:22px;accent-color:#3b82f6;cursor:pointer}
btn,button{display:inline-block;padding:9px 20px;border-radius:8px;border:none;cursor:pointer;font-size:.88rem;font-weight:600;transition:.2s}
.btn-primary{background:#3b82f6;color:#fff}.btn-primary:hover{background:#2563eb}
.btn-success{background:#059669;color:#fff}.btn-success:hover{background:#047857}
.btn-warn{background:#d97706;color:#fff}.btn-warn:hover{background:#b45309}
.actions{display:flex;gap:10px;margin-top:16px;flex-wrap:wrap}
#msg{margin-top:14px;padding:10px 14px;border-radius:8px;display:none;font-size:.88rem}
.msg-ok{background:#065f46;color:#6ee7b7;display:block!important}
.msg-err{background:#7f1d1d;color:#fca5a5;display:block!important}
.hint{font-size:.78rem;color:#64748b;margin-top:6px}
pre{background:#0f1117;padding:10px;border-radius:6px;font-size:.75rem;color:#6ee7b7;overflow-x:auto;margin-top:6px;max-height:80px}
</style>
</head>
<body>
<h1>&#128274; HTTPS / SSL 证书管理</h1>
<div class="grid" id="statusGrid">
  <div class="card"><h2>当前状态</h2><div id="statusHtml">加载中...</div></div>
  <div class="card"><h2>证书预览</h2><pre id="certPreview">-</pre></div>
</div>
<div class="grid">
  <div class="card">
    <h2>上传证书 (.pem / .crt / .cer)</h2>
    <input type="file" id="certFile" accept=".pem,.crt,.cer">
    <div class="actions"><button class="btn-primary" onclick="uploadCert()">上传证书</button></div>
  </div>
  <div class="card">
    <h2>上传私钥 (.pem / .key)</h2>
    <input type="file" id="keyFile" accept=".pem,.key">
    <div class="actions"><button class="btn-primary" onclick="uploadKey()">上传私钥</button></div>
  </div>
</div>
<div class="card" style="max-width:560px">
  <h2>配置</h2>
  <label>HTTP 端口</label>
  <input type="number" id="httpPort" value="18080" min="1" max="65535">
  <label>HTTPS 端口</label>
  <input type="number" id="httpsPort" value="18443" min="1" max="65535">
  <div class="toggle">
    <label style="margin:0">启用 HTTPS</label>
    <input type="checkbox" id="enabled">
  </div>
  <div class="toggle">
    <label style="margin:0">强制 HTTP → HTTPS 跳转</label>
    <input type="checkbox" id="forceHttps">
  </div>
  <div id="msg"></div>
  <div class="actions">
    <button class="btn-success" onclick="saveConfig()">保存配置</button>
  </div>
  <p class="hint">&#9888;&#65039; 配置保存后需<b>重启后端服务</b>方可生效</p>
</div>
<script>
function getCookie(n){const m=document.cookie.match(new RegExp('(?:^|; )'+n+'=([^;]*)'));return m?decodeURIComponent(m[1]):'';}
const TOKEN = getCookie('Admin-Token') || new URLSearchParams(location.search).get('token') || '';
const H = {'Authorization':'Bearer '+TOKEN,'Content-Type':'application/json'};
async function api(url,method,body){
  const r=await fetch(url+'?token='+TOKEN,{method,headers:method==='GET'?{}:H,body:body?JSON.stringify(body):undefined});
  return r.json();
}
async function load(){
  const d=await api('/system/ssl/config','GET');
  if(d.code!==200){document.getElementById('statusHtml').innerHTML='<span class="badge fail">查询失败</span>';return;}
  const c=d.data;
  document.getElementById('httpPort').value=c.httpPort||18080;
  document.getElementById('httpsPort').value=c.httpsPort||18443;
  document.getElementById('enabled').checked=c.enabled;
  document.getElementById('forceHttps').checked=c.forceHttps;
  document.getElementById('certPreview').textContent=c.certPreview||'（未上传）';
  document.getElementById('statusHtml').innerHTML=`
    <div style="display:flex;flex-wrap:wrap;gap:8px;margin-bottom:6px">
      <span class="badge ${c.enabled?'ok':'fail'}">${c.enabled?'HTTPS 已启用':'HTTPS 未启用'}</span>
      <span class="badge ${c.certExists?'ok':'fail'}">${c.certExists?'证书已上传':'证书未上传'}</span>
      <span class="badge ${c.certInDb?'ok':'fail'}">${c.certInDb?'DB: cert ✓':'DB: cert 无'}</span>
      <span class="badge ${c.keyInDb?'ok':'fail'}">${c.keyInDb?'DB: key ✓':'DB: key 无'}</span>
    </div>
    <div class="hint">HTTP:${c.httpPort} / HTTPS:${c.httpsPort}${c.forceHttps?' | 强制跳转':''}</div>`;
}
function showMsg(txt,ok){
  const el=document.getElementById('msg');
  el.textContent=txt;el.className=ok?'msg-ok':'msg-err';
  setTimeout(()=>el.className='',4000);
}
async function uploadCert(){
  const f=document.getElementById('certFile').files[0];
  if(!f){showMsg('请选择证书文件',false);return;}
  const fd=new FormData();fd.append('file',f);
  const r=await fetch('/system/ssl/uploadCert?token='+TOKEN,{method:'POST',body:fd});
  const d=await r.json();
  showMsg(d.msg,d.code===200);if(d.code===200)load();
}
async function uploadKey(){
  const f=document.getElementById('keyFile').files[0];
  if(!f){showMsg('请选择私钥文件',false);return;}
  const fd=new FormData();fd.append('file',f);
  const r=await fetch('/system/ssl/uploadKey?token='+TOKEN,{method:'POST',body:fd});
  const d=await r.json();
  showMsg(d.msg,d.code===200);if(d.code===200)load();
}
async function saveConfig(){
  const body={
    enabled:document.getElementById('enabled').checked,
    httpsPort:parseInt(document.getElementById('httpsPort').value),
    httpPort:parseInt(document.getElementById('httpPort').value),
    forceHttps:document.getElementById('forceHttps').checked
  };
  const d=await api('/system/ssl/config','PUT',body);
  showMsg(d.msg,d.code===200);
}
load();
</script>
</body></html>)html";
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setContentTypeCode(drogon::CT_TEXT_HTML);
                resp->setBody(html);
                cb(resp);
            }, {drogon::Get});

        // 数据库就绪后初始化
        drogon::app().registerBeginningAdvice([configFile, cfgLoader, isPrimary]() {
            std::cout << "[Cache] backend: " << MemCache::backendInfo() << std::endl;
            // 从 config.json 读取配置
            Json::Value dbCfg;
            int    listenPort    = 18080;
            std::string listenAddr = "0.0.0.0";
            SqliteCipher::KeyConfig sqliteCipherCfg;
            std::string simpleEncryptKey;   // 顶级 sqlite.encrypt_key 简化配置（wepay 风格）
            {
                std::ifstream cfgFile(configFile);
                if (cfgFile.is_open()) {
                    Json::Value root;
                    Json::CharReaderBuilder rb;
                    std::string errs;
                    if (Json::parseFromStream(rb, cfgFile, &root, &errs)) {
                        dbCfg = root["database"];
                        if (root.isMember("listeners") && root["listeners"].isArray()
                            && root["listeners"].size() > 0) {
                            auto &l = root["listeners"][0];
                            listenPort = l.get("port", 18080).asInt();
                            listenAddr = l.get("address", "0.0.0.0").asString();
                        }
                        // SQLite 加密配置（默认 enabled=false）
                        sqliteCipherCfg = SqliteCipher::loadConfig(root);
                        // 简化配置（wepay 风格）：顶级 sqlite.encrypt_key 非空即启用
                        simpleEncryptKey = root["sqlite"].get("encrypt_key", "").asString();
                    }
                }
            }
            if (!dbCfg.isNull()) {
                std::string connStr = buildDbConnStr(configFile, 5);
                LOG_INFO << "正在连接数据库: " << dbCfg.get("host","127.0.0.1").asString()
                         << ":" << dbCfg.get("port",5432).asInt()
                         << "/" << dbCfg.get("dbname","ruoyi").asString();
                (void)connStr;
                bool pgOk = DatabaseService::instance().connect(buildDbConnStr(*cfgLoader, 5));
                // 慢查询阈值（默认 200ms WARN, 1000ms ERROR；可在 config.database.slow_query_warn_ms / err_ms 调整）
                {
                    int warnMs = dbCfg.get("slow_query_warn_ms", 200).asInt();
                    int errMs  = dbCfg.get("slow_query_err_ms",  1000).asInt();
                    DatabaseService::instance().setSlowQueryThreshold(warnMs, errMs);
                }
                // 始终打开 SQLite（PG 可用时用于双写，PG 不可用时用作主库）
                // 优先用 config 里的 sqlite_path，否则用本地 Temp 目录（避免网络盘 disk I/O error）
                std::string sqlitePath = dbCfg.get("sqlite_path", "").asString();
                if (sqlitePath.empty()) {
#ifdef _WIN32
                    char localApp[MAX_PATH] = {};
                    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localApp) == S_OK) {
                        std::string dir = std::string(localApp) + "\\ruoyi-cpp";
                        CreateDirectoryA(dir.c_str(), nullptr); // 不存在时创建，已存在时忽略
                        sqlitePath = dir + "\\ruoyi-cpp.db";
                    } else {
                        sqlitePath = "./ruoyi-cpp.db";
                    }
#else
                    sqlitePath = "/tmp/ruoyi-cpp.db";
#endif
                }
                LOG_INFO << "[SQLite] path=" << sqlitePath;
                // ── 简化加密配置（wepay 风格）：sqlite.encrypt_key 非空 → 直接当 passphrase
                //    复杂派生 security.sqlite.encryption.* 优先级更高，未启用时才用简化
                if (!simpleEncryptKey.empty() && !sqliteCipherCfg.enabled) {
                    SqliteCipher::KeyConfig simpleCfg;
                    simpleCfg.enabled = true;
                    simpleCfg.source  = "config-simple";
                    DatabaseService::instance().setCipherKey(simpleEncryptKey, simpleCfg);
                    LOG_INFO << "[SQLite] 加密已启用（简化配置 sqlite.encrypt_key, "
                             << simpleEncryptKey.size() << " 字节）";
                }
                // 如启用加密，先派生密钥并设置到 DatabaseService
                if (sqliteCipherCfg.enabled) {
                    std::string kerr;
                    std::string key = SqliteCipher::deriveKey(sqliteCipherCfg, &kerr);
                    // kerr 在成功时也可能携带 "[WARN] ..." 降级提示
                    if (!kerr.empty() && !key.empty()) {
                        LOG_WARN << "[SQLite] " << kerr;
                    }
                    if (key.empty()) {
                        LOG_ERROR << "[SQLite] 加密密钥派生失败（source="
                                  << sqliteCipherCfg.source << "）: " << kerr;
                        std::cerr << "[致命错误] SQLite 加密已启用但密钥派生失败: " << kerr << std::endl;
                        std::exit(1);
                    }
                    DatabaseService::instance().setCipherKey(key, sqliteCipherCfg);
                    LOG_INFO << "[SQLite] 加密已启用（source=" << sqliteCipherCfg.source
                             << " pageSize=" << sqliteCipherCfg.cipherPageSize
                             << " kdfIter=" << sqliteCipherCfg.cipherKdfIter << "）";
                }
                DatabaseService::instance().connectSqlite(sqlitePath);
                if (!pgOk) {
                    LOG_ERROR << "数据库连接失败，已切换到 SQLite 回退!";
                    DatabaseService::instance().activateSqliteFallback();
                } else {
                    LOG_INFO << "已连接 PostgreSQL，SQLite 双写已就绪";
                }
            } else {
                LOG_ERROR << "config.json 中未找到 database 配置段";
            }

            LOG_INFO << "正在初始化数据库表...";
            DatabaseInit::run();

            // ── 启动时清菜单缓存：DatabaseInit 可能新增/修改了 sys_menu，
            // ── 而 /getRouters 有 30min MemCache（含 Redis），不清的话新菜单不显示
            try {
                MemCache::instance().removeByPrefix("routers:");
                LOG_INFO << "[Menu] routers cache cleared after DatabaseInit";
            } catch (const std::exception& e) {
                LOG_WARN << "[Menu] clear routers cache failed: " << e.what();
            }

            // ── 启动时统一校正所有 InnerLink 菜单 URL ─────────────────────────
            // 优先级：menu.api_base_url（显式，生产环境推荐）
            //       > frontend/embedded_frontend.enabled（合并部署）
            //       > 直连后端（无 apiPrefix）
            // menu.logfile_external_url 仍作为 menu_id=120 的单独兜底覆盖。
            try {
                std::ifstream mf(configFile);
                Json::Value mroot;
                if (mf.is_open()) {
                    Json::CharReaderBuilder rb; std::string err;
                    Json::parseFromStream(rb, mf, &mroot, &err);
                }

                // ── 判断 host 是否为内网地址（反向代理场景下不可从浏览器访问）────
                auto isPrivateHost = [](const std::string& h) -> bool {
                    if (h == "localhost" || h == "127.0.0.1" || h == "0.0.0.0"
                        || h == "::" || h == "::1") return true;
                    // 10.x / 172.16-31.x / 192.168.x
                    if (h.substr(0, 3) == "10.") return true;
                    if (h.substr(0, 8) == "192.168.") return true;
                    if (h.size() > 4 && h.substr(0, 4) == "172.") {
                        int seg = 0;
                        try { seg = std::stoi(h.substr(4, h.find('.', 4) - 4)); } catch (...) {}
                        if (seg >= 16 && seg <= 31) return true;
                    }
                    return false;
                };

                // ── 计算通用 baseUrl ──────────────────────────────────────────
                // baseUrl 为空表示"推断结果不可信，跳过更新"
                std::string baseUrl;
                bool explicitUrl = false;

                // 读 api_prefix（来自 frontend / embedded_frontend）
                auto readApiPrefix = [&]() -> std::string {
                    bool extEn = mroot.isMember("frontend")
                                 && mroot["frontend"].get("enabled", false).asBool();
                    bool embEn = mroot.isMember("embedded_frontend")
                                 && mroot["embedded_frontend"].get("enabled", false).asBool();
                    std::string ap;
                    if (extEn)      ap = mroot["frontend"].get("api_prefix", "/prod-api").asString();
                    else if (embEn) ap = mroot["embedded_frontend"].get("api_prefix", "/prod-api").asString();
                    if (ap == "/") ap.clear();
                    return ap;
                };

                if (mroot.isMember("menu")
                    && mroot["menu"].isMember("api_base_url")
                    && !mroot["menu"]["api_base_url"].asString().empty()) {
                    // ① 显式指定，去掉末尾 /，优先级最高
                    baseUrl = mroot["menu"]["api_base_url"].asString();
                    while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
                    explicitUrl = true;
                } else if (mroot.isMember("domain")
                           && !mroot["domain"].asString().empty()) {
                    // ② domain 字段：自动拼 scheme://domain[:port][/api_prefix]
                    std::string d = mroot["domain"].asString();
                    // nginx_embedded 启用时走 443 HTTPS（标准端口不加端口号）
                    bool nginxEnabled = mroot.isMember("nginx_embedded")
                                        && mroot["nginx_embedded"].get("enabled", false).asBool();
                    std::string scheme = nginxEnabled ? "https" : "http";
                    int port = -1; // -1=标准端口，不加
                    if (!nginxEnabled && mroot.isMember("listeners")
                        && mroot["listeners"].isArray()
                        && mroot["listeners"].size() > 0) {
                        auto& L = mroot["listeners"][0];
                        int p = L.get("port", 18080).asInt();
                        if (L.get("ssl", false).asBool()) scheme = "https";
                        bool isStd = (scheme=="http" && p==80)||(scheme=="https" && p==443);
                        if (!isStd) port = p;
                    }
                    std::string portStr = (port > 0) ? ":" + std::to_string(port) : "";
                    baseUrl = scheme + "://" + d + portStr + readApiPrefix();
                    explicitUrl = true;
                    LOG_INFO << "[Menu] baseUrl from domain field: " << baseUrl;
                    std::cout << "[Menu] domain=" << d << " -> baseUrl=" << baseUrl << std::endl;
                } else {
                    // 自动推断：从 listeners + frontend/embedded_frontend
                    std::string host   = "localhost";
                    int         port   = 18080;
                    std::string scheme = "http";
                    if (mroot.isMember("listeners") && mroot["listeners"].isArray()
                        && mroot["listeners"].size() > 0) {
                        auto& L = mroot["listeners"][0];
                        std::string a = L.get("address", "0.0.0.0").asString();
                        if (!a.empty() && a != "0.0.0.0" && a != "::") host = a;
                        port = L.get("port", 18080).asInt();
                        if (L.get("ssl", false).asBool()) scheme = "https";
                    }
                    bool extEn = mroot.isMember("frontend")
                                 && mroot["frontend"].get("enabled", false).asBool();
                    bool embEn = mroot.isMember("embedded_frontend")
                                 && mroot["embedded_frontend"].get("enabled", false).asBool();
                    std::string apiPrefix;
                    if (extEn)
                        apiPrefix = mroot["frontend"].get("api_prefix", "/prod-api").asString();
                    else if (embEn)
                        apiPrefix = mroot["embedded_frontend"].get("api_prefix", "/prod-api").asString();
                    if (apiPrefix == "/") apiPrefix.clear();

                    if (isPrivateHost(host)) {
                        // 内网地址 → 反向代理场景下浏览器无法访问，跳过更新
                        LOG_WARN << "[Menu] InnerLink 菜单 URL 未更新：自动推断地址 "
                                 << host << " 为内网地址，反向代理场景下浏览器无法访问。\n"
                                 << "        请在 config.json 设置 menu.api_base_url，例如：\n"
                                 << "        \"api_base_url\": \"https://your-domain.com/prod-api\"";
                        std::cerr << "[Menu] WARNING: api_base_url 未配置，菜单 URL 保持数据库现有值。\n"
                                  << "  -> 生产/反向代理环境请设置 config.json: menu.api_base_url\n";
                        // baseUrl 保持空字符串，下方 for 循环将跳过写库
                    } else {
                        baseUrl = scheme + "://" + host + ":" + std::to_string(port) + apiPrefix;
                    }
                }

                // ── 全量 InnerLink 菜单 ID → 路径后缀 ────────────────────────
                // 带 api_prefix 的菜单（通过前端代理访问的 API 页面）
                static const std::pair<int, const char*> kMenuSuffixes[] = {
                    {120,  "/monitor/logfile/page"},
                    {130,  "/monitor/restart/page"},
                    {131,  "/system/apikey/page"},
                    {132,  "/system/notify/channel/page"},
                    {133,  "/api/license/page"},
                };
                // 直接后端路由（不经过 api_prefix，只用 scheme://host:port）
                static const std::pair<int, const char*> kDirectSuffixes[] = {
                    {1100, "/ssl-config"},
                    {2100, "/ai/page"},
                };

                // baseUrlDirect：去掉 api_prefix，用于直接后端路由
                std::string baseUrlDirect;
                if (!baseUrl.empty()) {
                    if (explicitUrl) {
                        // 显式指定了 api_base_url，去掉末尾的 apiPrefix 部分
                        std::string ap;
                        if (mroot.isMember("frontend")
                            && mroot["frontend"].get("enabled", false).asBool())
                            ap = mroot["frontend"].get("api_prefix", "/prod-api").asString();
                        else if (mroot.isMember("embedded_frontend")
                                 && mroot["embedded_frontend"].get("enabled", false).asBool())
                            ap = mroot["embedded_frontend"].get("api_prefix", "/prod-api").asString();
                        if (!ap.empty() && ap != "/" && baseUrl.size() > ap.size()
                            && baseUrl.compare(baseUrl.size() - ap.size(), ap.size(), ap) == 0)
                            baseUrlDirect = baseUrl.substr(0, baseUrl.size() - ap.size());
                        else
                            baseUrlDirect = baseUrl; // 无前缀或无法剥离，直接用
                    } else {
                        // 自动推断：baseUrl 已含 apiPrefix，取 scheme://host:port 部分
                        auto pos = baseUrl.find("://");
                        if (pos != std::string::npos) {
                            auto slash = baseUrl.find('/', pos + 3);
                            baseUrlDirect = (slash != std::string::npos)
                                ? baseUrl.substr(0, slash) : baseUrl;
                        } else {
                            baseUrlDirect = baseUrl;
                        }
                    }
                }

                auto updateMenu = [&](int mid, const std::string& url) {
                    DatabaseService::instance().execParams(
                        "UPDATE sys_menu SET path=$1 WHERE menu_id=$2",
                        {url, std::to_string(mid)});
                    LOG_INFO << "[Menu] 菜单 " << mid << " URL 已校正: " << url;
                    std::cout << "[Menu] " << mid << " -> " << url << std::endl;
                };

                for (auto& [mid, suffix] : kMenuSuffixes) {
                    std::string url;
                    if (mid == 120
                        && mroot.isMember("menu")
                        && mroot["menu"].isMember("logfile_external_url")
                        && !mroot["menu"]["logfile_external_url"].asString().empty()) {
                        url = mroot["menu"]["logfile_external_url"].asString();
                    } else if (!baseUrl.empty()) {
                        url = baseUrl + suffix;
                    } else {
                        continue;
                    }
                    updateMenu(mid, url);
                }
                for (auto& [mid, suffix] : kDirectSuffixes) {
                    if (baseUrlDirect.empty()) continue;
                    updateMenu(mid, baseUrlDirect + suffix);
                }
            } catch (const std::exception& e) {
                LOG_WARN << "[Menu] 校正菜单 URL 失败: " << e.what();
            }

            // ── 设备绑定检查 ─────────────────────────────────────────────────
            {
                std::ifstream dbCfgF(configFile);
                if (dbCfgF.is_open()) {
                    Json::Value dbRoot; Json::CharReaderBuilder dbRb; std::string dbErrs;
                    if (Json::parseFromStream(dbRb, dbCfgF, &dbRoot, &dbErrs)) {
                        DeviceBinding::Config dbCfgBind;
                        if (dbRoot.isMember("device_binding")) {
                            auto& db2 = dbRoot["device_binding"];
                            dbCfgBind.enabled       = db2.get("enabled",        false).asBool();
                            dbCfgBind.localKeyFile  = db2.get("local_key_file", "device_key.pem").asString();
                            dbCfgBind.vaultSecretPath = db2.get("vault_secret_path", "secret/ruoyi-cpp").asString();
                            dbCfgBind.vaultKeyField   = db2.get("vault_key_field",   "device_private_key").asString();
                        }
                        if (dbRoot.isMember("vault")) {
                            auto& vt = dbRoot["vault"];
                            dbCfgBind.vault.enabled  = vt.get("enabled",  false).asBool();
                            dbCfgBind.vault.exePath  = vt.get("exe_path", "").asString();
                            dbCfgBind.vault.addr     = vt.get("addr",     "http://127.0.0.1:8200").asString();
                            dbCfgBind.vault.token    = vt.get("token",    "").asString();
                        }
                        if (!DeviceBinding::check(dbCfgBind)) {
                            std::cout << "[DeviceBinding] 设备验证失败，服务器拒绝启动。" << std::endl;
                            std::cout << "重置方法: DELETE FROM sys_device_binding WHERE id=1; 后重启" << std::endl;
                            drogon::app().quit();
                        }
                    }
                }
            }

            LOG_INFO << "加载配置缓存...";
            SysConfigService::instance().loadConfigCache();

            LOG_INFO << "加载字典缓存...";
            SysDictService::instance().loadDictCache();

            // SMTP 配置在数据库就绪后加载
            SysEmailConfigCtrl::reloadSmtp();

            // RateLimiter 定时清理（每 60s 一次，防止 IP 记录无限增长）
            std::thread([]() {
                while (true) {
                    std::this_thread::sleep_for(std::chrono::seconds(60));
                    RateLimiter::instance().cleanup();
                }
            }).detach();

            // ── 子进程启动（在 loadConfigCache 后，确保读到数据库开关）─────────
            // KoboldCpp AI
            if (isPrimary) {
                std::ifstream cfgF2(configFile);
                if (cfgF2.is_open()) {
                    Json::Value root;
                    Json::CharReaderBuilder rb2; std::string errs2;
                    if (Json::parseFromStream(rb2, cfgF2, &root, &errs2) && root.isMember("koboldcpp")) {
                        auto& k = root["koboldcpp"];
                        if (k.get("enabled", false).asBool()) {
                            auto kcSw = SysConfigService::instance().selectConfigByKey("sys.subprocess.koboldcpp");
                            if (kcSw == "false") {
                                std::cout << "[KoboldCpp] sys_config 已禁用，跳过" << std::endl;
                            } else {
                                KoboldCppConfig kc;
                                kc.enabled      = true;
                                kc.launchCmd    = k.get("launch_cmd",   "").asString();
                                kc.pythonExe    = k.get("python",       "python").asString();
                                kc.scriptPath   = k.get("script",       "").asString();
                                kc.modelPath    = k.get("model_path",   "").asString();
                                kc.whisperModel = k.get("whisper_model","").asString();
                                kc.port         = k.get("port",         5001).asInt();
                                kc.threads      = k.get("threads",      4).asInt();
                                kc.contextSize  = k.get("context_size", 2048).asInt();
                                kc.blasBatch    = k.get("blas_batch",   512).asInt();
                                kc.useGpu       = k.get("use_gpu",      false).asBool();
                                kc.gpuLayers    = k.get("gpu_layers",   99).asInt();
                                kc.showWindow   = k.get("show_window",  false).asBool();
                                kc.workDir      = k.get("work_dir",     "").asString();
                                KoboldCppService::instance().setPort(kc.port);
                                WhisperService::instance().setPort(kc.port);
                                KoboldCppManager::instance().start(kc);
                                std::atexit([]{ KoboldCppManager::instance().stop(); });
                            }
                        } else {
                            std::cout << "[KoboldCpp] config.json 中已禁用，跳过" << std::endl;
                        }
                    }
                }
            }
            // DDNS-go
            if (isPrimary) {
                std::ifstream cfgD(configFile);
                if (cfgD.is_open()) {
                    Json::Value root;
                    Json::CharReaderBuilder rbd; std::string errsd;
                    if (Json::parseFromStream(rbd, cfgD, &root, &errsd) && root.isMember("ddns")) {
                        auto& d = root["ddns"];
                        if (d.get("enabled", false).asBool()) {
                            auto ddnsSw = SysConfigService::instance().selectConfigByKey("sys.subprocess.ddns");
                            if (ddnsSw == "false") {
                                std::cout << "[DDNS] sys_config 已禁用，跳过" << std::endl;
                            } else {
                                DdnsGoConfig dc;
                                dc.enabled     = true;
                                dc.exePath     = d.get("exe_path",    "").asString();
                                dc.configPath  = d.get("config_path", "").asString();
                                dc.frequency   = d.get("frequency",   300).asInt();
                                dc.listenAddr  = d.get("listen",      ":9876").asString();
                                dc.noWeb       = d.get("no_web",      false).asBool();
                                dc.skipVerify  = d.get("skip_verify", false).asBool();
                                dc.showWindow  = d.get("show_window", false).asBool();
                                DdnsGoManager::instance().start(dc);
                                std::atexit([]{ DdnsGoManager::instance().stop(); });
                            }
                        } else {
                            std::cout << "[DDNS] config.json 中已禁用，跳过" << std::endl;
                        }
                    }
                }
            }
            // Nginx
            if (isPrimary) {
                NginxConfig ngCfg;
                std::ifstream cfgF(configFile);
                if (cfgF.is_open()) {
                    Json::Value root;
                    Json::CharReaderBuilder rb; std::string errs;
                    if (Json::parseFromStream(rb, cfgF, &root, &errs) && root.isMember("nginx")) {
                        auto& ng = root["nginx"];
                        ngCfg.enabled     = ng.get("enabled",     true).asBool();
                        ngCfg.exePath     = ng.get("exe_path",    "nginx/nginx.exe").asString();
                        ngCfg.prefix      = ng.get("prefix",      "nginx/").asString();
                        ngCfg.port        = ng.get("port",        18081).asInt();
                        ngCfg.autoRestart = ng.get("autoRestart", true).asBool();
                        ngCfg.maxRestarts = ng.get("maxRestarts", 5).asInt();
                    }
                }
                if (ngCfg.enabled) {
                    auto ngSw = SysConfigService::instance().selectConfigByKey("sys.subprocess.nginx");
                    if (ngSw == "false") {
                        std::cout << "[NGINX] sys_config 已禁用，跳过" << std::endl;
                    } else {
                        NginxManager::instance().init(ngCfg);
                        NginxManager::instance().start();
                        std::atexit([]{ NginxManager::instance().stop(); });
                    }
                } else {
                    std::cout << "[NGINX] config.json 中已禁用，跳过" << std::endl;
                }
            }

            LOG_INFO << "RuoYi-Cpp 启动完成，监听 " << listenAddr << ":" << listenPort;
            // 用 stderr 输出佛祖横幅与启动成功提示 —— stderr 不受 app.console_output
            // 控制的 stdout 重定向影响，确保关键启动信号始终在终端可见
            std::cerr <<
                "\x1b[38;2;255;215;0m"  // GOLD：佛祖保佑横幅
                "\n"
                "////////////////////////////////////////////////////////////////////\n"
                "//                          _ooOoo_                               //\n"
                "//                         o8888888o                              //\n"
                "//                         88\" . \"88                              //\n"
                "//                         (| ^_^ |)                              //\n"
                "//                         O\\  =  /O                              //\n"
                "//                      ____/`---'\\____                           //\n"
                "//                    .'  \\\\|     |//  `.                         //\n"
                "//                   /  \\\\|||  :  |||//  \\                        //\n"
                "//                  /  _||||| -:- |||||-  \\                       //\n"
                "//                  |   | \\\\\\  -  /// |   |                       //\n"
                "//                  | \\_|  ''\\---/''  |   |                       //\n"
                "//                  \\  .-\\__  `-`  ___/-. /                       //\n"
                "//                ___`. .'  /--.--\\  `. . ___                     //\n"
                "//              .\"\" '<  `.___\\_<|>_/___.'  >\"\"\".                  //\n"
                "//            | | :  `- \\`.;`\\ _ /`;.`/ - ` : | |                //\n"
                "//            \\  \\ `-.   \\_ __\\ /__ _/   .-` /  /                //\n"
                "//      ========`-.____`-.___\\_____/___.-`____.-'========         //\n"
                "//                           `=---='                              //\n"
                "//      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^        //\n"
                "//             佛祖保佑       永不宕机      永无BUG               //\n"
                "////////////////////////////////////////////////////////////////////\n"
                "\x1b[0m"  // 关闭金黄色
                "\n"
                "  RuoYi-Cpp started  |  " << listenAddr << ":" << listenPort << "\n"
                "\n";
        });

        // 启动 Cron 定时任务调度器（仅主实例运行，避免多实例重复执行）
        if (isPrimary) {
            JobScheduler::instance().init();
            std::atexit([]{ JobScheduler::instance().stop(); });
        } else {
            std::cout << "[Cluster] worker 模式，跳过 JobScheduler/Nginx/DDNSGo/KoboldCpp" << std::endl;
        }

        // ── 启动时从 sys_token 恢复在线会话（Token 持久化）────────────────────
        {
            auto& db = DatabaseService::instance();
            long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            // 先清理已过期 token
            db.execParams("DELETE FROM sys_token WHERE expire_time<$1",
                          {std::to_string(nowMs)});
            // 恢复有效 token
            auto res = db.query("SELECT token_key,token_value FROM sys_token");
            int recovered = 0;
            if (res.ok()) {
                auto& cfg = JwtUtils::config();
                for (int i = 0; i < res.rows(); ++i) {
                    std::string key = res.str(i, 0);
                    std::string val = res.str(i, 1);
                    Json::Value j; Json::Reader r;
                    if (!r.parse(val, j)) continue;
                    auto user = LoginUser::fromJson(j);
                    TokenCache::instance().set(key, user, cfg.expireMinutes);
                    ++recovered;
                }
            }
            LOG_INFO << "[TokenRestore] 已恢复 " << recovered << " 个在线会话";
            std::cout << "[TokenRestore] 恢复 " << recovered << " 个在线会话" << std::endl;
        }

        // ── 启动时归档过期日志（防止 sys_oper_log / sys_logininfor 表无限膨胀）
        // 默认保留 90 天；可通过 config.json 中 retention.oper_log_days / login_log_days 调整
        {
            auto& db = DatabaseService::instance();
            int operDays  = 90;
            int loginDays = 180;
            try {
                auto& cfg = drogon::app().getCustomConfig();
                if (cfg.isMember("retention")) {
                    operDays  = cfg["retention"].get("oper_log_days",  90).asInt();
                    loginDays = cfg["retention"].get("login_log_days", 180).asInt();
                }
            } catch (...) {}
            if (operDays > 0) {
                std::string d = std::to_string(operDays);
                db.execParams(
                    "DELETE FROM sys_oper_log WHERE oper_time < NOW() - ($1 || ' days')::INTERVAL",
                    {d});
            }
            if (loginDays > 0) {
                std::string d = std::to_string(loginDays);
                db.execParams(
                    "DELETE FROM sys_logininfor WHERE login_time < NOW() - ($1 || ' days')::INTERVAL",
                    {d});
            }
            LOG_INFO << "[Retention] sys_oper_log >" << operDays << "d, sys_logininfor >" << loginDays << "d cleaned";
        }

        // ── HTTPS 启动（读取 ./ssl/config.json，重启后生效）──────────────────
        {
            // Step 1: 早连接 DB，将证书/私钥从 sys_ssl_cert 表同步到磁盘
            // （保证即使磁盘文件丢失，重启后仍能从 DB 恢复）
            {
                std::string connStr = buildDbConnStr(configFile, 3);

                if (!connStr.empty()) {
                    auto& db = DatabaseService::instance();
                    if (!db.isConnected()) db.connect(connStr);
                    // PG 冷启动时可能第一次超时，等 1s 重试一次
                    if (!db.isConnected()) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        db.connect(connStr);
                    }
                    if (db.isConnected()) {
                        // 建表（首次启动时可能不存在）
                        db.exec("CREATE TABLE IF NOT EXISTS sys_ssl_cert ("
                                "  cert_key   VARCHAR(50) PRIMARY KEY,"
                                "  cert_val   TEXT        NOT NULL DEFAULT '',"
                                "  update_time TIMESTAMP  DEFAULT NOW()"
                                ")");
                        // 同步证书
                        auto cr = db.queryParams(
                            "SELECT cert_val FROM sys_ssl_cert WHERE cert_key=$1", {"cert_pem"});
                        if (cr.ok() && cr.rows() > 0 && !cr.str(0,0).empty())
                            SslManager::writeCert(cr.str(0,0));
                        // 同步私钥
                        auto kr = db.queryParams(
                            "SELECT cert_val FROM sys_ssl_cert WHERE cert_key=$1", {"key_pem"});
                        if (kr.ok() && kr.rows() > 0 && !kr.str(0,0).empty())
                            SslManager::writeKey(kr.str(0,0));
                        // 同步配置（覆盖 ssl/config.json，以 DB 为准）
                        auto syncCfgVal = [&](const std::string& key) -> std::string {
                            auto r = db.queryParams(
                                "SELECT cert_val FROM sys_ssl_cert WHERE cert_key=$1", {key});
                            return (r.ok() && r.rows() > 0) ? r.str(0,0) : "";
                        };
                        std::string dbEnabled = syncCfgVal("ssl_enabled");
                        if (!dbEnabled.empty()) {
                            SslManager::Config c;
                            c.enabled    = (dbEnabled == "1");
                            c.httpsPort  = SecurityUtils::parseInt(
                                              syncCfgVal("ssl_https_port"), 18443);
                            c.httpPort   = SecurityUtils::parseInt(
                                              syncCfgVal("ssl_http_port"),  18080);
                            c.forceHttps = (syncCfgVal("ssl_force_https") == "1");
                            SslManager::saveConfig(c);
                        }
                        LOG_INFO << "[SSL] cert/config synced from DB";
                    }
                }
            }

            // Step 2: 从磁盘读取最终 SSL 配置
            auto sslCfg = SslManager::loadConfig();
            if (sslCfg.enabled && SslManager::certExists()) {
                // 启动时检查证书有效期（30 天内 WARN，7 天内 ERROR）
                SslManager::checkCertOnStartup(30, 7);
                drogon::app().addListener("0.0.0.0", (uint16_t)sslCfg.httpsPort,
                                          true,
                                          SslManager::CERT_PATH,
                                          SslManager::KEY_PATH);
                LOG_INFO << "[SSL] HTTPS listener on port " << sslCfg.httpsPort;
                std::cout << "[SSL] HTTPS 已启用，端口 " << sslCfg.httpsPort << std::endl;

                if (sslCfg.forceHttps) {
                    int httpsPort = sslCfg.httpsPort;
                    // 强制 HTTP→HTTPS 跳转：仅对 HTTP 端口的请求做 301 重定向
                    // /health 豁免（供负载均衡 TCP 探测）
                    drogon::app().registerPreRoutingAdvice(
                        [httpsPort](const drogon::HttpRequestPtr& req,
                                    drogon::AdviceCallback&& acb,
                                    drogon::AdviceChainCallback&& accb) {
                            // 已经是 HTTPS 端口 → 放行
                            if (req->localAddr().toPort() == (uint16_t)httpsPort) {
                                accb(); return;
                            }
                            // /health 豁免（负载均衡 HTTP 探测）
                            if (std::string(req->path()) == "/health") {
                                accb(); return;
                            }
                            // 从 Host 头提取主机名（去掉端口部分）
                            std::string host = req->getHeader("Host");
                            auto colon = host.rfind(':');
                            if (colon != std::string::npos) host = host.substr(0, colon);
                            if (host.empty()) host = req->localAddr().toIp();
                            std::string location = "https://" + host
                                + ":" + std::to_string(httpsPort)
                                + std::string(req->path());
                            if (!std::string(req->query()).empty())
                                location += "?" + std::string(req->query());
                            auto resp = drogon::HttpResponse::newHttpResponse();
                            resp->setStatusCode(drogon::k301MovedPermanently);
                            resp->addHeader("Location", location);
                            acb(resp);
                        });
                    LOG_INFO << "[SSL] HTTP→HTTPS force redirect enabled";
                    std::cout << "[SSL] HTTP→HTTPS 强制跳转已启用" << std::endl;
                }
            } else if (sslCfg.enabled) {
                LOG_WARN << "[SSL] HTTPS enabled in config but cert/key not found, HTTP only";
                std::cout << "[SSL] 警告: 证书文件未找到，以 HTTP 模式运行" << std::endl;
            }
        }

        // ── 进程内 nginx 集成（静态链接 libnginx.a）─────────────────────────────
        // 仅 RUOYI_USE_NGINX=ON 编译时真实启动，否则空壳直接返回 false
        // 配置段：config.json 顶层 "nginx_embedded"
        // 直接读 config.json 文件，避免 drogon getCustomConfig 仅返回 custom_config 子段
        // 与 services/NginxManager（外部 nginx.exe 子进程版）共存互不干扰
        try {
            std::ifstream nf(configFile);
            if (nf.is_open()) {
                Json::Value nroot;
                Json::CharReaderBuilder nrb;
                std::string nerrs;
                if (Json::parseFromStream(nrb, nf, &nroot, &nerrs)
                    && nroot.isMember("nginx_embedded")) {
                    auto nc = NginxEmbedded::Config::fromJson(nroot["nginx_embedded"]);

                    // domain 字段：自动修改 nginx.conf server_name 并强制启用 nginx
                    std::string globalDomain;
                    if (nroot.isMember("domain"))
                        globalDomain = nroot["domain"].asString();
                    if (!globalDomain.empty()) {
                        nc.enabled = true; // domain 非空 → 自动启用
                        // 修改 nginx.conf 里所有 server_name _; → server_name {domain};
                        std::string confPath = nc.prefix + "/" + nc.confFile;
                        try {
                            std::ifstream cin_(confPath);
                            if (cin_.is_open()) {
                                std::string buf((std::istreambuf_iterator<char>(cin_)), {});
                                cin_.close();
                                std::string from = "server_name _;", to = "server_name " + globalDomain + ";";
                                size_t p = 0;
                                while ((p = buf.find(from, p)) != std::string::npos)
                                    { buf.replace(p, from.size(), to); p += to.size(); }
                                std::ofstream cout_(confPath);
                                cout_ << buf;
                                LOG_INFO << "[NginxEmbedded] nginx.conf server_name -> " << globalDomain;
                                std::cout << "[NginxEmbedded] nginx.conf server_name -> " << globalDomain << std::endl;
                            }
                        } catch (const std::exception& ce) {
                            LOG_WARN << "[NginxEmbedded] 修改 nginx.conf 失败: " << ce.what();
                        }
                    }

                    LOG_INFO << "[NginxEmbedded] enabled=" << nc.enabled
                             << " prefix=" << nc.prefix
                             << " conf=" << nc.confFile;
                    std::cout << "[NginxEmbedded] enabled=" << nc.enabled
                              << " prefix=" << nc.prefix
                              << " conf=" << nc.confFile << std::endl;
                    if (nc.enabled) {
                        // drogon 事件循环启动后再起 nginx（reverse_proxy 依赖后端可达）
                        drogon::app().getLoop()->queueInLoop([nc]() {
                            NginxEmbedded::instance().start(nc);
                        });
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_WARN << "[NginxEmbedded] 初始化失败: " << e.what();
        }

        // ── ACME 证书自动续期（仅 worker[0] 或单进程时启动）────────────────
        // 多 worker 进程下只在 index=0 的 worker 启 ACME，避免并行续期
        try {
            int wkIdx = WorkerOrchestrator::currentWorkerIndex();
            if (wkIdx == -1 || wkIdx == 0) {
                std::ifstream af(configFile);
                if (af.is_open()) {
                    Json::Value aroot;
                    Json::CharReaderBuilder arb;
                    std::string aerrs;
                    if (Json::parseFromStream(arb, af, &aroot, &aerrs)
                        && aroot.isMember("acme")) {
                        // 优先使用 certmanager 动态库（含 dns_provider 且库文件存在时）
                        auto cmc = CertManagerAcme::Config::fromJson(aroot["acme"]);
                        // domain 字段：acme.domains 为空时自动用顶层 domain
                        if (cmc.domains.empty() && aroot.isMember("domain")
                            && !aroot["domain"].asString().empty())
                            cmc.domains.push_back(aroot["domain"].asString());
                        bool usedCertManager = false;
                        if (cmc.enabled && !cmc.dnsProvider.empty())
                            usedCertManager = CertManagerAcme::instance().start(cmc);
                        if (!usedCertManager) {
                            // fallback: 原 win-acme / acme.sh 子进程方式
                            auto ac = AcmeManager::Config::fromJson(aroot["acme"]);
                            if (ac.enabled) AcmeManager::instance().start(ac);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_WARN << "[ACME] 初始化失败: " << e.what();
        }

        // ── 心跳线程：每 2 秒写 .watchdog_heartbeat，让守护进程检测假死 ─────
        std::atomic<bool> hbStop{false};
        std::thread hbThread([&hbStop]() {
            while (!hbStop.load()) {
                try {
                    std::ofstream f(".watchdog_heartbeat", std::ios::trunc);
                    f << std::time(nullptr);
                } catch (...) {}
                for (int i = 0; i < 20 && !hbStop.load(); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

        drogon::app().run();

        hbStop.store(true);
        if (hbThread.joinable()) hbThread.join();
        std::filesystem::remove(".watchdog_heartbeat");

        // ── 退出清理：先停反向代理（停止接收新连接，让 drogon 排空）─────
        try { CertManagerAcme::instance().stop(); } catch (...) {}
        try { AcmeManager::instance().stop(); } catch (...) {}
        try { NginxEmbedded::instance().stop(); } catch (...) {}
    } catch (const std::exception &e) {
        std::cerr << "[致命错误] " << e.what() << std::endl;
        std::cout << "按回车键退出..." << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        std::cerr << "[致命错误] 未知异常" << std::endl;
        std::cout << "按回车键退出..." << std::endl;
        std::cin.get();
        return 1;
    }
    return 0;
}
