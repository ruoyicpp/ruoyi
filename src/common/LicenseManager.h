// AI 轨迹片段重构恢复 - 来自 cb68a004 Steps 6283, 6466, 6768, 6819 等多次迭代
// 商业版核心：企业级许可证管理器
// license.lic 放在可执行文件同目录，格式：key=value，最后一行 sig=<HMAC-SHA256>
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
#include <atomic>
#include <chrono>
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/inotify.h>
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
    char buf[11];
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

// ─── 生成 license.lic 内容（厂商侧工具使用）────────────────────────────
static inline std::string generate(
        const std::string& licensee,
        const std::string& fpHash,
        const std::string& fpPrimary,
        const std::string& expireDate = "PERPETUAL",
        int maxUsers = 0,
        const std::string& features = "FULL",
        int graceDays = 7) {
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

    return body + "sig=" + info.signature + "\n";
}

// ─── 启动校验 ────────────────────────────────────────────────────────────
static inline bool isAllowedToStart(const LicenseInfo& lic) {
#ifdef RUOYI_DEV_BYPASS_LICENSE
    // 开发模式：CMake 通过 -DRUOYI_DEV_BYPASS_LICENSE=ON 启用，绕过许可证校验
    (void)lic;
    return true;
#else
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
    _cached() = lic;           // 填充全局缓存供 hasFeature() 使用
    printPanel(lic);
    if (!isAllowedToStart(lic)) {
        std::cerr << "[License] 许可证无效，程序退出\n";
        if (lic.status == Status::FILE_NOT_FOUND ||
            lic.status == Status::HARDWARE_MISMATCH)
            printFingerprintRequest();
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
        thread_ = std::thread([this]{ watch(); });
        thread_.detach();
        std::cout << "[LicenseWatcher] 监听许可证文件: " << licPath_ << "\n";
    }

    void stop() { running_.store(false); }

private:
    std::string       licPath_;
    std::atomic<bool> running_{false};
    std::thread       thread_;

    void reload() {
        std::cout << "[LicenseWatcher] 检测到许可证变更，重新加载...\n";
        try {
            auto lic = LicenseManager::check();
            LicenseManager::_cached() = lic;
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
        }
        close(fd);
#else
        pollFallback();
#endif
    }

    void pollFallback() {
        // 每 60s 轮询一次文件修改时间
        std::string path = LicenseManager::_licPath();
        time_t lastMod = 0;
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
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
