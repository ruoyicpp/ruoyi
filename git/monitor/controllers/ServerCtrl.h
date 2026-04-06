#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../filters/PermFilter.h"
#include <string>
#include <thread>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#else
#  include <fstream>
#  include <unistd.h>
#  include <sys/statvfs.h>
#endif

// 服务器信息接口 /monitor/server (跨平台: Windows MinGW + Linux)
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
        Json::Value cpu;
        int cores = (int)std::thread::hardware_concurrency();
        double used = getCpuUsage();
        double sys  = used * 0.3;           // 估算（粗略）
        double user = used - sys;
        cpu["cpuNum"] = cores;
        cpu["total"]  = 100.0;
        cpu["sys"]    = round2(sys);
        cpu["used"]   = round2(user);
        cpu["wait"]   = 0.0;
        cpu["free"]   = round2(100.0 - used);
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

        // ====== C++ 运行时信息（类比 JVM） ======
        Json::Value jvm;
        jvm["name"]    = "ruoyi-cpp (Drogon C++)";
        jvm["version"] = "C++17 / g++ " + std::string(__VERSION__);
        jvm["home"]    = getCurrentDir();
        jvm["startTime"] = fmtWallTime(startWall_);
        jvm["runTime"]   = fmtUptime();
        // 进程内存
        double procMB = getProcessMemMB();
        jvm["total"]   = round2(procMB) + " M";
        jvm["max"]     = "N/A";
        jvm["used"]    = round2(procMB) + " M";
        jvm["free"]    = "N/A";
        jvm["usage"]   = 0.0;
        jvm["inputArgs"] = "";
        server["jvm"] = jvm;

        // ====== 系统信息 ======
        Json::Value sys_info;
        sys_info["computerName"] = getHostname();
        sys_info["computerIp"]   = "127.0.0.1";
#ifdef _WIN32
        sys_info["osName"] = "Windows";
        sys_info["osArch"] = sizeof(void*) == 8 ? "x86_64" : "x86";
#else
        sys_info["osName"] = "Linux";
        sys_info["osArch"] = "x86_64";
#endif
        sys_info["userDir"] = getCurrentDir();
        server["sys"] = sys_info;

        // ====== 磁盘状态 ======
        server["sysFiles"] = getDiskInfo();

        RESP_OK(cb, server);
    }

private:
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

    double getCpuUsage() {
#ifdef _WIN32
        static FILETIME prevIdle{}, prevKernel{}, prevUser{};
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0;
        auto toULL = [](FILETIME ft) -> unsigned long long {
            return ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        };
        unsigned long long idleDiff   = toULL(idle)   - toULL(prevIdle);
        unsigned long long kernelDiff = toULL(kernel) - toULL(prevKernel);
        unsigned long long userDiff   = toULL(user)   - toULL(prevUser);
        prevIdle = idle; prevKernel = kernel; prevUser = user;
        unsigned long long total = kernelDiff + userDiff;
        if (total == 0) return 0.0;
        return 100.0 * (1.0 - (double)idleDiff / total);
#else
        std::ifstream f("/proc/loadavg");
        double load = 0;
        f >> load;
        return std::min(load * 10.0, 100.0);
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

    std::string getCurrentDir() {
#ifdef _WIN32
        char buf[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, buf);
        return buf;
#else
        char buf[1024] = {};
        getcwd(buf, sizeof(buf));
        return buf;
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
            double totalG = (double)totalBytes.QuadPart / 1024.0 / 1024.0 / 1024.0;
            double freeG  = (double)freeBytes.QuadPart  / 1024.0 / 1024.0 / 1024.0;
            double usedG  = totalG - freeG;
            if (totalG < 0.01) continue;  // 跳过极小驱动器
            Json::Value d;
            d["dirName"]     = root;
            d["sysTypeName"] = "NTFS";
            d["typeName"]    = "本地硬盘";
            d["total"]       = toGB(totalG);
            d["free"]        = toGB(freeG);
            d["used"]        = toGB(usedG);
            d["usage"]       = round2(usedG / totalG * 100.0);
            arr.append(d);
        }
#else
        // Linux 读取磁盘信息
        struct statvfs st;
        if (statvfs("/", &st) == 0) {
            double totalG = (double)st.f_blocks * st.f_frsize / 1024.0 / 1024.0 / 1024.0;
            double freeG  = (double)st.f_bavail * st.f_frsize / 1024.0 / 1024.0 / 1024.0;
            double usedG  = totalG - freeG;
            Json::Value d;
            d["dirName"]     = "/";
            d["sysTypeName"] = "ext4";
            d["typeName"]    = "本地硬盘";
            d["total"]       = toGB(totalG);
            d["free"]        = toGB(freeG);
            d["used"]        = toGB(usedG);
            d["usage"]       = round2(usedG / totalG * 100.0);
            arr.append(d);
        }
#endif
        return arr;
    }
};
