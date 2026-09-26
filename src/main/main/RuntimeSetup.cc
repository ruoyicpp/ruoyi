/**
 * @file RuntimeSetup.cc
 * @brief 运行时服务启动与退出清理（原 main.cc 末尾部分）
 *
 * startRuntimeServices()：
 *   1. JobScheduler 定时任务调度器（仅主实例）
 *   2. IotCtrl::loadFromDb() 加载 IoT 设备
 *   3. 从 sys_token 恢复在线会话（Token 持久化）
 *   4. 归档过期日志（sys_oper_log / sys_logininfor retention）
 *   5. HTTPS 启动（sys_ssl_cert 表 → 磁盘 → SSL listener + 强制跳转）
 *   6. NginxEmbedded（进程内 nginx，静态链接 libnginx.a）
 *   7. ACME 证书自动续期（仅 worker[0] 或单进程）
 *   8. TaskQueue 异步任务队列
 *   9. watchdog 心跳线程（每 2 秒写 .watchdog_heartbeat）
 *
 * shutdownCleanup()：
 *   run() 返回后停止心跳线程并做退出清理
 *   （TaskQueue / CertManagerAcme / AcmeManager / NginxEmbedded）
 */

#include "AppBootstrap.h"

namespace boot {

// 心跳线程状态（startRuntimeServices 启动，shutdownCleanup 停止）
static std::atomic<bool> g_hbStop{false};
static std::thread       g_hbThread;

void startRuntimeServices(AppContext& ctx) {
    auto& configFile = ctx.configFile;
    bool  isPrimary  = ctx.isPrimary;

    // 启动 Cron 定时任务调度器（仅主实例运行，避免多实例重复执行）
    if (isPrimary) {
        JobScheduler::instance().init();
        std::atexit([]{ JobScheduler::instance().stop(); });
    } else {
        std::cout << "[Cluster] worker 模式，跳过 JobScheduler/Nginx/DDNSGo/KoboldCpp" << std::endl;
    }

    // ── 启动时从 DB 加载 IoT 设备 ──────────────────────────────────────────
    IotCtrl::loadFromDb();

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

    // ── 异步任务队列（taskQueue）────────────────────────────────────
    // Redis 后端为主；Redis 不可用且 fallbackToMemory=true 时回退到进程内队列
    try {
        std::ifstream tqf(configFile);
        if (tqf.is_open()) {
            Json::Value troot;
            Json::CharReaderBuilder trb;
            std::string terrs;
            if (Json::parseFromStream(trb, tqf, &troot, &terrs)) {
                if (troot.isMember("taskQueue")) {
                    // 只在主进程或 worker[0] 启动 worker 线程，避免多 worker
                    // 重复消费导致任务被处理 N 次
                    int wkIdx = WorkerOrchestrator::currentWorkerIndex();
                    if (wkIdx == -1 || wkIdx == 0) {
                        TaskQueue::instance().init(troot["taskQueue"]);
                        TQ::registerBuiltinHandlers();
                        TaskQueue::instance().start();
                        LOG_INFO << "[Main] TaskQueue 初始化完成，backend="
                                 << TaskQueue::instance().backendInfo();
                    } else {
                        // 其他 worker 仍注册 handler（供跨进程 RPC 调用）
                        TaskQueue::instance().init(troot["taskQueue"]);
                        TQ::registerBuiltinHandlers();
                        LOG_INFO << "[Main] TaskQueue 已在 worker[" << wkIdx
                                 << "] 跳过 worker 线程启动";
                    }
                } else {
                    LOG_INFO << "[Main] config.json 未配置 taskQueue，任务队列未启用";
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_WARN << "[TaskQueue] 初始化失败: " << e.what();
    }

    // ── 心跳线程：每 2 秒写 .watchdog_heartbeat，让守护进程检测假死 ─────
    g_hbStop.store(false);
    g_hbThread = std::thread([]() {
        // 立即写第一次，让 watchdog 宽限期内就能看到有效心跳
        auto writeHb = []() {
            try {
                std::ofstream f(".watchdog_heartbeat", std::ios::trunc);
                f << std::time(nullptr);
            } catch (...) {}
        };
        writeHb();
        while (!g_hbStop.load()) {
            for (int i = 0; i < 20 && !g_hbStop.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!g_hbStop.load()) writeHb();
        }
    });
}

void shutdownCleanup() {
    g_hbStop.store(true);
    if (g_hbThread.joinable()) g_hbThread.join();
    std::filesystem::remove(".watchdog_heartbeat");

    // ── 退出清理：先停反向代理（停止接收新连接，让 drogon 排空）─────
    try { TaskQueue::instance().stop(); } catch (...) {}
    try { CertManagerAcme::instance().stop(); } catch (...) {}
    try { AcmeManager::instance().stop(); } catch (...) {}
    try { NginxEmbedded::instance().stop(); } catch (...) {}
}

} // namespace boot
