#pragma once
#include <drogon/HttpController.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <fstream>
#include <thread>
#include <iomanip>
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#else
#  include <unistd.h>
#  include <sys/resource.h>
#  include <sys/statvfs.h>
#endif
#include "../../common/AjaxResult.h"
#include "../../services/DatabaseService.h"
#include "../../common/TokenCache.h"
#include "../../services/NginxManager.h"
#include "../../services/KoboldCppManager.h"
#include "../../services/DdnsGoManager.h"
#include "../../services/WhisperService.h"

// GET /api/health - backend health checkpoint (matches druid page data)
class HealthCtrl : public drogon::HttpController<HealthCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(HealthCtrl::index, "/welcome", drogon::Get);
        // /api/health (URL-encoded for /api/健康)
        ADD_METHOD_TO(HealthCtrl::health, "/api/%E5%81%A5%E5%BA%B7", drogon::Get);
        // Druid-style monitoring page
        ADD_METHOD_TO(HealthCtrl::druid, "/api/druid", drogon::Get);
    METHOD_LIST_END

    // 首页欢迎页（匹配 RuoYi.Net HomeController.Index）
    void index(const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        static const std::string html = R"html(
<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>欢迎使用 RuoYi-Cpp 后台管理框架</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Helvetica Neue",Helvetica,"PingFang SC","Hiragino Sans GB","Microsoft YaHei","微软雅黑",Arial,sans-serif;background:#f0f2f5;color:#333}
.wrap{text-align:center;margin-top:80px}
.logo{height:80px;margin-bottom:24px}
h3{font-size:28px;font-weight:600;color:#222;margin-bottom:16px;letter-spacing:2px}
.desc{font-size:15px;color:#666;margin-bottom:8px;line-height:1.8}
.ver{font-size:13px;color:#999;margin-bottom:32px}
.links{display:flex;justify-content:center;gap:20px;flex-wrap:wrap}
a{color:#409eff;text-decoration:none;font-size:14px;padding:8px 20px;border:1px solid #409eff;border-radius:4px;transition:all .2s}
a:hover{background:#409eff;color:#fff}
</style>
</head>
<body>
<div class="wrap">
  <img class="logo" src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 120 120'%3E%3Ccircle cx='60' cy='60' r='56' fill='%23409EFF'/%3E%3Ctext x='60' y='75' text-anchor='middle' font-size='48' font-weight='bold' fill='white' font-family='sans-serif'%3ER%3C/text%3E%3C/svg%3E" alt="logo">
  <h3>RuoYi-Cpp</h3>
  <p class="desc">欢迎使用 RuoYi-Cpp 后台管理框架，当前版本：v1.0.0</p>
  <p class="ver">请通过前端地址访问</p>
  <div class="links">
    <a href="/api/health">健康检查</a>
    <a href="/api/druid">数据监控</a>
    <a href="/api/%E5%81%A5%E5%BA%B7">API JSON</a>
  </div>
</div>
</body>
</html>
)html";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(html);
        cb(resp);
    }

    // Druid-style monitoring page: full Druid look, RuoYi-Cpp branding
    void druid(const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& db = DatabaseService::instance();
        bool dbConn = db.isConnected();
        bool usingSql = db.isUsingSqlite();
        bool hasSql = db.hasSqlite();
        size_t pending = db.pendingCount();

        std::string pgVer, pgSize, pgActiveConn;
        if (dbConn && !usingSql) {
            auto r1 = db.query("SELECT version()");
            if (r1.rows() > 0) pgVer = r1.str(0, 0).substr(0, 60);
            auto r2 = db.query("SELECT pg_size_pretty(pg_database_size(current_database()))");
            if (r2.rows() > 0) pgSize = r2.str(0, 0);
            auto r3 = db.query("SELECT count(*) FROM pg_stat_activity WHERE state='active'");
            if (r3.rows() > 0) pgActiveConn = r3.str(0, 0);
        }

        std::string dbHost = "127.0.0.1", dbPort = "5432", dbName = "ruoyi.c";
        int nginxPort = 18081; bool nginxEnabled = false;
        std::string ddnsListen = ":39876"; bool ddnsEnabled = false;
        int aiPort = 5001; bool aiEnabled = false;
        {
            std::ifstream cf("config.json");
            if (cf.is_open()) {
                Json::Value root; Json::CharReaderBuilder rb; std::string e;
                if (Json::parseFromStream(rb, cf, &root, &e)) {
                    if (root.isMember("database")) {
                        dbHost = root["database"].get("host", "127.0.0.1").asString();
                        dbPort = std::to_string(root["database"].get("port", 5432).asInt());
                        dbName = root["database"].get("dbname", "ruoyi.c").asString();
                    }
                    if (root.isMember("nginx")) {
                        nginxEnabled = root["nginx"].get("enabled", false).asBool();
                        nginxPort = root["nginx"].get("port", 18081).asInt();
                    }
                    if (root.isMember("ddns")) {
                        ddnsEnabled = root["ddns"].get("enabled", false).asBool();
                        ddnsListen = root["ddns"].get("listen", ":39876").asString();
                    }
                    if (root.isMember("koboldcpp")) {
                        aiEnabled = root["koboldcpp"].get("enabled", false).asBool();
                        aiPort = root["koboldcpp"].get("port", 5001).asInt();
                    }
                }
            }
        }
        {
            auto cfgVal = [&](const std::string& key) -> std::string {
                auto r = db.queryParams(
                    "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1", {key});
                return (r.rows() > 0) ? r.str(0, 0) : "";
            };
            auto asBool = [](const std::string& v) -> int {
                if (v.empty()) return -1;
                return (v == "true" || v == "1" || v == "TRUE") ? 1 : 0;
            };
            int vNginx = asBool(cfgVal("sys.subprocess.nginx"));
            int vAi    = asBool(cfgVal("sys.subprocess.koboldcpp"));
            int vDdns  = asBool(cfgVal("sys.subprocess.ddns"));
            if (vNginx >= 0) nginxEnabled = (vNginx == 1);
            if (vAi    >= 0) aiEnabled    = (vAi    == 1);
            if (vDdns  >= 0) ddnsEnabled  = (vDdns  == 1);
        }

        bool nginxOk = NginxManager::instance().isRunning();
        bool aiOk   = KoboldCppManager::instance().isRunning();
        bool ddnsOk = DdnsGoManager::instance().isRunning();

        std::string memTotal = "N/A", memUsed = "N/A", memUsage = "N/A";
#ifdef _WIN32
        {
            MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
            if (GlobalMemoryStatusEx(&ms)) {
                auto toGB = [](DWORDLONG b) {
                    std::ostringstream s; s << std::fixed << std::setprecision(1)
                        << (double)b / 1073741824.0 << " G"; return s.str();
                };
                memTotal = toGB(ms.ullTotalPhys);
                memUsed  = toGB(ms.ullTotalPhys - ms.ullAvailPhys);
                std::ostringstream u; u << std::fixed << std::setprecision(0)
                    << (double)(ms.ullTotalPhys - ms.ullAvailPhys) / ms.ullTotalPhys * 100 << "%";
                memUsage = u.str();
            }
        }
#else
        {
            struct sysinfo si; ::sysinfo(&si);
            auto toGB = [&](unsigned long long b) {
                std::ostringstream s; s << std::fixed << std::setprecision(1)
                    << (double)b / 1073741824.0 << " G"; return s.str();
            };
            memTotal = toGB(si.totalram);
            memUsed  = toGB(si.totalram - si.freeram);
            std::ostringstream u; u << std::fixed << std::setprecision(0)
                << (double)(si.totalram - si.freeram) / si.totalram * 100 << "%";
            memUsage = u.str();
        }
#endif

        auto cpuUsage = []() -> std::string {
#ifdef _WIN32
            static FILETIME prevIdle{}, prevKernel{}, prevUser{};
            FILETIME idle{}, kernel{}, user{};
            if (!GetSystemTimes(&idle, &kernel, &user)) return "N/A";
            auto ftDiff = [](FILETIME a, FILETIME b) -> unsigned long long {
                ULARGE_INTEGER ia{}, ib{};
                ia.HighPart = a.dwHighDateTime; ia.LowPart = a.dwLowDateTime;
                ib.HighPart = b.dwHighDateTime; ib.LowPart = b.dwLowDateTime;
                return ia.QuadPart - ib.QuadPart;
            };
            unsigned long long idleD = ftDiff(idle, prevIdle);
            unsigned long long kernD = ftDiff(kernel, prevKernel);
            unsigned long long userD = ftDiff(user, prevUser);
            if (kernD + userD == 0) return "N/A";
            double pct = 100.0 * (kernD + userD - idleD) / (kernD + userD);
            prevIdle = idle; prevKernel = kernel; prevUser = user;
            std::ostringstream s; s << std::fixed << std::setprecision(1) << pct << "%";
            return s.str();
#else
            double load[1]; if (getloadavg(load, 1) > 0) {
                std::ostringstream s; s << std::fixed << std::setprecision(2) << load[0];
                return s.str();
            }
            return "N/A";
#endif
        };

        auto diskInfo = []() -> std::string {
#ifdef _WIN32
            ULARGE_INTEGER freeC, totalC, totalF;
            if (GetDiskFreeSpaceExW(L"C:\\", &freeC, &totalC, &totalF)) {
                auto toGB = [](ULARGE_INTEGER b) {
                    return std::to_string((unsigned long long)(b.QuadPart / 1073741824ULL));
                };
                return "C: free " + toGB(freeC) + " G / total " + toGB(totalC) + " G";
            }
            return "N/A";
#else
            struct statvfs sv; ::statvfs("/", &sv);
            auto toGB = [&](unsigned long long b) {
                return std::to_string((unsigned long long)(b * sv.f_frsize / 1073741824ULL));
            };
            return "free " + toGB(sv.f_bavail) + " G / total " + toGB(sv.f_blocks) + " G";
#endif
        };

        std::string uptime = []() {
            std::ostringstream s;
            auto now = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - s_startTime_).count();
            long long ms = elapsed % 1000; elapsed /= 1000;
            long long s_ = elapsed % 60;   elapsed /= 60;
            long long m  = elapsed % 60;    elapsed /= 60;
            long long h  = elapsed % 24;    elapsed /= 24;
            long long d  = elapsed;
            if (d > 0) s << d << "d ";
            s << h << "h " << m << "m " << s_ << "s";
            return s.str();
        }();

        // Build disk table rows
        std::string diskTableRows;
        {
            std::ostringstream dh;
#ifdef _WIN32
            for (char drive = 'C'; drive <= 'Z'; ++drive) {
                ULARGE_INTEGER freeC, totalC, totalF;
                std::wstring path = std::wstring(1, drive) + L":\\";
                if (GetDiskFreeSpaceExW(path.c_str(), &freeC, &totalC, &totalF)) {
                    unsigned long long totalB = totalC.QuadPart;
                    unsigned long long freeB  = freeC.QuadPart;
                    unsigned long long usedB   = totalB - freeB;
                    int usedPct = totalB > 0 ? (int)(100.0 * usedB / totalB) : 0;
                    auto gb = [&](unsigned long long b) {
                        std::ostringstream s; s << std::fixed << std::setprecision(1) << b / 1073741824.0 << " G";
                        return s.str();
                    };
                    dh << "<tr>"
                       << "<td class=\"el-table__cell is-leaf\"><div class=\"cell\">" << drive << ":\\</div></td>"
                       << "<td class=\"el-table__cell is-leaf\"><div class=\"cell\">NTFS</div></td>"
                       << "<td class=\"el-table__cell is-leaf\"><div class=\"cell\">-</div></td>"
                       << "<td class=\"el-table__cell is-leaf\"><div class=\"cell\">" << gb(totalB) << "</div></td>"
                       << "<td class=\"el-table__cell is-leaf\"><div class=\"cell\">" << gb(freeB) << "</div></td>"
                       << "<td class=\"el-table__cell is-leaf\"><div class=\"cell\">" << gb(usedB) << "</div></td>"
                       << "<td class=\"el-table__cell is-leaf\"><div class=\"cell"
                       << (usedPct > 80 ? " text-danger" : "") << "\">" << usedPct << "%</div></td>"
                       << "</tr>";
                }
            }
#endif
            diskTableRows = dh.str();
        }

        std::string statusBadge = dbConn
            ? "<span class=\"el-tag el-tag--success\">正常</span>"
            : "<span class=\"el-tag el-tag--danger\">异常</span>";
        std::string dbBadge = dbConn
            ? "<span class=\"el-tag el-tag--success\">Active</span>"
            : "<span class=\"el-tag el-tag--danger\">Inactive</span>";

        // CPU usage
        std::string cpuUsedPct = cpuUsage();
        int cpuPct = 0;
        try { cpuPct = std::stoi(cpuUsedPct); } catch (...) {}
        std::string cpuFreePct = std::to_string(100 - cpuPct) + "%";

        // Mem strings
        std::string memTotalVal = memTotal;
        std::string memUsedVal  = memUsed;
        std::string memUsageVal = memUsage;

        // Parse memory GB values
        auto parseGb = [&](const std::string& s) -> double {
            try {
                std::string v = s;
                size_t sp = v.find(' ');
                if (sp != std::string::npos) v = v.substr(0, sp);
                return std::stod(v);
            } catch (...) { return 0.0; }
        };
        double totalGb = parseGb(memTotalVal);
        double usedGb  = parseGb(memUsedVal);
        double freeGb  = std::max(0.0, totalGb - usedGb);
        std::ostringstream mfs;
        mfs << std::fixed << std::setprecision(1) << freeGb << " G";
        std::string memFreeVal = mfs.str();

        // OS name
        std::string osName =
#ifdef _WIN32
            "Windows (x86_64)";
#else
            "Linux (x86_64)";
#endif

        std::string css = R"css(
<link rel="stylesheet" href="https://unpkg.com/element-ui@2.15.14/lib/theme-chalk/index.css">
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:"Helvetica Neue",Helvetica,"PingFang SC","Hiragino Sans GB","Microsoft YaHei","微软雅黑",Arial,sans-serif;background:#f0f2f5;color:#333}
.app-container{padding:20px;max-width:1400px;margin:0 auto}
.el-card{background:#fff;border-radius:4px;box-shadow:0 2px 12px rgba(0,0,0,.06);margin-bottom:16px;border:1px solid #ebeef5;overflow:hidden}
.el-card__header{padding:14px 20px;border-bottom:1px solid #ebeef5;font-size:14px;font-weight:700;color:#333;background:#fff}
.el-table{width:100%;border-collapse:collapse;table-layout:fixed}
.el-table td{padding:8px 0;border-bottom:1px solid #ebeef5;font-size:13px}
.el-table td:first-child{width:120px;color:#606266;font-weight:600;background:#fafafa;padding-right:12px;text-align:right}
.el-table td:nth-child(2){padding-left:12px}
.el-table td:nth-child(3){width:120px;color:#606266;font-weight:600;background:#fafafa;padding-right:12px;text-align:right}
.el-table td:nth-child(4){padding-left:12px}
.el-table thead th{background:#f5f7fa;color:#909399;font-size:12px;text-align:left;padding:8px 0}
.el-table thead th:nth-child(1){text-align:left}
.el-table thead th:nth-child(2){text-align:left}
.text-danger{color:#f56c6c !important;font-weight:700}
.footer{text-align:center;font-size:12px;color:#999;padding:24px 0}
.footer a{color:#409eff;text-decoration:none}
.footer a:hover{text-decoration:underline}
</style>
)css";

        std::string html = R"html(
<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RuoYi-Cpp 服务器监控</title>
)html" + css + R"html(
</head>
<body>
<div class="app-container">

  <div style="display:flex;gap:16px;margin-bottom:16px;flex-wrap:wrap">
    <div style="flex:1;min-width:340px">
      <div class="el-card">
        <div class="el-card__header">&#9679; CPU</div>
        <div class="el-table el-table--enable-row-hover el-table--medium">
          <table style="width:100%">
            <thead><tr>
              <th class="el-table__cell is-leaf"><div class="cell">属性</div></th>
              <th class="el-table__cell is-leaf"><div class="cell">值</div></th>
            </tr></thead>
            <tbody>
              <tr><td class="el-table__cell is-leaf"><div class="cell">核心数</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">)html" + std::to_string(std::thread::hardware_concurrency()) + " 核" + R"html(</div></td></tr>
              <tr><td class="el-table__cell is-leaf"><div class="cell">使用率</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">)html" + cpuUsedPct + R"html(</div></td></tr>
              <tr><td class="el-table__cell is-leaf"><div class="cell">系统使用率</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">)html" + cpuUsedPct + R"html(</div></td></tr>
              <tr><td class="el-table__cell is-leaf"><div class="cell">当前空闲率</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">)html" + cpuFreePct + R"html(</div></td></tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <div style="flex:1;min-width:340px">
      <div class="el-card">
        <div class="el-card__header">&#9679; 内存</div>
        <div class="el-table el-table--enable-row-hover el-table--medium">
          <table style="width:100%">
            <thead><tr>
              <th class="el-table__cell is-leaf"><div class="cell">属性</div></th>
              <th class="el-table__cell is-leaf"><div class="cell">内存</div></th>
              <th class="el-table__cell is-leaf"><div class="cell">.NET CLR</div></th>
            </tr></thead>
            <tbody>
              <tr><td class="el-table__cell is-leaf"><div class="cell">总内存</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">)html" + memTotalVal + R"html(</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">-</div></td></tr>
              <tr><td class="el-table__cell is-leaf"><div class="cell">已用内存</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">)html" + memUsedVal + R"html(</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">-</div></td></tr>
              <tr><td class="el-table__cell is-leaf"><div class="cell">剩余内存</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">)html" + memFreeVal + R"html(</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">-</div></td></tr>
              <tr><td class="el-table__cell is-leaf"><div class="cell">使用率</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell)html" + (cpuPct > 80 ? " text-danger" : "") + R"html(">)html" + memUsageVal + R"html(</div></td>
                  <td class="el-table__cell is-leaf"><div class="cell">-</div></td></tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>
  </div>

  <div class="el-card">
    <div class="el-card__header">&#9679; 服务器信息</div>
    <div class="el-table el-table--enable-row-hover el-table--medium">
      <table style="width:100%">
        <tbody>
          <tr><td class="el-table__cell is-leaf"><div class="cell">服务器名称</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">)html" + dbHost + R"html(</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">操作系统</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">)html" + osName + R"html(</div></td></tr>
          <tr><td class="el-table__cell is-leaf"><div class="cell">服务器IP</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">)html" + dbHost + ":" + dbPort + R"html(</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">系统架构</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">x86_64</div></td></tr>
        </tbody>
      </table>
    </div>
  </div>

  <div class="el-card">
    <div class="el-card__header">&#9679; 数据库监控</div>
    <div class="el-table el-table--enable-row-hover el-table--medium">
      <table style="width:100%">
        <tbody>
          <tr><td class="el-table__cell is-leaf"><div class="cell">数据源</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">)html" + dbHost + ":" + dbPort + "/" + dbName + R"html(</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">连接状态</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">)html" + statusBadge + R"html(</div></td></tr>
          <tr><td class="el-table__cell is-leaf"><div class="cell">数据库类型</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">)html" + (usingSql ? "SQLite3" : "PostgreSQL") + R"html(</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">连接池</div></td>
              <td class="el-table__cell is-leaf"><div class="cell">)html" + dbBadge + R"html(</div></td></tr>
)html"
          + (pgVer.empty()       ? "" : "<tr><td class=\"el-table__cell is-leaf\"><div class=\"cell\">数据库版本</div></td><td class=\"el-table__cell is-leaf\"><div class=\"cell\">" + pgVer + "</div></td><td></td><td></td></tr>")
          + (pgSize.empty()     ? "" : "<tr><td class=\"el-table__cell is-leaf\"><div class=\"cell\">数据库大小</div></td><td class=\"el-table__cell is-leaf\"><div class=\"cell\">" + pgSize + "</div></td><td></td><td></td></tr>")
          + (pgActiveConn.empty() ? "" : "<tr><td class=\"el-table__cell is-leaf\"><div class=\"cell\">活跃连接数</div></td><td class=\"el-table__cell is-leaf\"><div class=\"cell\">" + pgActiveConn + "</div></td><td></td><td></td></tr>")
          + R"html(
        </tbody>
      </table>
    </div>
  </div>

  <div class="el-card">
    <div class="el-card__header">&#9679; 磁盘状态</div>
    <div class="el-table el-table--enable-row-hover el-table--medium">
      <table style="width:100%">
        <thead><tr>
          <th class="el-table__cell is-leaf"><div class="cell">盘符路径</div></th>
          <th class="el-table__cell is-leaf"><div class="cell">文件系统</div></th>
          <th class="el-table__cell is-leaf"><div class="cell">盘符类型</div></th>
          <th class="el-table__cell is-leaf"><div class="cell">总大小</div></th>
          <th class="el-table__cell is-leaf"><div class="cell">可用大小</div></th>
          <th class="el-table__cell is-leaf"><div class="cell">已用大小</div></th>
          <th class="el-table__cell is-leaf"><div class="cell">已用百分比</div></th>
        </tr></thead>
        <tbody>
)html" + diskTableRows + R"html(
        </tbody>
      </table>
    </div>
  </div>

  <div class="footer">
    Powered by RuoYi-Cpp &middot; <a href="/api/%E5%81%A5%E5%BA%B7">API JSON</a>
  </div>

</div>
</body>
</html>
)html";

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(html);
        cb(resp);
    }

    void health(const drogon::HttpRequestPtr&,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& db = DatabaseService::instance();
        bool dbConn = db.isConnected();
        bool usingSql = db.isUsingSqlite();
        bool hasSql = db.hasSqlite();
        size_t pending = db.pendingCount();

        std::string pgVer, pgSize, pgActiveConn;
        if (dbConn && !usingSql) {
            auto r1 = db.query("SELECT version()");
            if (r1.rows() > 0) pgVer = r1.str(0, 0).substr(0, 60);
            auto r2 = db.query("SELECT pg_size_pretty(pg_database_size(current_database()))");
            if (r2.rows() > 0) pgSize = r2.str(0, 0);
            auto r3 = db.query("SELECT count(*) FROM pg_stat_activity WHERE state='active'");
            if (r3.rows() > 0) pgActiveConn = r3.str(0, 0);
        }

        // read config.json
        std::string dbHost = "127.0.0.1", dbPort = "5432", dbName = "ruoyi.c";
        int nginxPort = 18081; bool nginxEnabled = false;
        std::string ddnsListen = ":39876"; bool ddnsEnabled = false;
        int aiPort = 5001; bool aiEnabled = false;
        {
            std::ifstream cf("config.json");
            if (cf.is_open()) {
                Json::Value root; Json::CharReaderBuilder rb; std::string e;
                if (Json::parseFromStream(rb, cf, &root, &e)) {
                    if (root.isMember("database")) {
                        dbHost = root["database"].get("host", "127.0.0.1").asString();
                        dbPort = std::to_string(root["database"].get("port", 5432).asInt());
                        dbName = root["database"].get("dbname", "ruoyi.c").asString();
                    }
                    if (root.isMember("nginx")) {
                        nginxEnabled = root["nginx"].get("enabled", false).asBool();
                        nginxPort = root["nginx"].get("port", 18081).asInt();
                    }
                    if (root.isMember("ddns")) {
                        ddnsEnabled = root["ddns"].get("enabled", false).asBool();
                        ddnsListen = root["ddns"].get("listen", ":39876").asString();
                    }
                    if (root.isMember("koboldcpp")) {
                        aiEnabled = root["koboldcpp"].get("enabled", false).asBool();
                        aiPort = root["koboldcpp"].get("port", 5001).asInt();
                    }
                }
            }
        }

        // sys_config overrides
        {
            auto cfgVal = [&](const std::string& key) -> std::string {
                auto r = db.queryParams(
                    "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1", {key});
                return (r.rows() > 0) ? r.str(0, 0) : "";
            };
            auto asBool = [](const std::string& v) -> int {
                if (v.empty()) return -1;
                return (v == "true" || v == "1" || v == "TRUE") ? 1 : 0;
            };
            int vNginx = asBool(cfgVal("sys.subprocess.nginx"));
            int vAi   = asBool(cfgVal("sys.subprocess.koboldcpp"));
            int vDdns = asBool(cfgVal("sys.subprocess.ddns"));
            if (vNginx >= 0) nginxEnabled = (vNginx == 1);
            if (vAi    >= 0) aiEnabled    = (vAi    == 1);
            if (vDdns  >= 0) ddnsEnabled  = (vDdns  == 1);
        }

        // sub-process status
        bool nginxOk   = NginxManager::instance().isRunning();
        bool aiOk      = KoboldCppManager::instance().isRunning();
        bool ddnsOk    = DdnsGoManager::instance().isRunning();
        bool whisperOk = WhisperService::instance().isReady();

        // memory
        std::string memTotal = "N/A", memUsed = "N/A", memFree = "N/A", memUsage = "N/A";
#ifdef _WIN32
        {
            MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
            if (GlobalMemoryStatusEx(&ms)) {
                auto toGB = [](DWORDLONG b) {
                    std::ostringstream s; s << std::fixed << std::setprecision(1)
                        << (double)b / 1073741824.0 << " G"; return s.str();
                };
                memTotal = toGB(ms.ullTotalPhys);
                memFree  = toGB(ms.ullAvailPhys);
                memUsed  = toGB(ms.ullTotalPhys - ms.ullAvailPhys);
                std::ostringstream u; u << std::fixed << std::setprecision(1)
                    << 100.0 * (ms.ullTotalPhys - ms.ullAvailPhys) / ms.ullTotalPhys << "%";
                memUsage = u.str();
            }
        }
#else
        {
            std::ifstream mf("/proc/meminfo");
            std::string k, unit; long long v, total = 0, avail = 0;
            while (mf >> k >> v >> unit) {
                if (k == "MemTotal:")     total = v;
                if (k == "MemAvailable:") avail = v;
            }
            if (total > 0) {
                auto kb2G = [](long long kb) {
                    std::ostringstream s; s << std::fixed << std::setprecision(1)
                        << kb / 1048576.0 << " G"; return s.str();
                };
                memTotal = kb2G(total); memFree = kb2G(avail);
                memUsed  = kb2G(total - avail);
                std::ostringstream u; u << std::fixed << std::setprecision(1)
                    << 100.0 * (total - avail) / total << "%";
                memUsage = u.str();
            }
        }
#endif

        // disk
        std::string diskTotal = "N/A", diskUsed = "N/A", diskFree = "N/A", diskUsage = "N/A";
#ifdef _WIN32
        {
            ULARGE_INTEGER tot{}, fr{};
            if (GetDiskFreeSpaceExA(".", nullptr, &tot, &fr)) {
                auto toGB = [](ULONGLONG b) {
                    std::ostringstream s; s << std::fixed << std::setprecision(1)
                        << (double)b / 1073741824.0 << " G"; return s.str();
                };
                diskTotal = toGB(tot.QuadPart); diskFree = toGB(fr.QuadPart);
                diskUsed  = toGB(tot.QuadPart - fr.QuadPart);
                std::ostringstream u; u << std::fixed << std::setprecision(1)
                    << 100.0 * (tot.QuadPart - fr.QuadPart) / tot.QuadPart << "%";
                diskUsage = u.str();
            }
        }
#else
        {
            struct statvfs st;
            if (statvfs(".", &st) == 0) {
                auto toGB = [](unsigned long long b) {
                    std::ostringstream s; s << std::fixed << std::setprecision(1)
                        << b / 1073741824.0 << " G"; return s.str();
                };
                unsigned long long tot = (unsigned long long)st.f_blocks * st.f_frsize;
                unsigned long long fr  = (unsigned long long)st.f_bavail * st.f_frsize;
                diskTotal = toGB(tot); diskFree = toGB(fr); diskUsed = toGB(tot - fr);
                std::ostringstream u; u << std::fixed << std::setprecision(1)
                    << 100.0 * (tot - fr) / tot << "%";
                diskUsage = u.str();
            }
        }
#endif

        // process mem
        std::string procMem = "N/A";
#ifdef _WIN32
        {
            PROCESS_MEMORY_COUNTERS pmc{};
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                std::ostringstream s; s << std::fixed << std::setprecision(1)
                    << pmc.WorkingSetSize / 1048576.0 << " MB";
                procMem = s.str();
            }
        }
#else
        {
            struct rusage ru{};
            if (getrusage(RUSAGE_SELF, &ru) == 0) {
                std::ostringstream s; s << std::fixed << std::setprecision(1)
                    << ru.ru_maxrss / 1024.0 << " MB";
                procMem = s.str();
            }
        }
#endif

        // uptime
        auto upSec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - s_startTime_).count();
        long d = upSec / 86400, h = (upSec % 86400) / 3600, m = (upSec % 3600) / 60, s2 = upSec % 60;
        std::ostringstream upFmt; if (d > 0) upFmt << d << "天"; if (h > 0) upFmt << h << "小时";
        if (m > 0) upFmt << m << "分钟"; upFmt << s2 << "秒";
        std::string startTimeStr;
        { auto t = std::chrono::system_clock::to_time_t(s_startTime_); char sb[32];
#ifdef _WIN32
          std::strftime(sb, sizeof(sb), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
#else
          std::strftime(sb, sizeof(sb), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
#endif
          startTimeStr = sb; }

        // server time
        auto now = std::chrono::system_clock::now();
        std::time_t nowt = std::chrono::system_clock::to_time_t(now);
        char timeBuf[32];
#ifdef _WIN32
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&nowt));
#else
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&nowt));
#endif

        // cpu cores
        int cpuCores = (int)std::thread::hardware_concurrency();

        // hostname / os
        std::string hostname = "localhost", osLabel = "Unknown";
#ifdef _WIN32
        osLabel = "Windows";
        { char hb[256] = {}; DWORD sz = sizeof(hb); if (GetComputerNameA(hb, &sz)) hostname = hb; }
#else
        osLabel = "Linux";
        { char hb[256] = {}; if (gethostname(hb, sizeof(hb)) == 0) hostname = hb; }
#endif

        // today stats
        std::string todayOps = "0", todayLogins = "0";
        size_t onlineUsers = TokenCache::instance().size();
        if (dbConn) {
            auto ro = db.query("SELECT COUNT(*) FROM sys_oper_log WHERE oper_time >= CURRENT_DATE");
            if (ro.ok() && ro.rows() > 0) todayOps = ro.str(0, 0);
            auto rl = db.query("SELECT COUNT(*) FROM sys_logininfor WHERE login_time >= CURRENT_DATE");
            if (rl.ok() && rl.rows() > 0) todayLogins = rl.str(0, 0);
        }

        // === build response (matches druid page exactly) ===
        Json::Value data;

        // top cards
        Json::Value onlineUsers_; onlineUsers_["count"] = (Json::Int)onlineUsers;
        onlineUsers_["hint"] = "活跃 Token";
        data["onlineUsers"] = onlineUsers_;

        Json::Value todayLogin_; todayLogin_["count"] = todayLogins; todayLogin_["unit"] = "次";
        data["todayLogins"] = todayLogin_;

        Json::Value todayOper_; todayOper_["count"] = todayOps; todayOper_["unit"] = "条操作日志";
        data["todayOperations"] = todayOper_;

        Json::Value uptime_; uptime_["text"] = upFmt.str(); uptime_["startTime"] = startTimeStr;
        data["uptime"] = uptime_;

        Json::Value dbStat_; dbStat_["connected"] = dbConn;
        dbStat_["backend"] = dbConn ? (usingSql ? "SQLite" : "PostgreSQL") : "断开";
        dbStat_["size"] = pgSize.empty() ? (dbConn ? "已连接" : "不可用") : pgSize;
        dbStat_["pending"] = (Json::Int)pending;
        data["database"] = dbStat_;

        // main sections
        Json::Value dbSection;
        dbSection["backend"]    = dbConn ? (usingSql ? "SQLite 回退" : "PostgreSQL") : "断开";
        dbSection["pgAddr"]   = dbHost + ":" + dbPort + " / " + dbName;
        dbSection["sqlite"]    = hasSql ? "就绪" : "未连接";
        dbSection["pgVersion"] = pgVer;
        dbSection["pgSize"]    = pgSize;
        dbSection["pgConn"]    = pgActiveConn;
        dbSection["connected"] = dbConn;
        data["dbSection"] = dbSection;

        Json::Value cacheSection;
        cacheSection["memCache"] = "就绪";
        cacheSection["backend"]  = "MemCache";
        data["cacheSection"] = cacheSection;

        Json::Value procSection;
        Json::Value nginx; nginx["running"] = nginxOk; nginx["enabled"] = nginxEnabled;
        nginx["port"] = ":" + std::to_string(nginxPort);
        procSection["nginx"] = nginx;
        Json::Value ai; ai["running"] = aiOk; ai["enabled"] = aiEnabled;
        ai["port"] = ":" + std::to_string(aiPort);
        procSection["ai"] = ai;
        Json::Value ddns; ddns["running"] = ddnsOk; ddns["enabled"] = ddnsEnabled;
        ddns["port"] = ddnsListen;
        procSection["ddns"] = ddns;
        Json::Value whisper; whisper["running"] = whisperOk; whisper["enabled"] = aiEnabled;
        whisper["port"] = "";
        procSection["whisper"] = whisper;
        data["procSection"] = procSection;

        Json::Value sysSection;
        sysSection["memUsage"]    = memUsage; sysSection["memUsed"] = memUsed;
        sysSection["memTotal"]    = memTotal;
        sysSection["diskUsage"]   = diskUsage; sysSection["diskUsed"] = diskUsed;
        sysSection["diskTotal"]   = diskTotal;
        sysSection["procMem"]     = procMem;
        sysSection["cpuCores"]    = cpuCores;
        sysSection["osLabel"]     = osLabel;
        sysSection["hostname"]    = hostname;
        data["sysSection"] = sysSection;

        // meta
        data["timestamp"] = std::string(timeBuf);
        data["serverTime"] = std::string(timeBuf);
        data["uptimeSec"] = (Json::Int64)upSec;

        RESP_OK(cb, data);
    }

private:
    static std::chrono::system_clock::time_point s_startTime_;
};

inline std::chrono::system_clock::time_point HealthCtrl::s_startTime_ = std::chrono::system_clock::now();
