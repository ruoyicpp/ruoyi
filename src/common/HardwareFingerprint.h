// AI 轨迹片段重构恢复 - 来自 cb68a004 Steps 6215, 6269, 6451, 6476 等多次 edit
// 商业版核心文件：硬件指纹采集 + HMAC 完整性签名 + 注册表二级绑定 + 解锁码生成
// 注意：本文件原版有 6+ 次 multi_edit 演进，此处是按可见 API 表面与其他文件
//      （LicenseManager.h / DatabaseInit.h / tools/license_tool.cc）的引用反推重构。
//      关键算法如 RFC 6238 TOTP（unlock code）按标准实现。
#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <ctime>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <iphlpapi.h>
#  include <intrin.h>
#  pragma comment(lib, "iphlpapi.lib")
#else
#  include <unistd.h>
#  include <sys/statvfs.h>
#  include <ifaddrs.h>
#  include <net/if.h>
#  include <sys/socket.h>
#  include <fstream>
#  ifdef __linux__
#    include <linux/if_packet.h>
#  endif
#endif

namespace HardwareFingerprint {

// ── 内嵌 HMAC 密钥（混淆存储，防止简单字符串搜索）──────────
static inline std::string _embeddedKey() {
    static const uint8_t k[] = {
        0xB7,0x3C,0x91,0x2F,0x6E,0xA4,0x58,0xD1,
        0x0F,0x7B,0xC3,0x85,0x4A,0x2D,0xE6,0x19,
        0xF3,0x8C,0x56,0xAB,0x1E,0x74,0xD2,0x3F,
        0x92,0x6D,0xB0,0x47,0xCA,0x5E,0x83,0x10
    };
    return std::string(reinterpret_cast<const char*>(k), 32);
}

// ── 工具：hex 编码 ─────────────────────────────────────────
static inline std::string _hexOf(const unsigned char* d, size_t n) {
    std::ostringstream ss;
    for (size_t i = 0; i < n; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)d[i];
    return ss.str();
}
static inline std::string sha256hex(const std::string& data) {
    unsigned char h[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), h);
    return _hexOf(h, SHA256_DIGEST_LENGTH);
}
static inline std::string hmacHex(const std::string& data, const std::string& key) {
    unsigned char h[EVP_MAX_MD_SIZE];
    unsigned int  hlen = 0;
    HMAC(EVP_sha256(), key.data(), (int)key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         h, &hlen);
    return _hexOf(h, hlen);
}
static inline std::string generateSalt() {
    unsigned char buf[16];
    RAND_bytes(buf, 16);
    return _hexOf(buf, 16);
}

// ── hex 字符串校验（防 DB 篡改注入）───────────────────────
static inline bool _isHexStr(const std::string& s, size_t expectedLen = 0) {
    if (s.empty()) return false;
    if (expectedLen > 0 && s.size() != expectedLen) return false;
    for (char c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    return true;
}
// 常量时间字符串比较（防时序攻击）
static inline bool _constTimeEq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    volatile unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}

// ── 硬件信息采集 ──────────────────────────────────────────

#ifdef _WIN32
// 第一块非 loopback 物理网卡 MAC
static inline std::string getMacAddress() {
    ULONG bufLen = 15000;
    std::vector<uint8_t> buf(bufLen);
    auto* addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    ULONG flags = GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST |
                  GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addrs, &bufLen) != ERROR_SUCCESS)
        return "NOMAC";
    for (auto* p = addrs; p; p = p->Next) {
        if (p->IfType == IF_TYPE_ETHERNET_CSMACD &&
            p->OperStatus == IfOperStatusUp &&
            p->PhysicalAddressLength == 6) {
            std::ostringstream ss;
            for (ULONG i = 0; i < p->PhysicalAddressLength; ++i)
                ss << std::hex << std::setw(2) << std::setfill('0') << (int)p->PhysicalAddress[i];
            return ss.str();
        }
    }
    return "NOMAC";
}

static inline std::string getMachineGuid() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Cryptography",
                      0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return "NOGUID";
    char buf[64] = {};
    DWORD sz = sizeof(buf);
    RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr, (LPBYTE)buf, &sz);
    RegCloseKey(hKey);
    return std::string(buf);
}

static inline std::string getVolumeSerial() {
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char root[4] = { exePath[0], ':', '\\', '\0' };
    DWORD serial = 0;
    GetVolumeInformationA(root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
    std::ostringstream ss;
    ss << std::hex << std::uppercase << serial;
    return ss.str();
}

static inline std::string getCpuVendor() {
    int regs[4] = {};
    __cpuid(regs, 0);
    char vendor[13] = {};
    std::memcpy(vendor,     &regs[1], 4);
    std::memcpy(vendor + 4, &regs[3], 4);
    std::memcpy(vendor + 8, &regs[2], 4);
    return std::string(vendor, 12);
}

// ── 注册表二级绑定标记（防止删库重新启动绕过）─────────────
static inline std::string _bindMarkPath() {
    return std::string("Software\\RuoYiCpp\\Bind");
}
static inline void writeBindMark() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, _bindMarkPath().c_str(),
                        0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return;
    std::string mark = generateSalt();
    RegSetValueExA(hKey, "Mark", 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(mark.c_str()),
                   (DWORD)mark.size() + 1);
    RegCloseKey(hKey);
}
static inline bool verifyBindMark() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, _bindMarkPath().c_str(),
                      0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;
    char buf[64] = {};
    DWORD sz = sizeof(buf);
    LSTATUS s = RegQueryValueExA(hKey, "Mark", nullptr, nullptr, (LPBYTE)buf, &sz);
    RegCloseKey(hKey);
    return s == ERROR_SUCCESS && buf[0] != '\0';
}
static inline void clearBindMark() {
    RegDeleteKeyA(HKEY_CURRENT_USER, _bindMarkPath().c_str());
}

#else // POSIX

static inline std::string getMacAddress() {
#ifdef __linux__
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) return "NOMAC";
    std::string result = "NOMAC";
    for (auto* p = ifap; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_PACKET) continue;
        if (p->ifa_flags & IFF_LOOPBACK) continue;
        auto* sll = reinterpret_cast<struct sockaddr_ll*>(p->ifa_addr);
        if (sll->sll_halen == 6) {
            std::ostringstream ss;
            for (int i = 0; i < 6; ++i)
                ss << std::hex << std::setw(2) << std::setfill('0')
                   << (int)(unsigned char)sll->sll_addr[i];
            result = ss.str();
            break;
        }
    }
    freeifaddrs(ifap);
    return result;
#else
    return "NOMAC";
#endif
}
static inline std::string getMachineGuid() {
    std::ifstream f("/etc/machine-id");
    std::string id;
    if (f.is_open()) std::getline(f, id);
    return id.empty() ? "NOGUID" : id;
}
static inline std::string getVolumeSerial() {
    struct statvfs s {};
    if (statvfs("/", &s) != 0) return "0";
    std::ostringstream ss;
    ss << std::hex << s.f_fsid;
    return ss.str();
}
static inline std::string getCpuVendor() {
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("vendor_id", 0) == 0) {
            auto p = line.find(':');
            if (p != std::string::npos) return line.substr(p + 2);
        }
    }
    return "GENUINEUNKNOWN";
}
static inline void writeBindMark() {}
static inline bool verifyBindMark() { return false; }
static inline void clearBindMark() {}
#endif

// ── 复合指纹 ──────────────────────────────────────────────
struct Factors {
    std::string primary;   // Machine GUID（主因子）
    std::string mac;
    std::string volume;
    std::string cpu;
    std::string full;      // primary|mac|volume|cpu
};
static inline Factors computeFactors() {
    Factors f;
    f.primary = getMachineGuid();
    f.mac     = getMacAddress();
    f.volume  = getVolumeSerial();
    f.cpu     = getCpuVendor();
    f.full    = f.primary + "|" + f.mac + "|" + f.volume + "|" + f.cpu;
    return f;
}
static inline std::string compute() {
    return sha256hex(computeFactors().full);
}

// ── DB 中存储的指纹记录（带盐 + 完整性签名）────────────────
struct Record {
    std::string saltedHash;  // SHA256(fullFingerprint + salt)
    std::string salt;
    std::string integrity;   // HMAC(saltedHash|salt, embeddedKey)
};
static inline Record makeRecord() {
    auto f = computeFactors();
    Record r;
    r.salt       = generateSalt();
    r.saltedHash = sha256hex(f.full + r.salt);
    r.integrity  = hmacHex(r.saltedHash + "|" + r.salt, _embeddedKey());
    return r;
}
static inline bool verifyRecord(const Record& r) {
    if (!_isHexStr(r.saltedHash, 64) || !_isHexStr(r.salt, 32) ||
        !_isHexStr(r.integrity, 64))
        return false;
    std::string expectedIntegrity = hmacHex(r.saltedHash + "|" + r.salt, _embeddedKey());
    if (!_constTimeEq(expectedIntegrity, r.integrity)) return false;
    auto f = computeFactors();
    std::string current = sha256hex(f.full + r.salt);
    return _constTimeEq(current, r.saltedHash);
}

// ── 管理员解锁码：基于 RFC 6238 TOTP，1 天周期 ─────────────
// 每天有效一个 6 位数字解锁码，由 GUID 派生
static inline int generateUnlockCode(const std::string& guid) {
    // 时间步：以天为单位
    uint64_t T = (uint64_t)(std::time(nullptr) / 86400);
    std::string key = _embeddedKey() + ":unlock:" + guid;

    unsigned char msg[8];
    for (int i = 7; i >= 0; --i) {
        msg[i] = T & 0xFF;
        T >>= 8;
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hlen = 0;
    HMAC(EVP_sha1(), key.data(), (int)key.size(), msg, 8, hash, &hlen);

    int offset = hash[hlen - 1] & 0x0F;
    int code = ((hash[offset]     & 0x7F) << 24)
             | ((hash[offset + 1] & 0xFF) << 16)
             | ((hash[offset + 2] & 0xFF) <<  8)
             | ((hash[offset + 3] & 0xFF));
    return code % 1000000;
}
static inline bool verifyUnlockCode(const std::string& guid, int input) {
    return input == generateUnlockCode(guid);
}

} // namespace HardwareFingerprint
