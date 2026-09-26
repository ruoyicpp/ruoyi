/**
 * @file LicenseManager.h
 * @brief 许可证管理器 — 企业级软件许可证验证
 * 
 * 功能概述：
 *   - 许可证验证：验证软件许可证的有效性
 *   - 硬件绑定：将许可证绑定到特定硬件
 *   - 过期管理：管理许可证的过期时间和宽限期
 *   - 功能模块：支持按功能模块授权
 *   - 用户限制：支持限制并发用户数
 * 
 * 许可证文件格式：
 *   license.lic（放在可执行文件同目录）
 *   licensee=Company Name
 *   fpHash=<SHA256 of full fingerprint>
 *   fpPrimary=<SHA256 of machine guid only>
 *   issueDate=2024-01-01
 *   expireDate=2025-01-01 或 PERPETUAL
 *   maxUsers=100
 *   features=module1,module2,module3
 *   graceDays=7
 *   sig=<HMAC-SHA256 signature>
 * 
 * 许可证状态：
 *   - VALID：完全有效
 *   - EXPIRING_SOON：30 天内到期或在宽限期内
 *   - EXPIRED：超过宽限期
 *   - HARDWARE_MISMATCH：硬件指纹完全不匹配
 *   - HARDWARE_UPGRADED：主因子匹配，次因子变化（硬件升级）
 *   - INVALID_SIGNATURE：签名无效（文件被篡改）
 *   - FILE_NOT_FOUND：license.lic 不存在
 *   - PARSE_ERROR：文件格式错误
 * 
 * 硬件绑定策略：
 *   - fpHash：完整硬件指纹（MachineGuid + CPU + DiskSerial）
 *   - fpPrimary：主因子（MachineGuid）
 *   - 允许次因子变化（硬件升级）
 * 
 * 使用示例：
 *   // 初始化许可证管理器
 *   auto info = LicenseManager::load();
 *   
 *   // 检查许可证状态
 *   if (info.status == LicenseManager::Status::VALID) {
 *       std::cout << "License is valid" << std::endl;
 *       std::cout << "Licensee: " << info.licensee << std::endl;
 *       std::cout << "Days left: " << info.daysLeft << std::endl;
 *   }
 *   
 *   // 检查功能授权
 *   if (LicenseManager::hasFeature(info, "advanced_analytics")) {
 *       // 启用高级分析功能
 *   }
 *   
 *   // 检查用户限制
 *   if (info.maxUsers > 0 && currentUsers >= info.maxUsers) {
 *       // 拒绝新用户登录
 *   }
 * 
 * 特性：
 *   - 硬件绑定：防止许可证被复制到其他设备
 *   - 签名验证：使用 HMAC-SHA256 防止文件篡改
 *   - 宽限期：到期后提供宽限期（默认 7 天）
 *   - 功能模块：支持按功能模块授权
 *   - 用户限制：支持限制并发用户数
 *   - 文件监控：监控 license.lic 文件变化
 * 
 * 配置项（config.json）：
 *   - license.enabled: 是否启用许可证验证（默认 false）
 *   - license.grace_days: 到期宽限天数（默认 7）
 *   - license.check_interval: 许可证检查间隔（秒，默认 3600）
 */

#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <thread>
#include <sys/stat.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <random>
#include <json/json.h>
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#else
#  include <unistd.h>
#  include <sys/inotify.h>
#  include <curl/curl.h>
#endif
#include "HardwareFingerprint.h"
#include "AjaxResult.h"

// ─── 许可证状态 ────────────────────────────────────────────────────────────
namespace LicenseManager {

enum class Status {
    VALID,              // 完全有效
    EXPIRING_SOON,      // 30天内到期或在宽限期内
    EXPIRED,            // 超过宽限期
    HARDWARE_MISMATCH,  // 硬件指纹完全不匹配
    HARDWARE_UPGRADED,  // 主因子匹配，次因子变化（硬件升级）
    INVALID_SIGNATURE,  // 签名无效（文件被篡改）
    FILE_NOT_FOUND,     // license.lic 不存在
    PARSE_ERROR,        // 文件格式错误
    REVOKED,            // 服务端已吊销/禁用（远程验证拒绝）
    REMOTE_FAIL,        // 远程验证不可达且超出离线宽限期
};

struct LicenseInfo {
    std::string licensee;     // 被授权方
    std::string fpHash;       // SHA256(full fingerprint)
    std::string fpPrimary;    // SHA256(machine guid only)
    std::string issueDate;    // 颁发日期 YYYY-MM-DD
    std::string expireDate;   // "PERPETUAL" 或 "YYYY-MM-DD"
    int         maxUsers  = 0;// 0=不限
    std::string features;     // 功能模块，逗号分隔
    int         graceDays = 7;// 到期宽限天数
    std::string signature;    // HMAC 签名
    // 远程验证字段（不参与本地签名，服务端用 fp 绑定防篡改）
    std::string licenseKey;   // RUOYI-XXXX-XXXX-XXXX 授权码
    std::string server;       // 签发服务端 base URL（如 https://lic.example.com）
    // 运行时状态（不参与签名）
    Status      status    = Status::FILE_NOT_FOUND;
    int         daysLeft  = 0;// >0 剩余天数，<0 已过期天数
};

// ─── 厂商 HMAC 密钥（与硬件绑定密钥分离）────────────────────────────────
static inline std::string _licKey() {
    static const uint8_t a[] = {0x4D,0xA3,0x7F,0xC1,0x98,0x2E,0x5B,0x60};
    static const uint8_t b[] = {0xE7,0x14,0x8D,0x3A,0xF6,0x51,0x29,0xBC};
    static const uint8_t c[] = {0x07,0x9E,0x4C,0xD5,0x72,0xAB,0x1F,0x83};
    static const uint8_t d[] = {0x6E,0x30,0xC4,0x57,0x8A,0x1D,0x95,0x42};
    std::vector<uint8_t> k;
    k.insert(k.end(), std::begin(a), std::end(a));
    k.insert(k.end(), std::begin(b), std::end(b));
    k.insert(k.end(), std::begin(c), std::end(c));
    k.insert(k.end(), std::begin(d), std::end(d));
    std::string r(reinterpret_cast<const char*>(k.data()), k.size());
    std::fill(k.begin(), k.end(), 0);
    return r;
}

// ─── 工具函数 ─────────────────────────────────────────────────────────────

static inline std::string _licPath() {
    char exe[512] = {};
#ifdef _WIN32
    GetModuleFileNameA(nullptr, exe, 512);
    std::string p(exe);
    auto s = p.find_last_of("\\/");
    return p.substr(0, s + 1) + "license.lic";
#else
    ssize_t n = readlink("/proc/self/exe", exe, 511);
    std::string p = (n > 0) ? std::string(exe, n) : "license.lic";
    auto s = p.find_last_of('/');
    return (s != std::string::npos) ? p.substr(0, s + 1) + "license.lic" : "license.lic";
#endif
}

static inline int _daysUntil(const std::string& ds) {
    if (ds == "PERPETUAL") return 99999;
    int y = 0, m = 0, d = 0;
    if (sscanf(ds.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return -99999;
    std::tm t = {};
    t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d;
    t.tm_hour = 23; t.tm_min = 59; t.tm_sec = 59;
    time_t exp = mktime(&t);
    return static_cast<int>((exp - time(nullptr)) / 86400);
}

static inline std::string _today() {
    time_t t = time(nullptr);
    std::tm* tm = localtime(&t);
    char buf[32];   // 防 -Wformat-truncation：tm_year 理论上可超 4 位
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    return buf;
}

// 签名原文（不含 sig 行）
static inline std::string _signSrc(const LicenseInfo& info) {
    std::ostringstream ss;
    ss << "licensee=" << info.licensee     << "\n"
       << "fp_hash="  << info.fpHash       << "\n"
       << "fp_primary="<< info.fpPrimary   << "\n"
       << "issue_date="<< info.issueDate   << "\n"
       << "expire_date="<< info.expireDate << "\n"
       << "max_users=" << info.maxUsers    << "\n"
       << "features="  << info.features    << "\n"
       << "grace_days="<< info.graceDays   << "\n";
    return ss.str();
}

static inline std::string _hmacSign(const std::string& content) {
    std::string key = _licKey();
    std::string sig = HardwareFingerprint::hmacHex(content, key);
    std::fill(key.begin(), key.end(), 0);
    return sig;
}

// ─── 全局许可证缓存 ───────────────────────────────────────────────────────
static inline LicenseInfo& _cached() {
    static LicenseInfo inst;
    return inst;
}

// 远程吊销原子标志：心跳线程写、HTTP 线程读，避免直接改 _cached().status 的数据竞争
static inline std::atomic<bool>& _revoked() {
    static std::atomic<bool> b{false};
    return b;
}

// ─── 解析 ─────────────────────────────────────────────────────────────────
static inline LicenseInfo parse(const std::string& content) {
    LicenseInfo info;
    info.status = Status::PARSE_ERROR;

    auto getVal = [&](const std::string& raw) -> std::string {
        auto p = raw.find('=');
        return (p == std::string::npos) ? "" : raw.substr(p + 1);
    };

    std::istringstream iss(content);
    std::string line;
    std::string body; // 重构签名原文
    while (std::getline(iss, line)) {
        // 去掉行尾 \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if      (line.rfind("licensee=",  0) == 0) { info.licensee  = getVal(line); body += line + "\n"; }
        else if (line.rfind("fp_hash=",   0) == 0) { info.fpHash    = getVal(line); body += line + "\n"; }
        else if (line.rfind("fp_primary=",0) == 0) { info.fpPrimary = getVal(line); body += line + "\n"; }
        else if (line.rfind("issue_date=",0) == 0) { info.issueDate = getVal(line); body += line + "\n"; }
        else if (line.rfind("expire_date=",0)==0)  { info.expireDate= getVal(line); body += line + "\n"; }
        else if (line.rfind("max_users=", 0) == 0) { try { info.maxUsers = std::stoi(getVal(line)); } catch (...) {} body += line + "\n"; }
        else if (line.rfind("features=",  0) == 0) { info.features  = getVal(line); body += line + "\n"; }
        else if (line.rfind("grace_days=",0) == 0) { try { info.graceDays= std::stoi(getVal(line)); } catch (...) {} body += line + "\n"; }
        else if (line.rfind("sig=",       0) == 0) { info.signature = getVal(line); }
        // 远程验证扩展字段：不进 body（不影响 HMAC 签名，服务端 fp 绑定防换 key）
        else if (line.rfind("license_key=",0)==0)  { info.licenseKey = getVal(line); }
        else if (line.rfind("server=",    0) == 0) { info.server     = getVal(line); }
    }

    if (info.licensee.empty() || info.fpHash.empty() || info.signature.empty())
        return info; // PARSE_ERROR

    // 验签
    std::string expected = _hmacSign(body);
    if (expected != info.signature) {
        info.status = Status::INVALID_SIGNATURE;
        return info;
    }

    // 硬件指纹校验
    auto fac = HardwareFingerprint::computeFactors();
    std::string curHash    = HardwareFingerprint::sha256hex(fac.full);
    std::string curPrimary = HardwareFingerprint::sha256hex(fac.primary);

    if (curHash != info.fpHash) {
        if (curPrimary == info.fpPrimary)
            info.status = Status::HARDWARE_UPGRADED;
        else
            info.status = Status::HARDWARE_MISMATCH;
        return info;
    }

    // 有效期校验
    info.daysLeft = _daysUntil(info.expireDate);
    if (info.daysLeft < -info.graceDays) {
        info.status = Status::EXPIRED;
    } else if (info.daysLeft < 30) {
        info.status = Status::EXPIRING_SOON;
    } else {
        info.status = Status::VALID;
    }
    return info;
}

// ─── 读取并解析 ───────────────────────────────────────────────────────────
static inline LicenseInfo check() {
    std::string path = _licPath();
    std::ifstream f(path);
    if (!f.is_open()) {
        LicenseInfo info;
        info.status = Status::FILE_NOT_FOUND;
        return info;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse(ss.str());
}

// ═══════════════════════════════════════════════════════════════════════
// 远程验证（客户端侧）
//   本地 .lic 验签通过后 → POST {license_key,fp_hash,fp_primary} 到签发服务端
//   /api/license/verify（对应 LicenseApiCtrl），服务端查 sys_license 判定。
//   网络不可达时用 data/.license_ping（HMAC 签名）里"上次远程通过时间"做
//   离线宽限，超过 offline_days 仍未连通则拒绝启动。
//   周期心跳 /api/license/heartbeat 由 LicenseWatcher 循环驱动。
// ═══════════════════════════════════════════════════════════════════════

struct RemoteCfg {
    bool        enabled     = false;   // license.remote.enabled 或 lic.server 非空即启用
    std::string url;                   // 服务端 base URL（无 license_key 时的兜底地址）
    int         intervalSec = 3600;    // 心跳/复检间隔（秒）
    int         offlineDays = 7;       // 远程不可达时最大离线宽限天数
    int         timeoutMs   = 5000;
};

static inline RemoteCfg& remoteCfg() {
    static RemoteCfg c;
    return c;
}

/// 从 license.remote JSON 节点应用到 remoteCfg（config.json 与 sys_config 覆盖层共用）
static inline void applyRemoteJson(const Json::Value& sec) {
    if (sec.isNull() || !sec.isObject()) return;
    auto& r = remoteCfg();
    // 类型守卫：sys_config 覆盖层的值经 parseValue 转 JSON，坏类型不应用、不抛异常
    if (sec.isMember("enabled")      && sec["enabled"].isBool())
        r.enabled = sec["enabled"].asBool();
    if (sec.isMember("url")          && sec["url"].isString())
        r.url         = sec["url"].asString();
    if (sec.isMember("interval_sec") && sec["interval_sec"].isIntegral())
        r.intervalSec = sec["interval_sec"].asInt();
    if (sec.isMember("offline_days") && sec["offline_days"].isIntegral())
        r.offlineDays = sec["offline_days"].asInt();
    if (sec.isMember("timeout_ms")   && sec["timeout_ms"].isIntegral())
        r.timeoutMs   = sec["timeout_ms"].asInt();
    if (r.intervalSec < 60) r.intervalSec = 60;   // 下限 1min，防刷爆服务端
}

/// url 留空时推导本服务自身地址：menu.api_base_url → listeners[0]
/// （签发端和验证端是同一个进程时零配置）
static inline void deriveSelfUrl(const Json::Value& root) {
    auto& r = remoteCfg();
    if (!r.url.empty()) return;
    std::string base = root["menu"].get("api_base_url", "").asString();
    if (base.empty() && root["listeners"].isArray() && !root["listeners"].empty()) {
        const auto& l = root["listeners"][0];
        std::string scheme = l.get("https", false).asBool() ? "https" : "http";
        std::string host   = l.get("address", "127.0.0.1").asString();
        if (host == "0.0.0.0" || host == "::") host = "127.0.0.1";
        int port = l.get("port", 18080).asInt();
        base = scheme + "://" + host + ":" + std::to_string(port);
    }
    r.url = base;
}

/// 启动时从 config.json 读 license.remote 段
static inline void loadRemoteConfig(const std::string& cfgPath = "config.json") {
    std::ifstream f(cfgPath);
    if (!f.is_open()) return;
    Json::Value root;
    Json::CharReaderBuilder rb;
    std::string errs;
    if (!Json::parseFromStream(rb, f, &root, &errs)) return;
    applyRemoteJson(root["license"]["remote"]);
    deriveSelfUrl(root);
}

// ── URL 解析：scheme://host[:port][/path] ────────────────────────────────
struct _UrlParts {
    bool        https = false;
    std::string host;
    int         port = 0;
    std::string path = "/";
    bool        ok   = false;
};
static inline _UrlParts _parseUrl(const std::string& url) {
    _UrlParts p;
    std::string u = url;
    if (u.rfind("https://", 0) == 0) { p.https = true;  u = u.substr(8); }
    else if (u.rfind("http://", 0) == 0) { u = u.substr(7); }
    else return p;
    auto slash = u.find('/');
    std::string hp = (slash == std::string::npos) ? u : u.substr(0, slash);
    if (slash != std::string::npos) p.path = u.substr(slash);
    auto colon = hp.rfind(':');
    if (colon != std::string::npos) {
        p.host = hp.substr(0, colon);
        try { p.port = std::stoi(hp.substr(colon + 1)); } catch (...) { return p; }
    } else {
        p.host = hp;
        p.port = p.https ? 443 : 80;
    }
    p.ok = !p.host.empty();
    return p;
}

// ── 跨平台 HTTP POST（JSON body），返回响应体；失败返回空 ────────────────
static inline std::string _httpPost(const std::string& url,
                                    const std::string& body,
                                    int timeoutMs) {
    auto u = _parseUrl(url);
    if (!u.ok) return "";
#ifdef _WIN32
    std::wstring whost(u.host.begin(), u.host.end());
    std::wstring wpath(u.path.begin(), u.path.end());
    HINTERNET hS = WinHttpOpen(L"RuoYi-License/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hS) return "";
    WinHttpSetTimeouts(hS, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
    HINTERNET hC = WinHttpConnect(hS, whost.c_str(), (INTERNET_PORT)u.port, 0);
    if (!hC) { WinHttpCloseHandle(hS); return ""; }
    HINTERNET hR = WinHttpOpenRequest(hC, L"POST", wpath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        u.https ? WINHTTP_FLAG_SECURE : 0);
    if (!hR) { WinHttpCloseHandle(hC); WinHttpCloseHandle(hS); return ""; }
    if (u.https) {
        DWORD sf = SECURITY_FLAG_IGNORE_UNKNOWN_CA
                 | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE
                 | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
                 | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hR, WINHTTP_OPTION_SECURITY_FLAGS, &sf, sizeof(sf));
    }
    std::wstring ctype = L"Content-Type: application/json";
    LPVOID opt = body.empty() ? nullptr : (LPVOID)body.data();   // 空 body 显式 NULL
    BOOL okSend = WinHttpSendRequest(hR, ctype.c_str(), (DWORD)-1,
        opt, (DWORD)body.size(), (DWORD)body.size(), 0);
    std::string out;
    if (okSend && WinHttpReceiveResponse(hR, nullptr)) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hR, &avail) && avail > 0) {
            std::string buf(avail, '\0'); DWORD rd = 0;
            WinHttpReadData(hR, &buf[0], avail, &rd);
            out.append(buf.data(), rd);
        }
    }
    WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
    return out;
#else
    // 显式全局初始化：std::call_once 保证线程安全，不依赖"主线程先跑 remoteVerify"的时序假设
    static std::once_flag curlInitOnce;
    std::call_once(curlInitOnce, [] { curl_global_init(CURL_GLOBAL_ALL); });
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    std::string out;
    auto wrCb = +[](char* ptr, size_t sz, size_t nm, void* ud) -> size_t {
        ((std::string*)ud)->append(ptr, sz * nm);
        return sz * nm;
    };
    struct curl_slist* hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    // 服务端 isBotUserAgent 拦空 UA / curl/ 默认 UA，必须显式设置
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "RuoYi-License/1.0");
    curl_easy_setopt(curl, CURLOPT_POST,           1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     hdrs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  wrCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &out);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,     (long)timeoutMs);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)timeoutMs);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL,       1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);   // 与客户端环境容忍度一致
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return (rc == CURLE_OK) ? out : "";
#endif
}

// ── .license_ping 签名缓存：记录上次远程验证通过时间 ─────────────────────
// 文件内容（HMAC 签名防伪造离线时间）：
//   fp_hash=<hash>
//   last_ok=<unix_ts>
//   sig=<hmac>
static inline std::string _pingPath() {
    std::string p = _licPath();
    auto s = p.find_last_of("\\/");
    return (s != std::string::npos) ? p.substr(0, s + 1) + "data/.license_ping"
                                    : "data/.license_ping";
}

static inline bool _writePing(const std::string& fpHash) {
    std::ostringstream body;
    body << "fp_hash=" << fpHash << "\n"
         << "last_ok=" << (long long)std::time(nullptr) << "\n";
    std::string content = body.str() + "sig=" + _hmacSign(body.str()) + "\n";
    std::string pp = _pingPath();
    // 确保父目录（data/）存在
    auto s = pp.find_last_of("\\/");
    if (s != std::string::npos) {
        std::string dir = pp.substr(0, s);
        struct stat st{};
        if (stat(dir.c_str(), &st) != 0) {
#ifdef _WIN32
            CreateDirectoryA(dir.c_str(), nullptr);
#else
            mkdir(dir.c_str(), 0700);
#endif
        }
    }
    std::ofstream f(pp, std::ios::trunc);
    if (!f) return false;
    f << content;
    return true;
}

/// 返回距上次远程验证通过的秒数；无记录/验签失败返回 -1
static inline long long _readPingAge(const std::string& fpHash) {
    std::ifstream f(_pingPath());
    if (!f.is_open()) return -1;
    std::ostringstream ss; ss << f.rdbuf();
    std::istringstream iss(ss.str());
    std::string line, body, sig, fp;
    long long ts = 0;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("fp_hash=", 0) == 0) { fp = line.substr(8);  body += line + "\n"; }
        else if (line.rfind("last_ok=", 0) == 0) {
            try { ts = std::stoll(line.substr(8)); } catch (...) { return -1; }
            body += line + "\n";
        }
        else if (line.rfind("sig=", 0) == 0) sig = line.substr(4);
    }
    if (fp != fpHash || sig.empty() || ts <= 0) return -1;
    if (_hmacSign(body) != sig) return -1;   // 被篡改
    return (long long)std::time(nullptr) - ts;
}

// ── 远程验证主函数 ───────────────────────────────────────────────────────
// 返回值即最终 status；VALID 表示远程通过。
static inline Status remoteVerify(const LicenseInfo& lic) {
    auto& cfg = remoteCfg();

    // 地址优先级：lic.server（签发时写入）> config license.remote.url
    // 启用条件：lic 内嵌 server 自动启用；走 config.url 时需 enabled=true
    std::string base;
    if (!lic.server.empty())     base = lic.server;
    else if (cfg.enabled)        base = cfg.url;
    if (base.empty() || lic.licenseKey.empty()) {
        // 无远程地址 / 老 .lic 无授权码 → 纯离线模式
        return lic.status;
    }

    std::string baseUrl = base;
    while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
    std::string verifyUrl = baseUrl + "/api/license/verify";

    Json::Value req;
    req["license_key"] = lic.licenseKey;
    req["fp_hash"]     = lic.fpHash;
    req["fp_primary"]  = lic.fpPrimary;
    Json::StreamWriterBuilder wb;
    std::string bodyStr = Json::writeString(wb, req);

    std::string resp = _httpPost(verifyUrl, bodyStr, cfg.timeoutMs);
    if (!resp.empty()) {
        Json::Value j;
        Json::CharReaderBuilder rb; std::string e;
        std::istringstream is(resp);
        if (Json::parseFromStream(rb, is, &j, &e)) {
            int code = j.get("code", -1).asInt();
            if (code == 0) {
                _writePing(lic.fpHash);
                return lic.status;   // 远程确认有效，保留本地判定的状态
            }
            return Status::REVOKED;  // 服务端明确拒绝（无效/禁用/指纹不符）
        }
    }

    // 网络不可达或响应异常 → 离线宽限判定
    long long age = _readPingAge(lic.fpHash);
    if (age >= 0 && age <= (long long)cfg.offlineDays * 86400) {
        std::cout << "[License] 远程验证不可达，离线宽限期内放行"
                     "（距上次验证 " << (age / 3600) << " 小时，上限 "
                  << cfg.offlineDays << " 天）\n";
        return lic.status;
    }
    return Status::REMOTE_FAIL;
}

// ── 心跳：LicenseWatcher 周期调用 ────────────────────────────────────────
/// 返回 true 表示服务端仍认可该 license；网络失败也算 true（宽限由启动期 remoteVerify 管）
static inline bool remoteHeartbeat(const LicenseInfo& lic) {
    auto& cfg = remoteCfg();
    std::string base;
    if (!lic.server.empty())     base = lic.server;
    else if (cfg.enabled)        base = cfg.url;
    if (base.empty() || lic.licenseKey.empty()) return true;

    std::string baseUrl = base;
    while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();

    Json::Value req;
    req["license_key"] = lic.licenseKey;
    req["fp_hash"]     = lic.fpHash;
    Json::StreamWriterBuilder wb;
    std::string bodyStr = Json::writeString(wb, req);

    std::string resp = _httpPost(baseUrl + "/api/license/heartbeat",
                                 bodyStr, cfg.timeoutMs);
    if (resp.empty()) return true;   // 网络抖动不处理，靠启动期宽限兜底
    Json::Value j;
    Json::CharReaderBuilder rb; std::string e;
    std::istringstream is(resp);
    if (!Json::parseFromStream(rb, is, &j, &e)) return true;
    if (j.get("code", -1).asInt() == 0) {
        _writePing(lic.fpHash);
        return true;
    }
    return false;   // 服务端明确拒绝 → 吊销生效
}

// ─── 生成 license.lic 内容（厂商侧工具使用）────────────────────────────
static inline std::string generate(
        const std::string& licensee,
        const std::string& fpHash,
        const std::string& fpPrimary,
        const std::string& expireDate = "PERPETUAL",
        int maxUsers = 0,
        const std::string& features = "FULL",
        int graceDays = 7,
        const std::string& licenseKey = "",   // 远程授权码（可选，写入 lic 尾行）
        const std::string& server     = "") { // 签发服务端 URL（可选）
    LicenseInfo info;
    info.licensee  = licensee;
    info.fpHash    = fpHash;
    info.fpPrimary = fpPrimary;
    info.issueDate = _today();
    info.expireDate= expireDate;
    info.maxUsers  = maxUsers;
    info.features  = features;
    info.graceDays = graceDays;

    std::string body = _signSrc(info);
    info.signature   = _hmacSign(body);

    std::string out = body + "sig=" + info.signature + "\n";
    // 远程验证扩展字段放 sig 之后（不签名，服务端 fp 绑定防换 key）
    if (!licenseKey.empty()) out += "license_key=" + licenseKey + "\n";
    if (!server.empty())     out += "server="      + server     + "\n";
    return out;
}

// ─── 启动校验 ────────────────────────────────────────────────────────────
static inline bool isAllowedToStart(const LicenseInfo& lic) {
#ifdef RUOYI_DEV_BYPASS_LICENSE
    // 开发模式：CMake 通过 -DRUOYI_DEV_BYPASS_LICENSE=ON 启用，绕过许可证校验
    (void)lic;
    return true;
#else
    if (_revoked().load()) return false;   // 心跳已被服务端吊销
    switch (lic.status) {
        case Status::VALID:
        case Status::EXPIRING_SOON:
        case Status::HARDWARE_UPGRADED: // 硬件升级宽容
            return true;
        default:
            return false;
    }
#endif
}

// ─── 功能标志校验 ─────────────────────────────────────────────────────────
static inline bool hasFeature(const std::string& feature) {
    const auto& lic = _cached();
    if (!isAllowedToStart(lic)) return false;
    const std::string& feat = lic.features;
    if (feat == "FULL" || feat.find("FULL") != std::string::npos) return true;
    // 按逗号分隔搜索
    std::istringstream ss(feat);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        // trim
        tok.erase(tok.begin(), std::find_if(tok.begin(), tok.end(), [](char c){ return !std::isspace((unsigned char)c); }));
        tok.erase(std::find_if(tok.rbegin(), tok.rend(), [](char c){ return !std::isspace((unsigned char)c); }).base(), tok.end());
        if (tok == feature) return true;
    }
    return false;
}

// ─── 控制台输出 ───────────────────────────────────────────────────────────
static inline void printPanel(const LicenseInfo& lic) {
    // ANSI 颜色（直接内联，避免引入额外头文件）
    constexpr const char* GOLD   = "\x1b[38;2;255;215;0m";
    constexpr const char* GREEN  = "\x1b[1;32m";
    constexpr const char* YELLOW = "\x1b[1;33m";
    constexpr const char* RED    = "\x1b[1;31m";
    constexpr const char* RESET  = "\x1b[0m";

    // 按状态选颜色
    const char* stColor = YELLOW;
    switch (lic.status) {
        case Status::VALID:             stColor = GREEN;  break;
        case Status::EXPIRING_SOON:
        case Status::HARDWARE_UPGRADED:
        case Status::FILE_NOT_FOUND:    stColor = YELLOW; break;
        default:                        stColor = RED;    break;
    }

    auto statusStr = [](Status s) -> const char* {
        switch (s) {
            case Status::VALID:             return "✅ 有效";
            case Status::EXPIRING_SOON:     return "⚠️  即将到期";
            case Status::EXPIRED:           return "❌ 已过期";
            case Status::HARDWARE_MISMATCH: return "❌ 硬件不匹配";
            case Status::HARDWARE_UPGRADED: return "⚠️  硬件已升级（宽容）";
            case Status::INVALID_SIGNATURE: return "❌ 签名无效（文件被篡改）";
            case Status::FILE_NOT_FOUND:    return "❌ license.lic 未找到";
            case Status::PARSE_ERROR:       return "❌ 文件格式错误";
            case Status::REVOKED:           return "❌ 已被服务端吊销/禁用";
            case Status::REMOTE_FAIL:       return "❌ 远程验证失败且超离线宽限";
            default:                        return "未知";
        }
    };

    const char* LINE = "============================================================";
    std::cout << "\n" << GOLD << LINE << RESET << "\n";
    std::cout << GOLD << "  RuoYi-Cpp 许可证信息\n";
    std::cout << LINE << RESET << "\n";

    if (lic.status == Status::FILE_NOT_FOUND || lic.status == Status::PARSE_ERROR) {
        std::cout << "  状态    : " << stColor << statusStr(lic.status) << RESET << "\n";
    } else {
        std::cout << "  被授权方: " << lic.licensee   << "\n";
        std::cout << "  颁发日期: " << lic.issueDate  << "\n";
        std::cout << "  有效期  : " << lic.expireDate << "\n";
        if (lic.expireDate != "PERPETUAL") {
            if (lic.daysLeft > 0)
                std::cout << "  剩余天数: " << lic.daysLeft << " 天\n";
            else
                std::cout << "  已过期  : " << stColor << -lic.daysLeft << " 天前" << RESET << "\n";
        }
        std::cout << "  功能模块: " << lic.features   << "\n";
        if (lic.maxUsers > 0)
            std::cout << "  最大用户: " << lic.maxUsers << "\n";
        std::cout << "  状态    : " << stColor << statusStr(lic.status) << RESET << "\n";
    }
    std::cout << GOLD << LINE << RESET << "\n\n";
}

static inline void printFingerprintRequest() {
    try {
        auto fac = HardwareFingerprint::computeFactors();
        std::cout << "\n  请将以下信息发送给厂商以申请 license.lic：\n"
                  << "  fp_hash    : " << HardwareFingerprint::sha256hex(fac.full)    << "\n"
                  << "  fp_primary : " << HardwareFingerprint::sha256hex(fac.primary) << "\n"
                  << "  Machine GUID: " << fac.primary << "\n\n";
    } catch (...) {}
}

// ─── checkAndPrint：main.cc 一行调用入口 ────────────────────────────────
static inline void checkAndPrint() {
    auto lic = check();
    // 本地通过后做远程验证（lic 带 license_key 或配置了 license.remote.url 才生效）
    if (lic.status == Status::VALID ||
        lic.status == Status::EXPIRING_SOON ||
        lic.status == Status::HARDWARE_UPGRADED) {
        lic.status = remoteVerify(lic);
    }
    _revoked().store(lic.status == Status::REVOKED);
    _cached() = lic;           // 填充全局缓存供 hasFeature() 使用
    printPanel(lic);
    if (!isAllowedToStart(lic)) {
        std::cerr << "[License] 许可证无效，程序退出\n";
        if (lic.status == Status::FILE_NOT_FOUND ||
            lic.status == Status::HARDWARE_MISMATCH)
            printFingerprintRequest();
        if (lic.status == Status::REMOTE_FAIL)
            std::cerr << "[License] 提示：请确认能访问许可证服务端，或联系厂商检查离线宽限\n";
        std::exit(1);
    }
    if (lic.status == Status::EXPIRING_SOON && lic.daysLeft >= 0)
        std::cerr << "[License] 警告：许可证将在 " << lic.daysLeft << " 天后到期，请尽快续期\n";
}

} // namespace LicenseManager

// ─── 功能标志检查宏（在控制器中使用）──────────────────────────────────────
// 若许可证中不含该 feature，直接返回 403
#define CHECK_FEATURE(cb, feature) \
    if (!LicenseManager::hasFeature(feature)) { \
        auto _r = drogon::HttpResponse::newHttpJsonResponse( \
            AjaxResult::error(403, "当前许可证未授权功能: " feature)); \
        (cb)(_r); return; \
    }

// ─── i13: 许可证热加载监视器 ────────────────────────────────────────────
class LicenseWatcher {
public:
    static LicenseWatcher& instance() {
        static LicenseWatcher inst;
        return inst;
    }

    void start(const std::string& licPath = "license.lic") {
        if (running_.load()) return;
        licPath_ = licPath;
        running_.store(true);
        thread_ = std::thread([this]{ watch(); });   // joinable：stop() 时回收
        std::cout << "[LicenseWatcher] 监听许可证文件: " << licPath_ << "\n";
    }

    /// 周期心跳：服务端吊销后，intervalSec 内经 heartbeat 生效
    void heartbeatTick() {
        auto& lic = LicenseManager::_cached();
        if (lic.licenseKey.empty()) return;
        auto& cfg = LicenseManager::remoteCfg();
        auto now = std::chrono::steady_clock::now();
        if (lastBeat_ + std::chrono::seconds(cfg.intervalSec) > now) return;
        lastBeat_ = now;
        if (!LicenseManager::remoteHeartbeat(lic)) {
            std::cerr << "[LicenseWatcher] 远程心跳被拒绝：许可证已被吊销/禁用\n";
            LicenseManager::_revoked().store(true);   // 原子标志，不动共享的 lic.status
        }
    }

    void stop() {
        running_.store(false);
        if (thread_.joinable()) thread_.join();
    }
    ~LicenseWatcher() { stop(); }   // 兜底：防 joinable 线程析构触发 std::terminate

private:
    std::string       licPath_;
    std::atomic<bool> running_{false};
    std::thread       thread_;
    std::chrono::steady_clock::time_point lastBeat_{};

    void reload() {
        std::cout << "[LicenseWatcher] 检测到许可证变更，重新加载...\n";
        try {
            auto lic = LicenseManager::check();
            LicenseManager::_cached() = lic;
            // 热加载新 lic：本地验签通过则清吊销标志（管理员换证即恢复）
            LicenseManager::_revoked().store(false);
            LicenseManager::printPanel(lic);
            if (!LicenseManager::isAllowedToStart(lic))
                std::cerr << "[LicenseWatcher] 警告：新许可证无效，请检查\n";
            else
                std::cout << "[LicenseWatcher] 许可证热加载成功\n";
        } catch (const std::exception& e) {
            std::cerr << "[LicenseWatcher] 热加载异常: " << e.what() << "\n";
        }
    }

    void watch() {
#ifdef __linux__
        int fd = inotify_init1(IN_NONBLOCK);
        if (fd < 0) { pollFallback(); return; }
        std::string dir = ".";
        int wd = inotify_add_watch(fd, dir.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO);
        if (wd < 0) { close(fd); pollFallback(); return; }
        char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
        while (running_.load()) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                for (char* p = buf; p < buf + n; ) {
                    auto* ev = reinterpret_cast<struct inotify_event*>(p);
                    if (ev->len > 0) {
                        std::string name(ev->name);
                        if (licPath_.find(name) != std::string::npos || name == "license.lic")
                            reload();
                    }
                    p += sizeof(struct inotify_event) + ev->len;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            heartbeatTick();
        }
        close(fd);
#else
        pollFallback();
#endif
    }

    void pollFallback() {
        // 每 60s 轮询一次文件修改时间；睡眠按 1s 粒度切分，保证 stop() 后 join 秒级返回
        std::string path = LicenseManager::_licPath();
        time_t lastMod = 0;
        while (running_.load()) {
            for (int i = 0; i < 60 && running_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                heartbeatTick();   // 心跳 tick 也借此保持 1s 精度（内部按 intervalSec 限频）
            }
            if (!running_.load()) break;
#ifdef _WIN32
            FILETIME ft{};
            HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                FILETIME ct, at, wt;
                GetFileTime(h, &ct, &at, &wt);
                CloseHandle(h);
                time_t t = (((uint64_t)wt.dwHighDateTime << 32) | wt.dwLowDateTime) / 10000000ULL - 11644473600ULL;
                if (t != lastMod) { lastMod = t; reload(); }
            }
#else
            struct stat st{};
            if (stat(path.c_str(), &st) == 0 && st.st_mtime != lastMod) {
                lastMod = st.st_mtime;
                reload();
            }
#endif
        }
    }
};
