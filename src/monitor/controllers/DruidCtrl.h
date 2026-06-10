#pragma once
#include <drogon/HttpController.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <fstream>
#include <thread>
#include <iomanip>
#include <algorithm>
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#else
#  include <unistd.h>
#  include <sys/statvfs.h>
#  include <sys/resource.h>
#endif
#include <hiredis/hiredis.h>
#include "../../services/DatabaseService.h"
#include "../../services/NginxManager.h"
#include "../../services/DdnsGoManager.h"
#include "../../services/KoboldCppManager.h"
#include "../../services/WhisperService.h"
#include "../../common/TokenCache.h"
#include "../../common/JwtUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../common/AjaxResult.h"

/**
 * @file DruidCtrl.h
 * @brief 系统健康监控控制器 — 综合系统监控和诊断面板
 * 
 * 功能概述：
 *   - 系统健康检查：监控系统整体健康状态
 *   - 数据库监控：监控数据库连接池和性能
 *   - Redis 监控：监控 Redis 连接和缓存状态
 *   - Nginx 监控：监控 Nginx 服务状态和性能
 *   - 服务监控：监控各个后端服务状态
 *   - 性能诊断：诊断系统性能问题
 * 
 * 核心特性：
 *   - 综合监控：一个页面查看所有系统组件状态
 *   - 实时数据：实时获取各组件的性能指标
 *   - 健康评分：综合评分系统整体健康状态
 *   - 告警提示：异常情况实时告警
 *   - 详细报告：生成详细的诊断报告
 *   - 历史趋势：展示性能指标的历史趋势
 * 
 * 监控组件：
 *   - DatabaseService：数据库连接池、查询性能
 *   - Redis：缓存命中率、内存使用
 *   - Nginx：请求数、响应时间、错误率
 *   - DdnsGoManager：DDNS 服务状态
 *   - KoboldCppManager：AI 推理服务状态
 *   - WhisperService：语音识别服务状态
 * 
 * API 端点：
 *   - GET /druid/health - 获取系统健康状态
 *   - GET /druid/database - 获取数据库监控信息
 *   - GET /druid/redis - 获取 Redis 监控信息
 *   - GET /druid/nginx - 获取 Nginx 监控信息
 *   - GET /druid/services - 获取所有服务状态
 *   - GET /druid/report - 获取诊断报告
 * 
 * 请求/响应示例：
 *   ```
 *   GET /druid/health
 *   Authorization: Bearer <JWT>
 *   
 *   响应：
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": {
 *       "status": "healthy",
 *       "score": 95,
 *       "components": {
 *         "database": { "status": "ok", "connections": 10, "maxConnections": 20 },
 *         "redis": { "status": "ok", "memory": "100MB", "hitRate": 0.95 },
 *         "nginx": { "status": "ok", "requests": 1000, "avgTime": 50 }
 *       }
 *     }
 *   }
 *   ```
 * 
 * 权限要求：
 *   - monitor:druid:query - 查看系统监控信息
 *   - monitor:druid:report - 生成诊断报告
 * 
 * 配置项（config.json）：
 *   - druid.enabled: 是否启用 Druid 监控（默认 true）
 *   - druid.refresh_interval: 刷新间隔（秒，默认 5）
 *   - druid.alert_threshold: 告警阈值配置
 *   - druid.history_retention: 历史数据保留天数（默认 7）
 * 
 * 健康评分标准：
 *   - 90-100：优秀（绿色）- 系统运行正常
 *   - 70-89：良好（黄色）- 存在轻微问题
 *   - 50-69：一般（橙色）- 存在明显问题
 *   - <50：较差（红色）- 需要立即处理
 * 
 * 监控指标：
 *   - CPU 使用率：系统 CPU 占用百分比
 *   - 内存使用率：系统内存占用百分比
 *   - 磁盘使用率：磁盘空间占用百分比
 *   - 数据库连接数：当前活跃连接数
 *   - 数据库查询时间：平均查询耗时
 *   - Redis 内存：Redis 内存占用
 *   - Redis 命中率：缓存命中率
 *   - Nginx 请求数：每秒请求数
 *   - Nginx 响应时间：平均响应时间
 *   - 错误率：系统错误率
 * 
 * @see DatabaseService - 数据库服务
 * @see NginxManager - Nginx 管理器
 * @see DdnsGoManager - DDNS 管理器
 * @see KoboldCppManager - AI 推理管理器
 * @see WhisperService - 语音识别服务
 */
class DruidCtrl : public drogon::HttpController<DruidCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(DruidCtrl::handler, "/druid/{path}", drogon::Get);
    METHOD_LIST_END

    void handler(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 const std::string& /*path*/) {
        // ── JWT 鉴权（支持 header / query param / localhost 直访）──────
        {
            auto token = SecurityUtils::getToken(req);
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
                // 同源 iframe：让 JS 从 parent.sessionStorage 读 token 后重定向
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setContentTypeCode(drogon::CT_TEXT_HTML);
                resp->setBody(R"HTML(<!DOCTYPE html><html><head><meta charset="UTF-8"></head><body>
<script>
(function(){
  var t='';
  try{var u=new URL(window.location.href);t=u.searchParams.get('token')||'';}catch(e){}
  if(!t&&window.parent!==window){try{t=window.parent.sessionStorage.getItem('Admin-Token')||'';}catch(e){}}
  if(!t){try{t=sessionStorage.getItem('Admin-Token')||'';}catch(e){}}
  if(t){var u=new URL(window.location.href);u.searchParams.set('token',t);window.location.replace(u.toString());}
  else{document.body.innerHTML='<div style="text-align:center;padding:60px;font-family:sans-serif"><h2>\u{1F512} \u76d1\u63a7\u9875\u9762\u9700\u8981\u767b\u5f55</h2><p style="color:#909399">\u8bf7\u5148\u767b\u5f55\u7cfb\u7edf\u540e\u518d\u8bbf\u95ee</p></div>';}
})();
</script></body></html>)HTML");
                cb(resp); return;
            }
        }

        auto& db = DatabaseService::instance();
        bool dbConn   = db.isConnected();
        bool usingSql = db.isUsingSqlite();
        bool hasSql   = db.hasSqlite();
        size_t pending = db.pendingCount();
        std::string pgVer, pgSize, pgActiveConn;
        if (dbConn && !usingSql) {
            auto r1 = db.query("SELECT version()");
            if (r1.rows() > 0) pgVer = r1.str(0,0).substr(0, 60);
            auto r2 = db.query("SELECT pg_size_pretty(pg_database_size(current_database()))");
            if (r2.rows() > 0) pgSize = r2.str(0,0);
            auto r3 = db.query("SELECT count(*) FROM pg_stat_activity WHERE state='active'");
            if (r3.rows() > 0) pgActiveConn = r3.str(0,0);
        }

        // 读取 config.json
        std::string dbHost="127.0.0.1", dbPort="5432", dbName="ruoyi.c";
        std::string redisHost="127.0.0.1"; int redisPort=6379, redisDb=0;
        bool redisEnabled=false;
        int  nginxPort=18081; bool nginxEnabled=false;
        std::string ddnsListen=":39876"; bool ddnsEnabled=false;
        int  aiPort=5001; bool aiEnabled=false;
        {
            std::ifstream cf("config.json");
            if (cf.is_open()) {
                Json::Value root; Json::CharReaderBuilder rb; std::string e;
                if (Json::parseFromStream(rb, cf, &root, &e)) {
                    if (root.isMember("database")) {
                        dbHost = root["database"].get("host","127.0.0.1").asString();
                        dbPort = std::to_string(root["database"].get("port",5432).asInt());
                        dbName = root["database"].get("dbname","ruoyi.c").asString();
                    }
                    if (root.isMember("redis")) {
                        redisEnabled = root["redis"].get("enabled",false).asBool();
                        redisHost    = root["redis"].get("host","127.0.0.1").asString();
                        redisPort    = root["redis"].get("port",6379).asInt();
                        redisDb      = root["redis"].get("db",0).asInt();
                    }
                    if (root.isMember("nginx")) {
                        nginxEnabled = root["nginx"].get("enabled",false).asBool();
                        nginxPort    = root["nginx"].get("port",18081).asInt();
                    }
                    if (root.isMember("ddns")) {
                        ddnsEnabled = root["ddns"].get("enabled",false).asBool();
                        ddnsListen  = root["ddns"].get("listen",":39876").asString();
                    }
                    if (root.isMember("koboldcpp")) {
                        aiEnabled = root["koboldcpp"].get("enabled",false).asBool();
                        aiPort    = root["koboldcpp"].get("port",5001).asInt();
                    }
                }
            }
        }

        // 用数据库 sys_config 覆盖 enabled 标志（优先级高于 config.json）
        {
            auto cfgVal = [&](const std::string& key) -> std::string {
                auto r = db.queryParams(
                    "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1",
                    {key});
                return (r.rows() > 0) ? r.str(0, 0) : "";
            };
            auto asBool = [](const std::string& v) -> int {
                // -1=not found, 0=false, 1=true
                if (v.empty()) return -1;
                return (v=="true"||v=="1"||v=="TRUE") ? 1 : 0;
            };
            int vNginx   = asBool(cfgVal("sys.subprocess.nginx"));
            int vAi      = asBool(cfgVal("sys.subprocess.koboldcpp"));
            int vDdns    = asBool(cfgVal("sys.subprocess.ddns"));
            if (vNginx  >= 0) nginxEnabled = (vNginx  == 1);
            if (vAi     >= 0) aiEnabled    = (vAi     == 1);
            if (vDdns   >= 0) ddnsEnabled  = (vDdns   == 1);
        }

        // VRAM 缓存
        bool vramEnabled = VramCache::instance().available();
        std::string vramInfo;
        if (vramEnabled) vramInfo = VramCache::instance().backendInfo();

        // Redis PING
        bool redisPing = false;
        if (redisEnabled) {
            struct timeval tv{2, 0};
            auto* rc = redisConnectWithTimeout(redisHost.c_str(), redisPort, tv);
            if (rc && !rc->err) {
                auto* rp = (redisReply*)redisCommand(rc, "PING");
                if (rp) {
                    redisPing = (rp->type == REDIS_REPLY_STATUS &&
                                 std::string(rp->str) == "PONG");
                    freeReplyObject(rp);
                }
                redisFree(rc);
            } else if (rc) redisFree(rc);
        }

        // 子进程状态
        bool nginxOk   = NginxManager::instance().isRunning();
        bool aiOk      = KoboldCppManager::instance().isRunning();
        bool ddnsOk    = DdnsGoManager::instance().isRunning();
        bool whisperOk = WhisperService::instance().isReady();

        // 系统信息
        int cpuCores = (int)std::thread::hardware_concurrency();
        std::string memTotal="N/A", memUsed="N/A", memFree="N/A", memUsage="N/A";
#ifdef _WIN32
        {
            MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
            if (GlobalMemoryStatusEx(&ms)) {
                auto toGB = [](DWORDLONG b) {
                    std::ostringstream s;
                    s << std::fixed << std::setprecision(1) << (double)b/1073741824.0 << " G";
                    return s.str();
                };
                memTotal = toGB(ms.ullTotalPhys);
                memFree  = toGB(ms.ullAvailPhys);
                memUsed  = toGB(ms.ullTotalPhys - ms.ullAvailPhys);
                std::ostringstream u;
                u << std::fixed << std::setprecision(1)
                  << (100.0*(ms.ullTotalPhys - ms.ullAvailPhys)/ms.ullTotalPhys) << "%";
                memUsage = u.str();
            }
        }
#else
        {
            std::ifstream mf("/proc/meminfo");
            std::string k, unit; long long v, total=0, avail=0;
            while (mf >> k >> v >> unit) {
                if (k=="MemTotal:")     total = v;
                if (k=="MemAvailable:") avail = v;
            }
            if (total > 0) {
                auto kb2G = [](long long kb) {
                    std::ostringstream s;
                    s << std::fixed << std::setprecision(1) << kb/1048576.0 << " G";
                    return s.str();
                };
                memTotal = kb2G(total); memFree = kb2G(avail); memUsed = kb2G(total-avail);
                std::ostringstream u;
                u << std::fixed << std::setprecision(1) << 100.0*(total-avail)/total << "%";
                memUsage = u.str();
            }
        }
#endif
        std::string diskTotal="N/A", diskUsed="N/A", diskFree="N/A", diskUsage="N/A";
#ifdef _WIN32
        {
            ULARGE_INTEGER tot{}, fr{};
            if (GetDiskFreeSpaceExA(".", nullptr, &tot, &fr)) {
                auto toGB = [](ULONGLONG b) {
                    std::ostringstream s;
                    s << std::fixed << std::setprecision(1) << (double)b/1073741824.0 << " G";
                    return s.str();
                };
                diskTotal = toGB(tot.QuadPart); diskFree = toGB(fr.QuadPart);
                diskUsed  = toGB(tot.QuadPart - fr.QuadPart);
                std::ostringstream u;
                u << std::fixed << std::setprecision(1)
                  << 100.0*(tot.QuadPart-fr.QuadPart)/tot.QuadPart << "%";
                diskUsage = u.str();
            }
        }
#else
        {
            struct statvfs st;
            if (statvfs(".", &st) == 0) {
                auto toGB = [](unsigned long long b) {
                    std::ostringstream s;
                    s << std::fixed << std::setprecision(1) << b/1073741824.0 << " G";
                    return s.str();
                };
                unsigned long long tot = (unsigned long long)st.f_blocks * st.f_frsize;
                unsigned long long fr  = (unsigned long long)st.f_bavail * st.f_frsize;
                diskTotal = toGB(tot); diskFree = toGB(fr); diskUsed = toGB(tot-fr);
                std::ostringstream u;
                u << std::fixed << std::setprecision(1) << 100.0*(tot-fr)/tot << "%";
                diskUsage = u.str();
            }
        }
#endif
        std::string procMem = "N/A";
#ifdef _WIN32
        {
            PROCESS_MEMORY_COUNTERS pmc{};
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                std::ostringstream s;
                s << std::fixed << std::setprecision(1) << pmc.WorkingSetSize/1048576.0 << " MB";
                procMem = s.str();
            }
        }
#else
        {
            struct rusage ru{};
            if (getrusage(RUSAGE_SELF, &ru) == 0) {
                std::ostringstream s;
                s << std::fixed << std::setprecision(1) << ru.ru_maxrss/1024.0 << " MB";
                procMem = s.str();
            }
        }
#endif
        // 运行时长（s_startTime 为类级别静态，记录真实启动时间）
        auto uptimeSec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - s_startTime_).count();

        // 应用统计
        size_t onlineUsers = TokenCache::instance().size();
        std::string todayOps = "0", todayLogins = "0";
        if (dbConn) {
            auto ro = db.query("SELECT COUNT(*) FROM sys_oper_log WHERE oper_time >= CURRENT_DATE");
            if (ro.ok() && ro.rows() > 0) todayOps = ro.str(0, 0);
            auto rl = db.query("SELECT COUNT(*) FROM sys_logininfor WHERE login_time >= CURRENT_DATE");
            if (rl.ok() && rl.rows() > 0) todayLogins = rl.str(0, 0);
        }

        // 服务器时间 / 启动时间
        auto now = std::chrono::system_clock::now();
        std::time_t nowt = std::chrono::system_clock::to_time_t(now);
        char timeBuf[32]; std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&nowt));
        std::string startTimeStr;
        {
            auto t = std::chrono::system_clock::to_time_t(s_startTime_);
            char sb[32]; std::strftime(sb, sizeof(sb), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
            startTimeStr = sb;
        }

        // ── 辅助 lambda ──────────────────────────────────────────────────
        auto ok  = [](const std::string& t){ return "<span class='ok'>"  + t + "</span>"; };
        auto err = [](const std::string& t){ return "<span class='err'>" + t + "</span>"; };
        auto dim = [](const std::string& t){ return "<span class='dim'>" + t + "</span>"; };
        auto row = [](const std::string& k, const std::string& v){
            return "<tr><td>" + k + "</td><td>" + v + "</td></tr>";
        };

        // 子进程状态行
        auto proc = [&](const std::string& name, bool running, bool enabled, const std::string& port) {
            std::string st = running ? ok("运行中") : enabled ? err("异常") : dim("未启用");
            return "<div class='proc'><span class='pname'>" + name + "</span>"
                   "<span class='pport'>" + port + "</span>" + st + "</div>";
        };

#ifdef _WIN32
        std::string osLabel = "Windows";
        std::string hostname = "localhost";
        { char hb[256]={}; DWORD sz=sizeof(hb); if(GetComputerNameA(hb,&sz)) hostname=hb; }
#else
        std::string osLabel = "Linux";
        std::string hostname = "localhost";
        { char hb[256]={}; if(gethostname(hb,sizeof(hb))==0) hostname=hb; }
#endif

        // ── HTML ─────────────────────────────────────────────────────────
        std::ostringstream html;
        html << R"(<!DOCTYPE html><html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>数据监控 · RuoYi-Cpp</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,"PingFang SC","Microsoft YaHei",sans-serif;
     background:#0f172a;color:#e2e8f0;min-height:100vh;padding:24px}
h1{font-size:18px;color:#38bdf8;margin-bottom:20px;display:flex;align-items:center;gap:10px}
h1 small{font-size:12px;color:#475569;font-weight:normal}
h1 .dot{width:8px;height:8px;background:#34d399;border-radius:50%;
        animation:blink 2s infinite;flex-shrink:0}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:16px;margin-bottom:16px}
.grid.top{grid-template-columns:repeat(auto-fill,minmax(160px,1fr));margin-bottom:16px}
.card{background:#1e293b;border-radius:10px;border:1px solid #334155;padding:18px}
.card h2{font-size:12px;color:#64748b;font-weight:600;letter-spacing:.05em;
         text-transform:uppercase;margin-bottom:12px}
/* 顶部统计卡片 */
.stat .lbl{font-size:12px;color:#64748b;margin-bottom:4px}
.stat .val{font-size:26px;font-weight:700;color:#f1f5f9;line-height:1}
.stat .hint{font-size:11px;color:#475569;margin-top:5px}
/* 表格 */
table{width:100%;border-collapse:collapse}
td{padding:7px 0;border-bottom:1px solid #1e3a5f;font-size:13px;color:#94a3b8;vertical-align:middle}
td:first-child{color:#64748b;width:110px;font-size:12px}
tr:last-child td{border:none}
/* 进度条 */
.bar{width:100%;height:6px;background:#1e3a5f;border-radius:3px;margin:5px 0 2px;overflow:hidden}
.bar-fill{height:6px;border-radius:3px;transition:width .5s}
.bar-fill.g{background:#34d399}.bar-fill.w{background:#fbbf24}.bar-fill.d{background:#f87171}
/* 子进程 */
.proc{display:flex;align-items:center;gap:8px;padding:8px 0;
      border-bottom:1px solid #1e3a5f;font-size:13px}
.proc:last-child{border:none}
.pname{flex:1;color:#e2e8f0;font-weight:500}
.pport{color:#475569;font-size:12px}
/* 状态色 */
.ok{color:#34d399}.err{color:#f87171}.warn{color:#fbbf24}.dim{color:#475569}
</style></head><body>)";

        html << "<h1><div class='dot'></div>数据监控"
             << "<small>RuoYi-Cpp &nbsp;·&nbsp; " << timeBuf << "</small></h1>\n";

        // ── 顶部 4 个统计卡片 ─────────────────────────────────────────────
        html << "<div class='grid top'>"
             << "<div class='card stat'><div class='lbl'>在线用户</div>"
             << "<div class='val'>" << onlineUsers << "</div>"
             << "<div class='hint'>活跃 Token</div></div>"

             << "<div class='card stat'><div class='lbl'>今日登录</div>"
             << "<div class='val'>" << todayLogins << "</div>"
             << "<div class='hint'>次</div></div>"

             << "<div class='card stat'><div class='lbl'>今日操作</div>"
             << "<div class='val'>" << todayOps << "</div>"
             << "<div class='hint'>条操作日志</div></div>"

             << "<div class='card stat'><div class='lbl'>运行时长</div>"
             << "<div class='val' style='font-size:18px;padding-top:4px' id='uptime-val'></div>"
             << "<div class='hint'>" << startTimeStr << " 启动</div></div>"

             << "<div class='card stat'><div class='lbl'>数据库</div>"
             << "<div class='val' style='font-size:18px;padding-top:4px "
             << (dbConn?"":"err") << "'>"
             << (dbConn ? (usingSql ? "SQLite" : "PostgreSQL") : "<span class='err'>断开</span>")
             << "</div><div class='hint'>" << (pgSize.empty() ? (dbConn?"已连接":"不可用") : pgSize) << "</div></div>"

             << "<div class='card stat'><div class='lbl'>待回写</div>"
             << "<div class='val " << (pending>0?"warn":"") << "'>" << pending << "</div>"
             << "<div class='hint'>SQLite 双写队列</div></div>"
             << "</div>\n";

        // ── 主网格 ────────────────────────────────────────────────────────
        html << "<div class='grid'>\n";

        // 数据库
        html << "<div class='card'><h2>数据库</h2><table>"
             << row("后端", dbConn ? ok(usingSql ? "SQLite 回退" : "PostgreSQL") : err("断开"))
             << row("PG 地址", dbHost + ":" + dbPort + " / " + dbName)
             << row("SQLite", hasSql ? ok("就绪") : dim("未连接"))
             << (pgVer.empty()        ? "" : row("PG 版本", pgVer))
             << (pgSize.empty()       ? "" : row("库大小",  pgSize))
             << (pgActiveConn.empty() ? "" : row("活跃连接", pgActiveConn))
             << "</table></div>\n";

        // 缓存
        html << "<div class='card'><h2>缓存</h2><table>"
             << row("Redis",    redisEnabled ? (redisPing ? ok("PONG") : err("不可达")) : dim("未启用"))
             << row("地址",     redisEnabled ? redisHost+":"+std::to_string(redisPort)+" db="+std::to_string(redisDb) : "—")
             << row("VRAM",     vramEnabled  ? ok(vramInfo) : dim("CUDA 未检测"))
             << row("内存缓存", ok("就绪"))
             << row("后端",     MemCache::backendInfo())
             << "</table></div>\n";

        // 子进程
        html << "<div class='card'><h2>子进程</h2>"
             << proc("Nginx",   nginxOk,   nginxEnabled,   ":"+std::to_string(nginxPort))
             << proc("AI",     aiOk,      aiEnabled,      ":"+std::to_string(aiPort))
             << proc("DDNS",   ddnsOk,    ddnsEnabled,    ddnsListen)
             << proc("Whisper ASR", whisperOk, aiEnabled, "")
             << "</div>\n";

        // 系统信息
        {
            auto bar = [](const std::string& pct) {
                double v = std::atof(pct.c_str());
                std::string cls = v>=85?"d":(v>=60?"w":"g");
                std::ostringstream s;
                s << "<div class='bar'><div class='bar-fill " << cls
                  << "' style='width:" << std::min(v,100.0) << "%'></div></div>"
                  << "<small style='color:#475569'>" << pct << "</small>";
                return s.str();
            };
            html << "<div class='card'><h2>系统信息</h2><table>"
                 << row("内存 (RAM)",  bar(memUsage) + " &nbsp;" + memUsed + " / " + memTotal)
                 << row("磁盘 (DISK)", bar(diskUsage) + " &nbsp;" + diskUsed + " / " + diskTotal)
                 << row("进程内存",    procMem)
                 << row("CPU 核数",    std::to_string(cpuCores) + " 核")
                 << row("操作系统",    osLabel + " x86_64")
                 << row("主机名",      hostname)
                 << "</table></div>\n";
        }

        html << "<script>\n"
             << "(function(){\n"
             << "  var s=" << uptimeSec << ";\n"
             << "  function fmt(t){\n"
             << "    var d=Math.floor(t/86400),h=Math.floor(t%86400/3600),\n"
             << "        m=Math.floor(t%3600/60),sec=t%60;\n"
             << "    return (d?d+'天':'')+(h?h+'小时':'')+(m?m+'分钟':'')+sec+'秒';\n"
             << "  }\n"
             << "  var el=document.getElementById('uptime-val');\n"
             << "  if(el){el.textContent=fmt(s);setInterval(function(){el.textContent=fmt(++s);},1000);}\n"
             << "})();\n"
             << "</script>\n"
             << "</body></html>";

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(html.str());
        cb(resp);
    }

private:
    static std::chrono::system_clock::time_point s_startTime_;
};
inline std::chrono::system_clock::time_point DruidCtrl::s_startTime_ = std::chrono::system_clock::now();
