#include "AppIncludes.h"

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

int main(int argc, char* argv[]) {
    // 最先安装崩溃日志捕获，确保任何时刻崩溃都有记录
    CrashHandler::install("./logs");

    try {
#ifdef _WIN32
        // Windows 控制台设置 UTF-8 编码（参考 config-center-gateway）
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
        std::cout <<
            "\n"
            "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97      \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \n"
            "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\n"
            "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\n"
            "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d \n"
            "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \n"
            "  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d    \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d   \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d      \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d     \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d     \n"
            "\n";
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
            std::filesystem::create_directories(logDir);
            JsonLogger::instance().init(logDir, logBase, maxSizeBytes, keepFiles);
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
                        LOG_INFO << "[RateLimit] enabled=" << cfg.enabled
                                 << " max=" << cfg.maxRequests
                                 << "/" << cfg.windowSeconds << "s";
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

        // ── IP 限流 (DDoS 防御) ─────────────────────────────────────────────────
        drogon::app().registerPreRoutingAdvice(
            [](const drogon::HttpRequestPtr &req,
               drogon::AdviceCallback &&acb,
               drogon::AdviceChainCallback &&accb) {
                std::string ip = IpUtils::getIpAddr(req);
                if (!RateLimiter::instance().allow(ip)) {
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

        // ── 安全响应头（XSS/点击劫持/内容嗅探防御）────────────────────────────
        drogon::app().registerPostHandlingAdvice(
            [](const drogon::HttpRequestPtr&,
               const drogon::HttpResponsePtr& resp) {
                resp->addHeader("X-Content-Type-Options",  "nosniff");
                resp->addHeader("X-Frame-Options",         "SAMEORIGIN");
                resp->addHeader("X-XSS-Protection",        "1; mode=block");
                resp->addHeader("Referrer-Policy",         "strict-origin-when-cross-origin");
                resp->addHeader("Content-Security-Policy",
                    "default-src 'self'; script-src 'self' 'unsafe-inline'; "
                    "style-src 'self' 'unsafe-inline'; img-src 'self' data:");
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

                // 引导接口、重置接口、健康检查、版本接口完全放行
                if (path == "/challenge" || path == "/resetPassword" || path == "/forgotPassword"
                    || path == "/health" || path == "/version" || path == "/ssl-config") {
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
            std::cout <<
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
                            c.httpsPort  = std::stoi(syncCfgVal("ssl_https_port").empty()
                                            ? "18443" : syncCfgVal("ssl_https_port"));
                            c.httpPort   = std::stoi(syncCfgVal("ssl_http_port").empty()
                                            ? "18080" : syncCfgVal("ssl_http_port"));
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

        drogon::app().run();
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
