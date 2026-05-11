#pragma once
/**
 * SysMonitorConfigCtrl —— 服务器监控告警配置
 *
 * 5 个键持久化在 sys_config（与 RuoYi 风格一致）：
 *   sys.monitor.enabled        ("0"|"1")  默认 "1"
 *   sys.monitor.alertEmail     (string)   逗号分隔多收件人
 *   sys.monitor.memThreshold   (50~99)    默认 "90"（百分比）
 *   sys.monitor.diskThreshold  (50~99)    默认 "90"（百分比）
 *   sys.monitor.crashAlert     ("0"|"1")  默认 "1"
 *
 * 端点：
 *   GET  /system/monitorConfig         读取配置（key 不存在返回默认值）
 *   POST /system/monitorConfig         保存（一次写 5 个键）
 *   POST /system/monitorConfig/test    立即触发一次检测（同步采集 mem/disk 与阈值比对，命中则发邮件）
 */
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/OperLogUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../../common/SmtpUtils.h"
#include <json/json.h>
#include <string>
#include <cstdio>
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <sys/statvfs.h>
#endif

class SysMonitorConfigCtrl : public drogon::HttpController<SysMonitorConfigCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysMonitorConfigCtrl::get,  "/system/monitorConfig",      drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysMonitorConfigCtrl::save, "/system/monitorConfig",      drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(SysMonitorConfigCtrl::test, "/system/monitorConfig/test", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void get(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value j;
        j["enabled"]        = readKey("sys.monitor.enabled",       "1");
        j["alertEmail"]     = readKey("sys.monitor.alertEmail",    "");
        j["memThreshold"]   = readKey("sys.monitor.memThreshold",  "90");
        j["diskThreshold"]  = readKey("sys.monitor.diskThreshold", "90");
        j["crashAlert"]     = readKey("sys.monitor.crashAlert",    "1");
        RESP_OK(cb, j);
    }

    void save(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }

        std::string user = GET_USER_NAME(req);
        upsertKey("sys.monitor.enabled",       body->get("enabled",       "1").asString(), user);
        upsertKey("sys.monitor.alertEmail",    body->get("alertEmail",    "" ).asString(), user);
        upsertKey("sys.monitor.memThreshold",  body->get("memThreshold",  "90").asString(), user);
        upsertKey("sys.monitor.diskThreshold", body->get("diskThreshold", "90").asString(), user);
        upsertKey("sys.monitor.crashAlert",    body->get("crashAlert",    "1").asString(), user);

        LOG_OPER(req, "监控告警配置", BusinessType::UPDATE);
        RESP_MSG(cb, "保存成功");
    }

    void test(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:edit");

        // 1) 当前 enabled 状态
        if (readKey("sys.monitor.enabled", "1") != "1") {
            RESP_ERR(cb, "未启用监控，无法执行检测");
            return;
        }

        // 2) 采集 mem / disk 使用率
        double memPct  = sampleMemUsedPct();
        double diskPct = sampleDiskUsedPct();
        int memTh  = SecurityUtils::parseInt(readKey("sys.monitor.memThreshold",  "90"), 90);
        int diskTh = SecurityUtils::parseInt(readKey("sys.monitor.diskThreshold", "90"), 90);

        std::string alertEmail = readKey("sys.monitor.alertEmail", "");
        bool memHit  = (memPct  >= memTh);
        bool diskHit = (diskPct >= diskTh);

        // 3) 拼装一封邮件
        Json::Value detail;
        detail["memPct"]   = memPct;
        detail["memThr"]   = memTh;
        detail["memHit"]   = memHit;
        detail["diskPct"]  = diskPct;
        detail["diskThr"]  = diskTh;
        detail["diskHit"]  = diskHit;
        detail["msg"]      = (memHit || diskHit) ? "已触发告警" : "未达阈值（仅做发送链路检测）";

        if (!alertEmail.empty() && SmtpUtils::instance().isConfigured()) {
            std::string subject = "[RuoYi-Cpp 监控] 立即检测";
            char buf[1024];
            std::snprintf(buf, sizeof(buf),
                "服务器监控立即检测结果\n\n"
                "内存使用率: %.1f%% (阈值 %d%%) %s\n"
                "磁盘使用率: %.1f%% (阈值 %d%%) %s\n",
                memPct,  memTh,  memHit  ? "[已触发]" : "",
                diskPct, diskTh, diskHit ? "[已触发]" : "");
            // 多收件人：逗号或分号分隔
            std::string emails = alertEmail;
            for (auto& c : emails) if (c == ';') c = ',';
            size_t pos = 0;
            while (pos < emails.size()) {
                size_t comma = emails.find(',', pos);
                std::string to = emails.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                while (!to.empty() && (to.front() == ' ' || to.front() == '\t')) to.erase(to.begin());
                while (!to.empty() && (to.back()  == ' ' || to.back()  == '\t' || to.back() == '\r')) to.pop_back();
                if (!to.empty()) SmtpUtils::instance().send(to, subject, buf);
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
            detail["mailed"] = true;
        } else {
            detail["mailed"] = false;
            detail["mailWarn"] = "未配置告警收件人或 SMTP 发件人";
        }

        LOG_OPER(req, "监控告警立即检测", BusinessType::OTHER);
        // 即便阈值未触达也算成功——用 success(detail) 而不是 error，前端 catch 链不会触发
        RESP_OK(cb, detail);
    }

private:
    // ── sys_config 单键 upsert（与 SysConfigCtrl 风格一致）─────────────────
    static void upsertKey(const std::string& key, const std::string& val, const std::string& by) {
        auto& db = DatabaseService::instance();
        // PG ON CONFLICT 用 config_key 唯一索引；缺失时降级为 SELECT-then-INSERT/UPDATE
        auto res = db.queryParams(
            "SELECT config_id FROM sys_config WHERE config_key=$1", {key});
        if (res.ok() && res.rows() > 0) {
            db.execParams(
                "UPDATE sys_config SET config_value=$1, update_by=$2, update_time=NOW() WHERE config_key=$3",
                {val, by, key});
        } else {
            db.execParams(
                "INSERT INTO sys_config(config_name,config_key,config_value,config_type,create_by,create_time,remark) "
                "VALUES($1,$2,$3,'N',$4,NOW(),$5)",
                {key, key, val, by, std::string("自动写入：") + key});
        }
    }

    static std::string readKey(const std::string& key, const std::string& dflt) {
        auto& db = DatabaseService::instance();
        auto res = db.queryParams("SELECT config_value FROM sys_config WHERE config_key=$1", {key});
        if (res.ok() && res.rows() > 0) {
            std::string v = res.str(0, 0);
            return v.empty() ? dflt : v;
        }
        return dflt;
    }

    // ── 采样 ───────────────────────────────────────────────────────────────
    // 这里用最简单的跨平台实现；如已有 Sysinfo/PerfMonitor 工具可直接替换。
    static double sampleMemUsedPct() {
#ifdef _WIN32
        MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms)) return (double)ms.dwMemoryLoad;
#else
        FILE* fp = std::fopen("/proc/meminfo", "r");
        if (fp) {
            long total = 0, available = 0; char line[256];
            while (std::fgets(line, sizeof(line), fp)) {
                long v = 0;
                if (std::sscanf(line, "MemTotal: %ld kB",     &v) == 1) total = v;
                if (std::sscanf(line, "MemAvailable: %ld kB", &v) == 1) available = v;
            }
            std::fclose(fp);
            if (total > 0) return 100.0 * (total - available) / total;
        }
#endif
        return 0.0;
    }

    static double sampleDiskUsedPct() {
#ifdef _WIN32
        ULARGE_INTEGER freeBytes{}, totalBytes{}, totalFreeBytes{};
        if (GetDiskFreeSpaceExA("C:\\", &freeBytes, &totalBytes, &totalFreeBytes)) {
            if (totalBytes.QuadPart > 0)
                return 100.0 * (totalBytes.QuadPart - totalFreeBytes.QuadPart) / (double)totalBytes.QuadPart;
        }
#else
        struct statvfs s{};
        if (statvfs("/", &s) == 0 && s.f_blocks > 0)
            return 100.0 * (s.f_blocks - s.f_bavail) / (double)s.f_blocks;
#endif
        return 0.0;
    }
};
