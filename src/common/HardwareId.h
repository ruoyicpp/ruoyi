#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>

#ifdef _WIN32
#  include <windows.h>
#  include <intrin.h>
#else
#  include <fstream>
#endif

// Windows/Linux 硬件指纹：MachineGuid + CPU 品牌 + C: 盘序列号 → SHA-256 hex
namespace HardwareId {

#ifdef _WIN32
    inline std::string machineGuid() {
        HKEY hk;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hk) != ERROR_SUCCESS)
            return "";
        char buf[256] = {};
        DWORD sz = sizeof(buf);
        RegQueryValueExA(hk, "MachineGuid", nullptr, nullptr, (LPBYTE)buf, &sz);
        RegCloseKey(hk);
        return buf;
    }

    inline std::string cpuBrand() {
        int r[4] = {};
        char brand[49] = {};
        __cpuid(r, 0x80000002); memcpy(brand,    r, 16);
        __cpuid(r, 0x80000003); memcpy(brand+16, r, 16);
        __cpuid(r, 0x80000004); memcpy(brand+32, r, 16);
        return brand;
    }

    inline uint32_t diskSerial() {
        DWORD s = 0;
        GetVolumeInformationA("C:\\", nullptr, 0, &s, nullptr, nullptr, nullptr, 0);
        return s;
    }
#else
    inline std::string machineGuid() {
        std::ifstream f("/etc/machine-id");
        std::string s; std::getline(f, s); return s;
    }
    inline std::string cpuBrand() {
        std::ifstream f("/proc/cpuinfo");
        std::string line;
        while (std::getline(f, line))
            if (line.rfind("model name", 0) == 0) {
                auto p = line.find(':');
                if (p != std::string::npos) return line.substr(p + 2);
            }
        return "";
    }
    inline uint32_t diskSerial() { return 0; }
#endif

    // SHA-256(MachineGuid|CpuBrand|DiskSerial) → 64-char hex
    inline std::string compute() {
        std::string raw = machineGuid() + "|" + cpuBrand() + "|" + std::to_string(diskSerial());
        unsigned char hash[32];
        unsigned int  len = 32;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, raw.c_str(), raw.size());
        EVP_DigestFinal_ex(ctx, hash, &len);
        EVP_MD_CTX_free(ctx);
        std::ostringstream ss;
        for (auto b : hash) ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        return ss.str();
    }
}
