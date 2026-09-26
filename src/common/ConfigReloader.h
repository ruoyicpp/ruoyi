/**
 * @file ConfigReloader.h
 * @brief 配置热更新 — config.json 修改后自动/手动重载，无需重启后端
 *
 * 功能概述：
 *   - 自动监听：轮询 config.json mtime（默认 5s），变更即重载
 *   - 手动触发：POST /actuator/reload（loopback 限定，见 MetricsCollector）
 *   - 重载范围：jwt / security.rate_limit / database 慢查询阈值+slow_log /
 *     backup / log.retention / sms / security.waf / security.audit /
 *     security.sso / security.mqtt
 *   - 扩展点：onReload(name, fn) 注册自定义重载回调
 *
 * 实现说明：
 *   - 手动解析 config.json 得 root，再调 drogon loadConfigFile 刷新
 *     getCustomConfig()（每次请求现读配置的代码自动生效）
 *   - 对启动时缓存配置的子系统逐一 re-init（幂等）
 *   - 配置项（config.json → config_watch）：
 *       enabled: 自动监听开关（默认 true）
 *       interval_sec: 轮询间隔秒（默认 5）
 */

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <memory>
#include <json/json.h>
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include "JwtUtils.h"
#include "LicenseManager.h"
#include "RateLimiter.h"
#include "IpUtils.h"
#include "../services/DatabaseService.h"
#include "../log/SlowLogQueue.h"
#include "../log/LogRetention.h"
#include "../backup/BackupService.h"
#include "../sms/SmsService.h"
#include "../iot/MqttClient.h"
#include "../captcha/CaptchaClient.h"
#include "NginxEmbedded.h"
#include "../log/LogIndexer.h"
#ifdef __linux__
#  include "../waf/WafEngine.h"
#  include "../audit/AuditQueue.h"
#  include "../sso/SsoServer.h"
#endif

// 注：ConfigReloadHook::fn 声明在 MetricsCollector.h（/actuator/reload 调用方），
//     由 main.cc 绑定到 ConfigReloader::instance().reload()

class ConfigReloader {
public:
    static ConfigReloader& instance() {
        static ConfigReloader inst;
        return inst;
    }

    /// 注册重载回调（模块自定义热更新逻辑）
    void onReload(const std::string& name, std::function<void(const Json::Value&)> fn) {
        std::lock_guard<std::mutex> lk(mu_);
        callbacks_.push_back({name, std::move(fn)});
    }

    /// 执行重载：解析 config.json → 刷新 drogon 配置 → 重设子系统 → 回调
    Json::Value reload(const std::string& path = "config.json") {
        Json::Value result;
        Json::Value root;
        {
            std::ifstream f(path);
            if (!f.is_open()) {
                result["code"] = 500;
                result["msg"]  = "cannot open " + path;
                return result;
            }
            Json::CharReaderBuilder rb;
            std::string errs;
            if (!Json::parseFromStream(rb, f, &root, &errs)) {
                result["code"] = 500;
                result["msg"]  = "config.json parse error: " + errs;
                return result;
            }
        }

        // 刷新 drogon 内部配置（getCustomConfig 读者自动生效）
        try { drogon::app().loadConfigFile(path); }
        catch (const std::exception& e) {
            LOG_WARN << "[ConfigReload] loadConfigFile: " << e.what();
        }

        // loadConfigFile 会把 document_root 重置为 config.json 的 "document_root"
        // （缺省 "."），冲掉启动时 setDocumentRoot(frontend.dist_path) 的设置，
        // 导致 / 和 /index.html 等静态文件全部 404。这里重新应用前端托管目录。
        try {
            bool extFe = root.isMember("frontend") &&
                         root["frontend"].get("enabled", false).asBool();
            bool embFe = root.isMember("embedded_frontend") &&
                         root["embedded_frontend"].get("enabled", false).asBool();
            if (extFe && !embFe) {
                const auto& fc = root["frontend"];
                std::string distPath = fc.get("dist_path", "./web").asString();
                std::error_code ec;
                if (std::filesystem::exists(distPath + "/index.html", ec)) {
                    drogon::app().setDocumentRoot(distPath);
                    drogon::app().setStaticFilesCacheTime(
                        fc.get("cache_seconds", 3600).asInt());
                    drogon::app().enableGzip(true);
                    drogon::app().setGzipStatic(true);
                    drogon::app().setBrStatic(true);
                }
            }
        } catch (const std::exception& e) {
            LOG_WARN << "[ConfigReload] restore document_root: " << e.what();
        }

        int applied = 0;
        applied += applyDbOverrides(root);   // sys_config sys.cfg.* 覆盖层（先覆盖再分发）
        applied += applyJwt(root);
        applied += applyLicenseRemote(root);
        applied += applyTrustedProxies(root);
        applied += applyRateLimit(root);
        applied += applyDatabase(root);
        applied += applySlowLog(root);
        applied += applyBackup(root);
        applied += applyLogRetention(root);
        applied += applyLogManticore(root);
        applied += applySms(root);
        applied += applyMqtt(root);
        applied += applyCaptcha(root);
        applied += applyNginxEmbedded(root);
#ifdef __linux__
        applied += applyWaf(root);
        applied += applyAudit(root);
        applied += applySso(root);
#endif

        // 自定义回调
        std::vector<std::pair<std::string, std::function<void(const Json::Value&)>>> cbs;
        {
            std::lock_guard<std::mutex> lk(mu_);
            cbs = callbacks_;
        }
        for (auto& [name, fn] : cbs) {
            try { fn(root); ++applied; }
            catch (const std::exception& e) {
                LOG_WARN << "[ConfigReload] callback " << name << ": " << e.what();
            }
        }

        lastMtime_ = mtimeOf(path);
        LOG_INFO << "[ConfigReload] applied " << applied << " section(s)";
        result["code"] = 200;
        result["msg"]  = "config reloaded";
        result["data"]["applied"] = applied;
        return result;
    }

    /// 启动 mtime 监听（在 drogon 事件循环上轮询）
    void startWatcher(const std::string& path = "config.json") {
        // 读监听配置（此时 drogon 已 loadConfigFile）
        bool enabled = true;
        double interval = 5.0;
        try {
            auto& cw = drogon::app().getCustomConfig()["config_watch"];
            if (cw.isObject()) {
                enabled  = cw.get("enabled", true).asBool();
                interval = cw.get("interval_sec", 5).asDouble();
                if (interval < 1.0) interval = 1.0;
            }
        } catch (...) {}
        if (!enabled) {
            LOG_INFO << "[ConfigReload] watcher disabled";
            return;
        }
        path_ = path;
        lastMtime_ = mtimeOf(path);
        drogon::app().getLoop()->runEvery(interval, [this]{
            auto mt = mtimeOf(path_);
            if (mt != lastMtime_ && mt != std::filesystem::file_time_type{}) {
                LOG_INFO << "[ConfigReload] config.json changed, reloading";
                reload(path_);
            }
        });
        LOG_INFO << "[ConfigReload] watching config.json every "
                 << interval << "s";
    }

private:
    ConfigReloader() = default;

    static std::filesystem::file_time_type mtimeOf(const std::string& p) {
        std::error_code ec;
        auto t = std::filesystem::last_write_time(p, ec);
        return ec ? std::filesystem::file_time_type{} : t;
    }

    // ── sys_config 覆盖层 ────────────────────────────────────────────
    // config_key='sys.cfg.<dot.path>' 覆盖 root 对应节点，空值=不覆盖。
    // 值按 JSON 解析：true/false→bool，数字→int/double，[...]/{...}→数组/对象，
    // 其余→字符串。覆盖后所有 applyXxx 自动生效（前端参数设置可改）。
    // DB 未就绪（启动早期 watcher 触发）时跳过。
    static int applyDbOverrides(Json::Value& root) {
        auto& db = DatabaseService::instance();
        if (!db.isConnected() && !db.isUsingSqlite()) return 0;
        auto res = db.query(
            "SELECT config_key,config_value FROM sys_config WHERE config_key LIKE 'sys.cfg.%'");
        if (!res.ok()) return 0;
        int n = 0;
        for (int i = 0; i < res.rows(); ++i) {
            std::string path = res.str(i, 0).substr(8);   // 去掉 "sys.cfg."
            std::string val  = res.str(i, 1);
            if (path.empty() || val.empty()) continue;
            setByPath(root, path, parseValue(val));
            ++n;
        }
        if (n) LOG_INFO << "[ConfigReload] sys_config overrides: " << n;
        return n ? 1 : 0;
    }

    /// 按点号路径写入 JSON 树（中间节点不存在则建对象）
    static void setByPath(Json::Value& root, const std::string& path,
                          const Json::Value& v) {
        Json::Value* node = &root;
        size_t pos = 0;
        while (true) {
            size_t dot = path.find('.', pos);
            if (dot == std::string::npos) {
                (*node)[path.substr(pos)] = v;
                return;
            }
            node = &(*node)[path.substr(pos, dot - pos)];
            pos = dot + 1;
        }
    }

    /// DB 字符串 → 类型化 JSON 值（保证下游 asBool/asInt 不抛异常）
    static Json::Value parseValue(const std::string& s) {
        Json::Value v;
        Json::CharReaderBuilder rb;
        std::string errs;
        std::unique_ptr<Json::CharReader> rd(rb.newCharReader());
        if (rd->parse(s.data(), s.data() + s.size(), &v, &errs) &&
            (v.isBool() || v.isNumeric() || v.isArray() || v.isObject()))
            return v;
        return Json::Value(s);
    }

    // ── 各子系统重设（幂等）─────────────────────────────────────────
    /// 嵌入式 nginx 启停：nginx_embedded.enabled 变化时动态 start/stop
    /// 经 queueInLoop 调度，避免在重载线程里阻塞 drogon 事件循环
    static int applyNginxEmbedded(const Json::Value& root) {
        if (!root.isMember("nginx_embedded")) return 0;
        auto nc = NginxEmbedded::Config::fromJson(root["nginx_embedded"]);
        bool wantRun  = nc.enabled;
        bool running  = NginxEmbedded::instance().isRunning();
        if (wantRun == running) return wantRun ? 1 : 0;
        drogon::app().getLoop()->queueInLoop([nc, wantRun]() {
            if (wantRun) {
                LOG_INFO << "[ConfigReload] nginx_embedded: starting";
                NginxEmbedded::instance().start(nc);
            } else {
                LOG_INFO << "[ConfigReload] nginx_embedded: stopping";
                NginxEmbedded::instance().stop();
            }
        });
        return 1;
    }

    /// 可信代理名单：security.trusted_proxies → IpUtils（支持 sys.cfg.* 覆盖）
    static int applyTrustedProxies(const Json::Value& root) {
        if (!root.isMember("security")) return 0;
        auto& sec = root["security"];
        if (!sec.isMember("trusted_proxies")) return 0;
        std::vector<std::string> proxies;
        for (auto& p : sec["trusted_proxies"]) proxies.push_back(p.asString());
        IpUtils::setTrustedProxies(proxies);
        return 1;
    }

    static int applyJwt(const Json::Value& root) {
        if (!root.isMember("jwt")) return 0;
        JwtUtils::loadConfig();   // 处理 RS256 密钥文件等
        auto& j = root["jwt"];    // 手动覆盖标量（不依赖 getCustomConfig 语义）
        auto& c = JwtUtils::config();
        c.secret        = j.get("secret",         c.secret).asString();
        c.issuer        = j.get("issuer",         c.issuer).asString();
        c.audience      = j.get("audience",       c.audience).asString();
        c.expireMinutes = j.get("expire_minutes", c.expireMinutes).asInt();
        c.jwtExpireDays = j.get("jwt_expire_days",c.jwtExpireDays).asInt();
        return 1;
    }

    /// 许可证远程验证参数：license.remote → LicenseManager::remoteCfg()
    /// （sys.cfg.license.remote.* 经 applyDbOverrides 覆盖后在此分发，前端可改）
    static int applyLicenseRemote(const Json::Value& root) {
        if (!root.isMember("license") ||
            !root["license"].isMember("remote")) return 0;
        LicenseManager::applyRemoteJson(root["license"]["remote"]);
        LicenseManager::deriveSelfUrl(root);   // url 留空 → 本服务自身
        return 1;
    }

    static int applyRateLimit(const Json::Value& root) {
        if (!root.isMember("security") ||
            !root["security"].isMember("rate_limit")) return 0;
        auto& rl = root["security"]["rate_limit"];
        ::RateLimiter::Config cfg;
        cfg.enabled       = rl.get("enabled", true).asBool();
        cfg.maxRequests   = rl.get("max_requests", 200).asInt();
        cfg.windowSeconds = rl.get("window_seconds", 60).asInt();
        cfg.banSeconds    = rl.get("ban_seconds", 300).asInt();
        if (rl.isMember("whitelist"))
            for (auto& ip : rl["whitelist"])
                cfg.whitelist.push_back(ip.asString());
        ::RateLimiter::instance().configure(cfg);
        return 1;
    }

    static int applyDatabase(const Json::Value& root) {
        if (!root.isMember("database")) return 0;
        auto& d = root["database"];
        DatabaseService::instance().setSlowQueryThreshold(
            d.get("slow_query_warn_ms", 200).asInt(),
            d.get("slow_query_err_ms", 1000).asInt());
        return 1;
    }

    static int applySlowLog(const Json::Value& root) {
        if (!root.isMember("database") ||
            !root["database"].isMember("slow_log")) return 0;
        auto& sl = root["database"]["slow_log"];
        SlowLogQueue::Config sc;
        sc.enabled       = sl.get("enabled", true).asBool();
        sc.alertMs       = sl.get("alert_ms", 2000).asInt64();
        sc.queueCapacity = sl.get("queue_capacity", 2000).asInt();
        SlowLogQueue::instance().init(sc);
        return 1;
    }

    static int applyBackup(const Json::Value& root) {
        if (!root.isMember("backup")) return 0;
        auto& bk = root["backup"];
        BackupService::Config bc;
        bc.enabled      = bk.get("enabled", false).asBool();
        bc.dir          = bk.get("dir", "backups").asString();
        bc.keepCount    = bk.get("keep_count", 7).asInt();
        bc.scheduleHour = bk.get("schedule_hour", 3).asInt();
        if (root.isMember("database")) {
            auto& d = root["database"];
            bc.dbHost = d.get("host", "127.0.0.1").asString();
            bc.dbPort = d.get("port", 5432).asInt();
            bc.dbName = d.get("dbname", "ruoyi").asString();
            bc.dbUser = d.get("user", "").asString();
            bc.dbPass = d.get("passwd", "").asString();
        }
        BackupService::instance().init(bc);
        return 1;
    }

    static int applyLogRetention(const Json::Value& root) {
        if (!root.isMember("log") ||
            !root["log"].isMember("retention")) return 0;
        auto& rt = root["log"]["retention"];
        LogRetention::Config lc;
        lc.enabled      = rt.get("enabled", true).asBool();
        lc.operLogDays  = rt.get("oper_log_days", 90).asInt();
        lc.loginLogDays = rt.get("login_log_days", 90).asInt();
        lc.localLogDays = rt.get("local_log_days", 30).asInt();
        lc.maxFiles     = rt.get("max_files", 5000).asInt();
        lc.maxTotalMb   = rt.get("max_total_mb", 2048).asInt64();
        lc.logDir       = rt.get("log_dir", "logs").asString();
        lc.archiveDir   = rt.get("archive_dir", "").asString();
        LogRetention::instance().init(lc);
        return 1;
    }

    static int applySms(const Json::Value& root) {
        if (!root.isMember("sms")) return 0;
        SmsService::instance().init(root["sms"]);
        return 1;
    }

    static int applyMqtt(const Json::Value& root) {
        if (!root.isMember("security") ||
            !root["security"].isMember("mqtt")) return 0;
        auto& mq = root["security"]["mqtt"];
        MqttClient::Config mc;
        mc.enabled   = mq.get("enabled", false).asBool();
        mc.host      = mq.get("host", "127.0.0.1").asString();
        mc.port      = mq.get("port", 1883).asInt();
        mc.username  = mq.get("username", "").asString();
        mc.password  = mq.get("password", "").asString();
        mc.clientId  = mq.get("client_id", "").asString();
        mc.keepalive = mq.get("keepalive", 60).asInt();
        mc.persist   = mq.get("persist", false).asBool();
        mc.forwardWs = mq.get("forward_ws", true).asBool();
        for (auto& t : mq["topics"]) mc.topics.push_back(t.asString());
        if (mc.topics.empty())
            mc.topics = {"device/+/status", "device/+/data"};
        MqttClient::instance().stop();   // 先停旧连接再按新配置重连
        MqttClient::instance().init(mc);
        return 1;
    }

    static int applyCaptcha(const Json::Value& root) {
        if (!root.isMember("captcha") ||
            !root["captcha"].isMember("behavioral")) return 0;
        auto& bh = root["captcha"]["behavioral"];
        CaptchaClient::Config cc;
        cc.enabled    = bh.get("enabled", false).asBool();
        cc.serverAddr = bh.get("server_addr", "127.0.0.1:18090").asString();
        cc.timeoutMs  = bh.get("timeout_ms", 3000).asInt();
        cc.type       = bh.get("type", "slide").asString();
        CaptchaClient::instance().init(cc);   // 幂等：重建 channel/stub
        return 1;
    }

    static int applyLogManticore(const Json::Value& root) {
        if (!root.isMember("log") || !root["log"].isMember("manticore")) return 0;
        auto& mc = root["log"]["manticore"];
        LogIndexer::Config li;
        li.enabled         = mc.get("enabled", false).asBool();
        li.endpoint        = mc.get("endpoint", "").asString();
        li.index           = mc.get("index", "sys_logs").asString();
        li.logDir          = mc.get("log_dir", "./logs").asString();
        li.batchSize       = mc.get("batch_size", 500).asInt();
        li.maxLinesPerTick = mc.get("max_lines_per_tick", 20000).asInt();
        if (li.endpoint.empty() && root.isMember("security") &&
            root["security"].isMember("audit"))
            li.endpoint = root["security"]["audit"]
                          .get("endpoint", "http://127.0.0.1:7700").asString();
        if (li.endpoint.empty()) li.endpoint = "http://127.0.0.1:7700";
        LogIndexer::instance().configure(li);
        return 1;
    }

#ifdef __linux__
    static int applyWaf(const Json::Value& root) {
        if (!root.isMember("security") ||
            !root["security"].isMember("waf")) return 0;
        WafEngine::instance().init(root["security"]["waf"]);
        return 1;
    }
    static int applyAudit(const Json::Value& root) {
        if (!root.isMember("security") ||
            !root["security"].isMember("audit")) return 0;
        auto& au = root["security"]["audit"];
        AuditQueue::Config aCfg;
        aCfg.enabled         = au.get("enabled", false).asBool();
        aCfg.endpoint        = au.get("endpoint", "http://127.0.0.1:7700").asString();
        aCfg.index           = au.get("index", "waf_logs").asString();
        aCfg.batchSize       = au.get("batch_size", 100).asInt();
        aCfg.flushIntervalMs = au.get("flush_interval_ms", 3000).asInt();
        aCfg.retentionDays   = au.get("retention_days", 30).asInt();
        aCfg.queueCapacity   = au.get("queue_capacity", 10000).asInt();
        AuditQueue::instance().init(aCfg);
        return 1;
    }
    static int applySso(const Json::Value& root) {
        if (!root.isMember("security") ||
            !root["security"].isMember("sso")) return 0;
        SsoServer::instance().init(root["security"]["sso"]);
        return 1;
    }
#endif

    std::string path_ = "config.json";
    std::filesystem::file_time_type lastMtime_{};
    std::mutex mu_;
    std::vector<std::pair<std::string, std::function<void(const Json::Value&)>>> callbacks_;
};
