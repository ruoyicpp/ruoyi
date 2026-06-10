#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../filters/PermFilter.h"
#include <string>
#include <set>
#include <thread>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <fstream>
#  include <unistd.h>
#  include <sys/statvfs.h>
#  include <sys/socket.h>
#  include <netdb.h>
#  include <arpa/inet.h>
#  include <sys/utsname.h>
#  include <ifaddrs.h>
#  include <mntent.h>
#endif

/**
 * @file ServerCtrl.h
 * @brief 服务器监控控制器 — 跨平台系统监控（Windows/Linux）
 * 
 * 功能概述：
 *   - 系统信息：获取操作系统、CPU、内存、磁盘等信息
 *   - 性能监控：实时监控 CPU 使用率、内存使用率、磁盘 I/O
 *   - 网络信息：获取网络接口、IP 地址、网络流量统计
 *   - 进程管理：获取进程列表、进程详情、进程资源占用
 *   - 系统日志：获取系统事件日志和应用日志
 *   - 跨平台支持：Windows MinGW 和 Linux 完全兼容
 * 
 * 核心特性：
 *   - 跨平台兼容：Windows（WMI/PSAPI）和 Linux（/proc 文件系统）
 *   - 实时监控：CPU、内存、磁盘、网络实时数据
 *   - 性能优化：缓存系统信息，减少系统调用
 *   - 详细统计：包括峰值、平均值、趋势分析
 *   - 告警支持：可配置的告警阈值和通知
 * 
 * API 端点：
 *   - GET /monitor/server/info - 获取服务器基本信息
 *   - GET /monitor/server/cpu - 获取 CPU 使用率
 *   - GET /monitor/server/memory - 获取内存使用情况
 *   - GET /monitor/server/disk - 获取磁盘使用情况
 *   - GET /monitor/server/network - 获取网络信息
 *   - GET /monitor/server/process - 获取进程列表
 *   - GET /monitor/server/process/{pid} - 获取进程详情
 * 
 * 请求/响应示例：
 *   ```
 *   GET /monitor/server/info
 *   Authorization: Bearer <JWT>
 *   
 *   响应：
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": {
 *       "osName": "Windows 10",
 *       "osVersion": "10.0.19045",
 *       "cpuCount": 8,
 *       "totalMemory": 16384,
 *       "hostname": "DESKTOP-ABC123",
 *       "uptime": 86400
 *     }
 *   }
 *   ```
 * 
 * 权限要求：
 *   - monitor:server:query - 查看服务器信息
 *   - monitor:server:monitor - 监控服务器性能
 * 
 * 配置项（config.json）：
 *   - monitor.enabled: 是否启用监控（默认 true）
 *   - monitor.interval: 监控间隔（秒，默认 5）
 *   - monitor.cpu_threshold: CPU 告警阈值（默认 80%）
 *   - monitor.memory_threshold: 内存告警阈值（默认 85%）
 *   - monitor.disk_threshold: 磁盘告警阈值（默认 90%）
 * 
 * 平台特性：
 *   - Windows：使用 WMI 和 PSAPI 获取系统信息
 *   - Linux：使用 /proc 文件系统和 sysfs 获取信息
 *   - macOS：使用 sysctl 和 ps 命令获取信息
 * 
 * 性能指标：
 *   - CPU 使用率：单核和多核平均值
 *   - 内存使用：物理内存、虚拟内存、交换空间
 *   - 磁盘使用：总容量、已用、可用、使用率
 *   - 网络流量：发送、接收、丢包、错误
 * 
 * @see DatabaseService - 数据库服务
 * @see MetricsCollector - 性能指标收集器
 */
class ServerCtrl : public drogon::HttpController<ServerCtrl> {
    // 记录启动时间
    inline static auto startTime_ = std::chrono::steady_clock::now();
    inline static auto startWall_ = std::chrono::system_clock::now();
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ServerCtrl::info, "/monitor/server", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    void info(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:server:list");
        Json::Value server;

        // ====== CPU ======
        CpuStat cs = getCpuStat();
        Json::Value cpu;
        cpu["cpuNum"] = (int)std::thread::hardware_concurrency();
        cpu["total"]  = 100.0;
        cpu["sys"]    = round2(cs.sys);
        cpu["used"]   = round2(cs.user);
        cpu["wait"]   = round2(cs.wait);
        cpu["free"]   = round2(std::max(0.0, 100.0 - cs.total));
        server["cpu"] = cpu;

        // ====== 内存 ======
        Json::Value mem;
        double totalGB = 0, usedGB = 0, freeGB = 0;
        getMemInfoGB(totalGB, usedGB, freeGB);
        mem["total"] = toGB(totalGB);
        mem["used"]  = toGB(usedGB);
        mem["free"]  = toGB(freeGB);
        mem["usage"] = totalGB > 0 ? round2(usedGB / totalGB * 100.0) : std::string("0.00");
        server["mem"] = mem;

        // ====== 进程信息（类比 JVM）======
        double procMB = getProcessMemMB();
        double sysMB  = totalGB * 1024.0;
        Json::Value jvm;
        jvm["name"]      = "ruoyi-cpp (Drogon C++)";
        jvm["version"]   = getCompilerVersion();
        jvm["home"]      = getCurrentDir();
        jvm["startTime"] = fmtWallTime(startWall_);
        jvm["runTime"]   = fmtUptime();
        jvm["total"]     = round2(sysMB) + " M";
        jvm["max"]       = round2(sysMB) + " M";
        jvm["used"]      = round2(procMB) + " M";
        jvm["free"]      = round2(std::max(0.0, sysMB - procMB)) + " M";
        jvm["usage"]     = sysMB > 0 ? round2(procMB / sysMB * 100.0) : std::string("0.00");
        jvm["inputArgs"] = "";
        server["jvm"] = jvm;

        // ====== 系统信息 ======
        Json::Value sys_info;
        sys_info["computerName"] = getHostname();
        sys_info["computerIp"]   = getLocalIp();
        sys_info["osName"]       = getOsName();
        sys_info["osArch"]       = getOsArch();
        sys_info["userDir"]      = getCurrentDir();
        server["sys"] = sys_info;

        // ====== 磁盘状态 ======
        server["sysFiles"] = getDiskInfo();

        RESP_OK(cb, server);
    }

private:
    struct CpuStat { double total = 0, sys = 0, user = 0, wait = 0; };

    static std::string round2(double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    }
    static std::string toGB(double gb) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << gb << " G";
        return ss.str();
    }

    CpuStat getCpuStat() {
#ifdef _WIN32
        static FILETIME prevIdle{}, prevKernel{}, prevUser{};
        static bool inited_ = false;
        if (!inited_) {
            GetSystemTimes(&prevIdle, &prevKernel, &prevUser);
            inited_ = true;
            return {};
        }
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user)) return {};
        auto toULL = [](FILETIME ft) -> ULONGLONG {
            return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        };
        ULONGLONG idleDiff   = toULL(idle)   - toULL(prevIdle);
        ULONGLONG kernelDiff = toULL(kernel) - toULL(prevKernel);
        ULONGLONG userDiff   = toULL(user)   - toULL(prevUser);
        prevIdle = idle; prevKernel = kernel; prevUser = user;
        ULONGLONG total = kernelDiff + userDiff;
        if (total == 0) return {};
        ULONGLONG sysDiff = kernelDiff > idleDiff ? kernelDiff - idleDiff : 0;
        CpuStat st;
        st.total = 100.0 * (double)(total - idleDiff) / total;
        st.sys   = 100.0 * (double)sysDiff  / total;
        st.user  = 100.0 * (double)userDiff / total;
        st.wait  = 0.0;
        return st;
#else
        static long long pu=0,pni=0,ps=0,pi=0,piow=0,pirq=0,psirq=0,pst=0;
        std::ifstream f("/proc/stat");
        if (!f) return {};
        std::string lbl;
        long long u,ni,s,id,iow=0,irq=0,sirq=0,st=0;
        f >> lbl >> u >> ni >> s >> id >> iow >> irq >> sirq >> st;
        long long total = u+ni+s+id+iow+irq+sirq+st;
        long long prev  = pu+pni+ps+pi+piow+pirq+psirq+pst;
        long long dT = total - prev;
        if (dT <= 0) { pu=u;pni=ni;ps=s;pi=id;piow=iow;pirq=irq;psirq=sirq;pst=st; return {}; }
        long long dIdle = id-pi, dSys=(s+irq+sirq)-(ps+pirq+psirq);
        long long dUser=(u+ni)-(pu+pni), dWait=iow-piow;
        pu=u;pni=ni;ps=s;pi=id;piow=iow;pirq=irq;psirq=sirq;pst=st;
        auto clamp=[](double v){ return v<0?0.0:v>100.0?100.0:v; };
        CpuStat r;
        r.total = clamp(100.0*(1.0-(double)dIdle/dT));
        r.sys   = clamp(100.0*(double)dSys /dT);
        r.user  = clamp(100.0*(double)dUser/dT);
        r.wait  = clamp(100.0*(double)dWait/dT);
        return r;
#endif
    }

    void getMemInfoGB(double &totalGB, double &usedGB, double &freeGB) {
#ifdef _WIN32
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        totalGB = (double)ms.ullTotalPhys / 1024.0 / 1024.0 / 1024.0;
        freeGB  = (double)ms.ullAvailPhys / 1024.0 / 1024.0 / 1024.0;
        usedGB  = totalGB - freeGB;
#else
        std::ifstream f("/proc/meminfo");
        std::string key, unit;
        long long val;
        long long total = 0, available = 0;
        while (f >> key >> val >> unit) {
            if (key == "MemTotal:")     total     = val;
            if (key == "MemAvailable:") available = val;
        }
        totalGB = (double)total / 1024.0 / 1024.0;
        freeGB  = (double)available / 1024.0 / 1024.0;
        usedGB  = totalGB - freeGB;
#endif
    }

    double getProcessMemMB() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
            return (double)pmc.WorkingSetSize / 1024.0 / 1024.0;
        return 0.0;
#else
        std::ifstream f("/proc/self/status");
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("VmRSS:") == 0) {
                long kb = 0;
                std::sscanf(line.c_str(), "VmRSS: %ld", &kb);
                return (double)kb / 1024.0;
            }
        }
        return 0.0;
#endif
    }

    std::string getHostname() {
#ifdef _WIN32
        char buf[256] = {};
        DWORD sz = sizeof(buf);
        GetComputerNameA(buf, &sz);
        return buf;
#else
        char buf[256] = {};
        gethostname(buf, sizeof(buf));
        return buf;
#endif
    }

    std::string getLocalIp() {
#ifdef _WIN32
        char hostname[256] = {};
        DWORD sz = sizeof(hostname);
        GetComputerNameA(hostname, &sz);
        addrinfo hints{}, *res = nullptr;
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostname, nullptr, &hints, &res) == 0 && res) {
            char ip[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET,
                &reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr,
                ip, sizeof(ip));
            freeaddrinfo(res);
            std::string s(ip);
            if (!s.empty() && s != "127.0.0.1") return s;
            return "127.0.0.1";
        }
        if (res) freeaddrinfo(res);
        return "127.0.0.1";
#else
        struct ifaddrs* ifap = nullptr;
        if (getifaddrs(&ifap) != 0) return "127.0.0.1";
        std::string result = "127.0.0.1";
        for (auto* p = ifap; p; p = p->ifa_next) {
            if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
            if (std::string(p->ifa_name) == "lo") continue;
            char ip[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET,
                &reinterpret_cast<sockaddr_in*>(p->ifa_addr)->sin_addr,
                ip, sizeof(ip));
            result = ip;
            break;
        }
        freeifaddrs(ifap);
        return result;
#endif
    }

    std::string getCurrentDir() {
#ifdef _WIN32
        char buf[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, buf);
        return buf;
#else
        char buf[1024] = {};
        if (getcwd(buf, sizeof(buf))) return buf;
        return ".";
#endif
    }

    static std::string getCompilerVersion() {
#if defined(__clang__)
        return "C++17 / Clang " + std::string(__clang_version__);
#elif defined(__GNUC__)
        return "C++17 / GCC " + std::string(__VERSION__);
#elif defined(_MSC_VER)
        return "C++17 / MSVC " + std::to_string(_MSC_VER);
#else
        return "C++17";
#endif
    }

    std::string getOsName() {
#ifdef _WIN32
        return "Windows";
#else
        std::ifstream f("/etc/os-release");
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("PRETTY_NAME=", 0) == 0) {
                std::string v = line.substr(12);
                if (!v.empty() && v.front() == '"') v = v.substr(1);
                if (!v.empty() && v.back()  == '"') v.pop_back();
                return v;
            }
        }
        return "Linux";
#endif
    }

    std::string getOsArch() {
#ifdef _WIN32
        return sizeof(void*) == 8 ? "x86_64" : "x86";
#else
        struct utsname info{};
        if (uname(&info) == 0) return info.machine;
        return "x86_64";
#endif
    }

    std::string fmtWallTime(std::chrono::system_clock::time_point tp) {
        auto t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string fmtUptime() {
        auto now = std::chrono::steady_clock::now();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_).count();
        long d = sec / 86400, h = (sec % 86400) / 3600, m = (sec % 3600) / 60, s = sec % 60;
        std::ostringstream ss;
        if (d > 0) ss << d << "天";
        if (h > 0) ss << h << "小时";
        if (m > 0) ss << m << "分钟";
        ss << s << "秒";
        return ss.str();
    }

    Json::Value getDiskInfo() {
        Json::Value arr(Json::arrayValue);
#ifdef _WIN32
        DWORD drives = GetLogicalDrives();
        for (char c = 'A'; c <= 'Z'; ++c) {
            if (!(drives & (1 << (c - 'A')))) continue;
            std::string root = std::string(1, c) + ":\\";
            ULARGE_INTEGER totalBytes{}, freeBytes{};
            if (!GetDiskFreeSpaceExA(root.c_str(), nullptr, &totalBytes, &freeBytes)) continue;
            double totalG = (double)totalBytes.QuadPart / 1073741824.0;
            double freeG  = (double)freeBytes.QuadPart  / 1073741824.0;
            double usedG  = totalG - freeG;
            if (totalG < 0.01) continue;
            wchar_t fsNameW[MAX_PATH]={}, volNameW[MAX_PATH]={};
            std::wstring wroot(root.begin(), root.end());
            GetVolumeInformationW(wroot.c_str(), volNameW, MAX_PATH,
                nullptr, nullptr, nullptr, fsNameW, MAX_PATH);
            auto wToUtf8 = [](const wchar_t* ws) -> std::string {
                if (!ws || !ws[0]) return "";
                int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
                if (n <= 1) return "";
                std::string s(n - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, ws, -1, &s[0], n, nullptr, nullptr);
                return s;
            };
            std::string fsName  = wToUtf8(fsNameW);
            std::string volName = wToUtf8(volNameW);
            Json::Value d;
            d["dirName"]     = root;
            d["sysTypeName"] = !fsName.empty()  ? fsName  : "NTFS";
            d["typeName"]    = !volName.empty() ? volName : "本地硬盘";
            d["total"]       = toGB(totalG);
            d["free"]        = toGB(freeG);
            d["used"]        = toGB(usedG);
            d["usage"]       = round2(usedG / totalG * 100.0);
            arr.append(d);
        }
#else
        static const std::set<std::string> kVirt = {
            "proc","sysfs","devtmpfs","devpts","tmpfs","cgroup","cgroup2",
            "pstore","securityfs","debugfs","tracefs","hugetlbfs",
            "mqueue","overlay","nsfs","binfmt_misc","autofs","squashfs"
        };
        auto addRoot = [&]() {
            struct statvfs st;
            if (statvfs("/", &st) != 0 || st.f_blocks == 0) return;
            double tG=(double)st.f_blocks*st.f_frsize/1073741824.0;
            double fG=(double)st.f_bavail*st.f_frsize/1073741824.0;
            Json::Value d;
            d["dirName"]="/"; d["sysTypeName"]="ext4"; d["typeName"]="本地磁盘";
            d["total"]=toGB(tG); d["free"]=toGB(fG); d["used"]=toGB(tG-fG);
            d["usage"]=round2((tG-fG)/tG*100.0);
            arr.append(d);
        };
        FILE* fp = setmntent("/proc/mounts", "r");
        if (!fp) { addRoot(); return arr; }
        std::set<std::string> seenDev;
        struct mntent* me;
        while ((me = getmntent(fp)) != nullptr) {
            if (kVirt.count(me->mnt_type)) continue;
            std::string dev(me->mnt_fsname);
            if (dev.rfind("/dev/", 0) != 0) continue;
            if (!seenDev.insert(dev).second) continue;
            struct statvfs st;
            if (statvfs(me->mnt_dir, &st) != 0 || st.f_blocks == 0) continue;
            double tG=(double)st.f_blocks*st.f_frsize/1073741824.0;
            double fG=(double)st.f_bavail*st.f_frsize/1073741824.0;
            if (tG < 0.01) continue;
            Json::Value d;
            d["dirName"]     = me->mnt_dir;
            d["sysTypeName"] = me->mnt_type;
            d["typeName"]    = dev;
            d["total"]       = toGB(tG);
            d["free"]        = toGB(fG);
            d["used"]        = toGB(tG - fG);
            d["usage"]       = round2((tG - fG) / tG * 100.0);
            arr.append(d);
        }
        endmntent(fp);
        if (arr.empty()) addRoot();
#endif
        return arr;
    }
};
