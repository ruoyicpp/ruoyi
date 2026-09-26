/**
 * @file ConfigInit.cc
 * @brief 配置与服务初始化阶段（原 main.cc 中部）
 *
 * 顺序与原文件一致：
 *   1. 解析 --config <file>
 *   2. DefaultConfig::ensure（缺失则自动生成默认配置）
 *   3. 许可证校验 + LicenseWatcher
 *   4. 集群角色预读 + nginx upstream.conf 生成
 *   5. JwtAuthFilter 注册、drogon::app().loadConfigFile
 *   6. DatabaseAdapter 初始化（MySQL/PostgreSQL 运行时选择）
 *   7. Config Schema 校验 + 插件自动加载
 *   8. 日志系统：本地时间、JsonLogger/ErrorLogger、stdout 重定向
 *   9. Vault 子进程启动、ConfigLoader 构建、LogCollector 系列
 *   10. Cache 配置（CacheStrategy/Warmup/Invalidation）
 *   11. JWT 配置（Vault 补全 secret）
 *   12. 安全配置（RateLimiter/SignUtils/IpUtils/WAF/AuditQueue/SsoServer）
 */

#include "AppBootstrap.h"

namespace boot {

std::optional<int> configInit(int argc, char* argv[], AppContext& ctx) {
    // ── 解析命令行参数 --config <file>（提前：许可证远程验证需要 config 路径）──
    std::string configFile = "config.json";
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--config") {
            configFile = argv[i + 1];
            break;
        }
    }
    ctx.configFile = configFile;

    // ── 首次启动：配置文件缺失则自动生成默认配置（SQLite 零依赖）──────
    if (!DefaultConfig::ensure(configFile)) {
        std::cerr << "[错误] 配置文件 " << configFile
                  << " 不存在且自动生成失败，请手动创建" << std::endl;
        std::cerr << "当前目录: " << std::filesystem::current_path() << std::endl;
        std::cout << "按回车键退出..." << std::endl;
        std::cin.get();
        return 1;
    }

    // ── 许可证校验（必须在加载配置前完成，工作目录已切换到 exe 目录）──────
    LicenseManager::loadRemoteConfig(configFile);   // license.remote 段
    LicenseManager::checkAndPrint();
    LicenseWatcher::instance().start(LicenseManager::_licPath());

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
    ctx.isPrimary = isPrimary;
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

    // ── 初始化数据库适配器（MySQL/PostgreSQL 运行时选择）──────────────────
    {
        std::ifstream dbcf(configFile);
        if (dbcf.is_open()) {
            Json::Value root; Json::CharReaderBuilder rb; std::string errs;
            if (Json::parseFromStream(rb, dbcf, &root, &errs) && root.isMember("database")) {
                auto& adapter = ruoyi::DatabaseAdapter::instance();
                adapter.init(root["database"]);
                LOG_INFO << "[DatabaseAdapter] Initialized with type: " << adapter.getTypeName();
            }
        }
    }

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

        try {
            auto autoLoaded = ruoyi::plugin::PluginManager::instance().autoLoadFromConfig(root);
            if (!autoLoaded.empty()) {
                std::cout << "[Plugin] 已自动加载: ";
                for (size_t i = 0; i < autoLoaded.size(); ++i) {
                    if (i) std::cout << ", ";
                    std::cout << autoLoaded[i];
                }
                std::cout << std::endl;
            }
        } catch (const std::exception& e) {
            LOG_WARN << "[Plugin] 自动加载插件失败: " << e.what();
        }
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
        ErrorLogger::instance().init(logDir, 10 * 1024 * 1024, keepFiles);
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
    ctx.cfgLoader = cfgLoader;

    {
        const auto& root = cfgLoader->raw();
        Log::LogConfig logConfig;
        if (root.isMember("log") && root["log"].isObject()) {
            const auto& log = root["log"];
            logConfig.enabled = log.get("enabled", true).asBool();
            logConfig.path = log.get("path", "./logs").asString();
            logConfig.maxFiles = log.get("max_files", 20).asInt();
            logConfig.maxResults = log.get("max_results", 500).asInt();

            if (log.isMember("alerts") && log["alerts"].isObject()) {
                const auto& alerts = log["alerts"];
                logConfig.alerts.highErrorRate = alerts.get("high_error_rate", logConfig.alerts.highErrorRate).asDouble();
                logConfig.alerts.criticalErrorRate = alerts.get("critical_error_rate", logConfig.alerts.criticalErrorRate).asDouble();
                logConfig.alerts.errorSpikeThreshold = static_cast<std::size_t>(
                    alerts.get("error_spike_threshold", static_cast<Json::UInt64>(logConfig.alerts.errorSpikeThreshold)).asUInt64());
                logConfig.alerts.criticalErrorSpikeCount = static_cast<std::size_t>(
                    alerts.get("critical_error_spike_count", static_cast<Json::UInt64>(logConfig.alerts.criticalErrorSpikeCount)).asUInt64());
                logConfig.alerts.unknownLevelRatio = alerts.get("unknown_level_ratio", logConfig.alerts.unknownLevelRatio).asDouble();
                logConfig.alerts.warningUnknownLevelRatio = alerts.get("warning_unknown_level_ratio", logConfig.alerts.warningUnknownLevelRatio).asDouble();
                logConfig.alerts.parseFailureRatio = alerts.get("parse_failure_ratio", logConfig.alerts.parseFailureRatio).asDouble();
                logConfig.alerts.warningParseFailureRatio = alerts.get("warning_parse_failure_ratio", logConfig.alerts.warningParseFailureRatio).asDouble();
                logConfig.alerts.repeatedMessageCount = static_cast<std::size_t>(
                    alerts.get("repeated_message_count", static_cast<Json::UInt64>(logConfig.alerts.repeatedMessageCount)).asUInt64());
                logConfig.alerts.repeatedMessageRatio = alerts.get("repeated_message_ratio", logConfig.alerts.repeatedMessageRatio).asDouble();
                logConfig.alerts.warningRepeatedMessageRatio = alerts.get("warning_repeated_message_ratio", logConfig.alerts.warningRepeatedMessageRatio).asDouble();
            }

            if (log.isMember("elasticsearch") && log["elasticsearch"].isObject()) {
                const auto& elasticsearch = log["elasticsearch"];
                logConfig.elasticsearch.enabled = elasticsearch.get("enabled", false).asBool();
                logConfig.elasticsearch.host = elasticsearch.get("host", "127.0.0.1").asString();
                logConfig.elasticsearch.port = elasticsearch.get("port", 9200).asInt();
                logConfig.elasticsearch.indexPrefix = elasticsearch.get("index_prefix", "ruoyi-logs").asString();
            }

            if (log.isMember("kibana") && log["kibana"].isObject()) {
                const auto& kibana = log["kibana"];
                logConfig.kibana.enabled = kibana.get("enabled", false).asBool();
                logConfig.kibana.host = kibana.get("host", "127.0.0.1").asString();
                logConfig.kibana.port = kibana.get("port", 5601).asInt();
            }
        }

        if (logConfig.path.empty()) {
            logConfig.path = "./logs";
        }

        Log::LogCollector::instance().init(logConfig);
        Log::LogCollector::instance().start();
        Log::LogSearchEngine::instance().init(logConfig);
        Log::LogAnalyzer::instance().init(logConfig);

        LOG_INFO << "[Log] enabled=" << logConfig.enabled
                 << " path=" << logConfig.path
                 << " max_files=" << logConfig.maxFiles
                 << " max_results=" << logConfig.maxResults
                 << " collector_running=" << Log::LogCollector::instance().isRunning();
    }

    // ── Cache 配置加载 ──────────────────────────────────────────────────────
    {
        const auto& root = cfgLoader->raw();
        Cache::CacheConfig cacheConfig;
        Cache::WarmupConfig warmupConfig;
        std::vector<Cache::InvalidationRule> invalidationRules;

        if (root.isMember("cache") && root["cache"].isObject()) {
            const auto& cache = root["cache"];
            cacheConfig.enabled = cache.get("enabled", true).asBool();
            cacheConfig.localTtlSeconds = cache.get("local_ttl_seconds", 60).asInt();
            cacheConfig.redisTtlSeconds = cache.get("redis_ttl_seconds", 3600).asInt();
            cacheConfig.maxLocalEntries = cache.get("max_local_entries", 10000).asInt();
            cacheConfig.nullValueCaching = cache.get("null_value_caching", true).asBool();
            cacheConfig.nullValueTtlSeconds = cache.get("null_value_ttl_seconds", 60).asInt();
            cacheConfig.lockTimeoutMs = cache.get("lock_timeout_ms", 5000).asInt();
            cacheConfig.enableRandomExpiryJitter = cache.get("enable_random_expiry_jitter", true).asBool();
            cacheConfig.randomExpiryJitterSeconds = cache.get("random_expiry_jitter_seconds", 60).asInt();

            if (cache.isMember("redis_cluster") && cache["redis_cluster"].isObject()) {
                const auto& redisCluster = cache["redis_cluster"];
                cacheConfig.redisCluster.enabled = redisCluster.get("enabled", cacheConfig.enabled).asBool();
                cacheConfig.redisCluster.connectionTimeoutMs = redisCluster.get("connection_timeout_ms", 5000).asInt();
                cacheConfig.redisCluster.commandTimeoutMs = redisCluster.get("command_timeout_ms", 3000).asInt();
                cacheConfig.redisCluster.maxConnectionsPerNode = redisCluster.get("max_connections_per_node", 8).asInt();
                cacheConfig.redisCluster.minIdleConnections = redisCluster.get("min_idle_connections", 2).asInt();
                cacheConfig.redisCluster.maxQueueSize = redisCluster.get("max_queue_size", 1024).asInt();
                cacheConfig.redisCluster.autoReconnect = redisCluster.get("auto_reconnect", true).asBool();
                cacheConfig.redisCluster.reconnectIntervalSeconds = redisCluster.get("reconnect_interval_seconds", 3).asInt();
                if (redisCluster.isMember("nodes") && redisCluster["nodes"].isArray()) {
                    for (const auto& node : redisCluster["nodes"]) {
                        Cache::RedisNode redisNode;
                        redisNode.host = node.get("host", "127.0.0.1").asString();
                        redisNode.port = node.get("port", 6379).asInt();
                        redisNode.password = node.get("password", "").asString();
                        redisNode.db = node.get("db", 0).asInt();
                        redisNode.master = node.get("master", true).asBool();
                        redisNode.nodeId = node.get("node_id", redisNode.host + ":" + std::to_string(redisNode.port)).asString();
                        cacheConfig.redisCluster.nodes.push_back(std::move(redisNode));
                    }
                }
            }

            if (cache.isMember("warmup") && cache["warmup"].isObject()) {
                const auto& warmup = cache["warmup"];
                warmupConfig.enabled = warmup.get("enabled", false).asBool();
                warmupConfig.onStartup = warmup.get("on_startup", true).asBool();
                warmupConfig.intervalSeconds = warmup.get("interval_seconds", 3600).asInt();
                warmupConfig.batchSize = warmup.get("batch_size", 100).asInt();
                warmupConfig.timeoutSeconds = warmup.get("timeout_seconds", 30).asInt();
                if (warmup.isMember("priority_keys") && warmup["priority_keys"].isArray()) {
                    for (const auto& key : warmup["priority_keys"]) {
                        warmupConfig.priorityKeys.push_back(key.asString());
                    }
                }
            }

            if (cache.isMember("invalidation") && cache["invalidation"].isObject()) {
                const auto& invalidation = cache["invalidation"];
                Cache::InvalidationRule defaultRule;
                defaultRule.keyPattern = "*";
                defaultRule.relatedPattern.clear();
                const auto strategy = invalidation.get("default_strategy", "active").asString();
                if (strategy == "passive") {
                    defaultRule.strategy = Cache::InvalidationStrategy::Passive;
                } else if (strategy == "delayed") {
                    defaultRule.strategy = Cache::InvalidationStrategy::Delayed;
                } else if (strategy == "write_behind") {
                    defaultRule.strategy = Cache::InvalidationStrategy::WriteBehind;
                } else {
                    defaultRule.strategy = Cache::InvalidationStrategy::Active;
                }
                defaultRule.ttlSeconds = invalidation.get("default_ttl_seconds", 3600).asInt();
                defaultRule.delayMs = invalidation.get("default_delay_ms", 0).asInt();
                invalidationRules.push_back(defaultRule);
            }
        }

        Cache::CacheStrategy::instance().init(cacheConfig);
        Cache::CacheWarmup::instance().init(warmupConfig);
        Cache::CacheInvalidation::instance().init(invalidationRules);

        LOG_INFO << "[Cache] enabled=" << cacheConfig.enabled
                 << " local_ttl=" << cacheConfig.localTtlSeconds
                 << " redis_ttl=" << cacheConfig.redisTtlSeconds
                 << " warmup_enabled=" << warmupConfig.enabled
                 << " warmup_on_startup=" << warmupConfig.onStartup
                 << " invalidation_rules=" << invalidationRules.size();
    }

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
                    ::RateLimiter::Config cfg;
                    cfg.enabled       = rl.get("enabled", true).asBool();
                    cfg.maxRequests   = rl.get("max_requests", 200).asInt();
                    cfg.windowSeconds = rl.get("window_seconds", 60).asInt();
                    cfg.banSeconds    = rl.get("ban_seconds", 300).asInt();
                    if (rl.isMember("whitelist"))
                        for (auto& ip : rl["whitelist"])
                            cfg.whitelist.push_back(ip.asString());
                    ::RateLimiter::instance().configure(cfg);

                    // ── P3-12: 注入 Redis 后端，实现跨进程限流计数 ─────
                    // Redis 不可用时 RateLimiter 自动降级到内存
                    ::RateLimiter::RedisBackend rb;
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
                        ::RateLimiter::instance().setRedisBackend(rb);
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
                // ── 可信代理名单：只有这些直连对端的 XFF/X-Real-IP 才被采信 ──
                // 部署在 nginx/网关后时把代理 IP 填进来；默认空=不信任何转发头
                {
                    std::vector<std::string> proxies;
                    for (auto& p : sec["trusted_proxies"])
                        proxies.push_back(p.asString());
                    IpUtils::setTrustedProxies(proxies);
                    if (!proxies.empty())
                        LOG_INFO << "[IpUtils] trusted_proxies=" << proxies.size();
                }
                // ── WAF 引擎 + 风控存储 + nftables 内核封禁（仅 Linux）──
#ifdef __linux__
                if (sec.isMember("waf")) {
                    auto& waf = sec["waf"];
                    // 风控存储（封禁名单/票据/计数器持久化）
                    RiskStore::Config rsCfg;
                    if (waf.isMember("risk_store")) {
                        auto& rs = waf["risk_store"];
                        rsCfg.backend     = rs.get("backend", "sqlite").asString();
                        rsCfg.dbPath      = rs.get("db_path", "data/waf_risk.db").asString();
                        rsCfg.rocksdbPath = rs.get("rocksdb_path", "data/waf_risk_rocks").asString();
                    }
                    RiskStore::instance().init(rsCfg);
                    // nftables 内核层封禁（Linux，无权限自动降级）
                    bool nftOn = waf.isMember("nftables") &&
                                 waf["nftables"].get("enabled", false).asBool();
                    NftBan::instance().init(nftOn);
                    // WAF 规则引擎
                    WafEngine::instance().init(waf);
                }
                // ── 安全审计：Manticore 异步批量上报 ──────────────────
                if (sec.isMember("audit")) {
                    auto& au = sec["audit"];
                    AuditQueue::Config aCfg;
                    aCfg.enabled         = au.get("enabled", false).asBool();
                    aCfg.endpoint        = au.get("endpoint", "http://127.0.0.1:7700").asString();
                    aCfg.index           = au.get("index", "waf_logs").asString();
                    aCfg.batchSize       = au.get("batch_size", 100).asInt();
                    aCfg.flushIntervalMs = au.get("flush_interval_ms", 3000).asInt();
                    aCfg.retentionDays   = au.get("retention_days", 30).asInt();
                    aCfg.queueCapacity   = au.get("queue_capacity", 10000).asInt();
                    AuditQueue::instance().init(aCfg);
                }
                // ── SSO 单点登录服务端（OAuth2.0/OIDC）─────────────────
                if (sec.isMember("sso"))
                    SsoServer::instance().init(sec["sso"]);
#endif // __linux__
            }
        }
    }

    return std::nullopt;
}

} // namespace boot
