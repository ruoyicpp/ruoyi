/**
 * @file StartupAdvice.cc
 * @brief registerBeginningAdvice 回调（原 main.cc 后半部分）
 *
 * drogon 事件循环启动后、数据库就绪前执行一次性初始化：
 *   1. 数据库连接（PostgreSQL / MySQL 适配器 / SQLite 加密与回退）
 *   2. DatabaseInit::run() + 菜单缓存清理
 *   3. SmsService / DataSourceManager 初始化
 *   4. InnerLink 菜单 URL 统一校正（api_base_url / domain / 自动推断）
 *   5. 设备绑定检查（DeviceBinding）
 *   6. 配置/字典缓存加载、ConfigReloader、SMTP
 *   7. RateLimiter 清理线程
 *   8. 子进程启动（KoboldCpp / DDNS-go / Nginx / StorageService /
 *      ExternalServices）
 *   9. 启动完成日志 + 佛祖横幅（stderr）
 */

#include "AppBootstrap.h"

namespace boot {

void registerStartupAdvice(AppContext& ctx) {
    auto& configFile = ctx.configFile;
    auto& cfgLoader  = ctx.cfgLoader;
    bool  isPrimary  = ctx.isPrimary;

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
            // 检查数据库类型
            std::string dbType = dbCfg.get("type", "postgresql").asString();
            std::transform(dbType.begin(), dbType.end(), dbType.begin(), ::tolower);

            int displayPort = dbCfg.get("port", 5432).asInt();
            if (dbType == "mysql" || dbType == "mariadb") {
                displayPort = dbCfg.get("port", 3306).asInt();
            }

            LOG_INFO << "正在连接数据库: " << dbCfg.get("host","127.0.0.1").asString()
                     << ":" << displayPort
                     << "/" << dbCfg.get("dbname","ruoyi").asString()
                     << " (type=" << dbType << ")";

            // 如果是 MySQL，跳过 PostgreSQL 连接，由 DatabaseAdapter 处理
            bool pgOk = false;
            if (dbType == "mysql" || dbType == "mariadb") {
                LOG_INFO << "[MySQL] 使用 DatabaseAdapter 处理 MySQL 连接，跳过 PostgreSQL 驱动";
                pgOk = false;  // MySQL 不使用 PostgreSQL 驱动
            } else {
                // PostgreSQL 或其他类型，使用 PostgreSQL 驱动
                pgOk = DatabaseService::instance().connect(buildDbConnStr(*cfgLoader, 5));
            }
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

        // ── 企业增强：短信通道 + 多数据源注册表（需 DB 就绪）──────────────
        {
            auto& rootCfg = drogon::app().getCustomConfig();
            if (rootCfg.isMember("sms"))
                SmsService::instance().init(rootCfg["sms"]);
            DataSourceManager::instance().loadAll();
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
                {1101, "/certmanager"},
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

        // sys.cfg.* 覆盖层：DB 就绪后应用一次（参数设置里的值覆盖 config.json）
        try { ConfigReloader::instance().reload(configFile); }
        catch (const std::exception& e) {
            LOG_WARN << "[ConfigReload] boot reload: " << e.what();
        }

        LOG_INFO << "加载字典缓存...";
        SysDictService::instance().loadDictCache();

        // SMTP 配置在数据库就绪后加载
        SysEmailConfigCtrl::reloadSmtp();

        // RateLimiter 定时清理（每 60s 一次，防止 IP 记录无限增长）
        std::thread([]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(60));
                ::RateLimiter::instance().cleanup();
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
            // ── StorageService 初始化（本地 / MinIO / S3）────────────────────
            {
                std::ifstream scf(configFile);
                if (scf.is_open()) {
                    Json::Value sr; Json::CharReaderBuilder srb; std::string se;
                    if (Json::parseFromStream(srb, scf, &sr, &se) && sr.isMember("storage")) {
                        StorageService::instance().init(sr["storage"]);
                        LOG_INFO << "[Storage] backend=" << sr["storage"].get("type","local").asString()
                                 << " endpoint=" << sr["storage"].get("endpoint","").asString();
                    } else {
                        Json::Value def; def["type"] = "local"; def["local_path"] = "./upload";
                        StorageService::instance().init(def);
                        LOG_INFO << "[Storage] backend=local (config.json 无 storage 段)";
                    }
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

        // ── 外部服务管理器（进程监控 + 自动代理路由）──────────────────────
        if (isPrimary) {
            std::ifstream esF(configFile);
            if (esF.is_open()) {
                Json::Value esRoot;
                Json::CharReaderBuilder esRb;
                std::string esErrs;
                if (Json::parseFromStream(esRb, esF, &esRoot, &esErrs)
                    && esRoot.isMember("external_services")) {
                    auto& es = esRoot["external_services"];
                    if (es.get("enabled", false).asBool()) {
                        auto esSw = SysConfigService::instance().selectConfigByKey("sys.external_services");
                        if (esSw == "false") {
                            std::cout << "[ExternalService] sys_config 已禁用，跳过" << std::endl;
                        } else {
                            std::vector<ruoyi::ExternalServiceConfig> configs;
                            if (es.isMember("services") && es["services"].isArray()) {
                                for (auto& svc : es["services"]) {
                                    ruoyi::ExternalServiceConfig cfg;
                                    cfg.name         = svc.get("name", "").asString();
                                    cfg.displayName  = svc.get("display_name", "").asString();
                                    cfg.exeName      = svc.get("exe_name", "").asString();
                                    cfg.port         = svc.get("port", 0).asInt();
                                    cfg.pathPrefix   = svc.get("path_prefix", "").asString();
                                    cfg.upstream     = svc.get("upstream", "").asString();
                                    cfg.stripPrefix  = svc.get("strip_prefix", true).asBool();
                                    cfg.enabled      = svc.get("enabled", true).asBool();
                                    if (!cfg.name.empty()) configs.push_back(cfg);
                                }
                            }
                            if (!configs.empty()) {
                                ruoyi::ExternalServiceManager::instance().init(configs);
                                ruoyi::ExternalServiceManager::instance().start();
                                std::atexit([]{ ruoyi::ExternalServiceManager::instance().stop(); });
                                std::cout << "[ExternalService] 已加载 " << configs.size() << " 个外部服务" << std::endl;
                            }
                        }
                    }
                }
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
}

} // namespace boot
