/**
 * @file HardwareId.h
 * @brief 硬件指纹工具 — 生成设备唯一标识
 * 
 * 功能概述：
 *   - 硬件指纹生成：基于硬件信息生成唯一的设备标识
 *   - 跨平台支持：支持 Windows 和 Linux
 *   - 防篡改：使用 SHA-256 哈希，难以伪造
 *   - 设备绑定：用于设备绑定和许可证验证
 * 
 * 硬件信息收集：
 *   Windows：
 *   - MachineGuid：从注册表 HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography 读取
 *   - CPU 品牌：使用 CPUID 指令读取 CPU 品牌字符串
 *   - 磁盘序列号：从 C: 盘获取卷序列号
 *   
 *   Linux：
 *   - MachineGuid：从 /etc/machine-id 读取
 *   - CPU 品牌：从 /proc/cpuinfo 读取 model name
 *   - 磁盘序列号：不支持（返回 0）
 * 
 * 指纹格式：
 *   - 输入：MachineGuid|CpuBrand|DiskSerial
 *   - 算法：SHA-256
 *   - 输出：64 个十六进制字符
 * 
 * 使用示例：
 *   // 生成硬件指纹
 *   std::string fingerprint = HardwareId::compute();
 *   std::cout << "Hardware ID: " << fingerprint << std::endl;
 *   
 *   // 获取单个硬件信息
 *   std::string guid = HardwareId::machineGuid();
 *   std::string cpu = HardwareId::cpuBrand();
 *   uint32_t disk = HardwareId::diskSerial();
 * 
 * 应用场景：
 *   - 设备绑定：将许可证绑定到特定设备
 *   - 防盗版：防止软件被复制到其他设备
 *   - 设备识别：唯一识别用户设备
 *   - 审计日志：记录操作来自哪个设备
 * 
 * 特性：
 *   - 跨平台：同时支持 Windows 和 Linux
 *   - 唯一性：基于多个硬件特征，难以重复
 *   - 稳定性：硬件不变时指纹保持不变
 *   - 安全性：使用 SHA-256 哈希，不可逆
 */

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

/**
 * @namespace HardwareId
 * @brief 硬件指纹工具命名空间
 * 
 * 提供硬件指纹生成功能，用于设备唯一标识。
 * 所有函数都是内联的，无需编译链接。
 */
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
