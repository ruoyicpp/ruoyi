/**
 * @file CidrMatcher.h
 * @brief CIDR 网段匹配器 — IP 黑白名单的网段匹配
 *
 * 功能概述：
 *   - CIDR 解析：支持 "192.168.1.0/24"、"10.0.0.0/8" 格式
 *   - IPv4 匹配：位运算掩码匹配，O(1) 复杂度
 *   - IPv6 匹配：支持 "::1/128" 等 IPv6 CIDR
 *   - 精确匹配：无掩码时退化为精确 IP 匹配
 *   - 批量规则：支持多网段列表匹配（白名单/黑名单共用）
 *
 * 设计说明：
 *   - 自实现，不依赖 libcidr（CIDR 匹配本质是位运算，约 50 行）
 *   - 跨平台：纯标准 C++，Windows/Linux 均可用
 *   - 线程安全：所有方法无状态，可并发调用
 *
 * 使用示例：
 *   CidrMatcher matcher;
 *   matcher.addRule("192.168.0.0/16");
 *   matcher.addRule("10.0.0.0/8");
 *   if (matcher.matches("192.168.1.100")) {
 *       // 命中规则
 *   }
 */

#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <mutex>
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#endif

/**
 * @class CidrMatcher
 * @brief CIDR 网段匹配器
 *
 * 将 IP 地址与一组 CIDR 规则进行匹配。
 * 内部存储解析后的二进制地址 + 掩码位数，匹配时直接位运算。
 */
class CidrMatcher {
public:
    /**
     * @struct Rule
     * @brief 单条 CIDR 规则（解析后的二进制形式）
     */
    struct Rule {
        bool     isV6 = false;          ///< 是否 IPv6 规则
        uint32_t v4Addr = 0;            ///< IPv4 网络地址（网络字节序转主机序）
        uint8_t  v6Addr[16] = {};       ///< IPv6 网络地址
        int      prefixLen = 0;         ///< 掩码位数（0-32 / 0-128）
        std::string raw;                ///< 原始字符串（日志/回显用）
    };

    /**
     * @brief 添加一条 CIDR 规则
     *
     * 支持格式：
     *   - "192.168.1.0/24"  IPv4 CIDR
     *   - "192.168.1.1"     单 IP（等价 /32）
     *   - "::1/128"         IPv6 CIDR
     *   - "::1"             单 IPv6（等价 /128）
     *
     * @param cidr CIDR 字符串
     * @return 解析成功返回 true，格式错误返回 false
     */
    bool addRule(const std::string& cidr) {
        Rule r;
        r.raw = cidr;
        std::string ip = cidr;
        int prefix = -1;

        auto slash = cidr.find('/');
        if (slash != std::string::npos) {
            ip = cidr.substr(0, slash);
            try { prefix = std::stoi(cidr.substr(slash + 1)); }
            catch (...) { return false; }
        }

        // 尝试 IPv4
        struct in_addr a4;
        if (inet_pton(AF_INET, ip.c_str(), &a4) == 1) {
            r.isV6 = false;
            r.v4Addr = ntohl(a4.s_addr);
            r.prefixLen = (prefix < 0) ? 32 : prefix;
            if (r.prefixLen < 0 || r.prefixLen > 32) return false;
            // 掩码归零主机位，保证匹配正确
            r.v4Addr &= maskV4(r.prefixLen);
            std::lock_guard<std::mutex> lk(mu_);
            rules_.push_back(r);
            return true;
        }

        // 尝试 IPv6
        struct in6_addr a6;
        if (inet_pton(AF_INET6, ip.c_str(), &a6) == 1) {
            r.isV6 = true;
            memcpy(r.v6Addr, &a6, 16);
            r.prefixLen = (prefix < 0) ? 128 : prefix;
            if (r.prefixLen < 0 || r.prefixLen > 128) return false;
            applyMaskV6(r.v6Addr, r.prefixLen);
            std::lock_guard<std::mutex> lk(mu_);
            rules_.push_back(r);
            return true;
        }

        return false;
    }

    /**
     * @brief 批量添加规则（逗号或分号分隔）
     * @param list 如 "192.168.0.0/16,10.0.0.0/8;1.2.3.4"
     * @return 成功解析的规则数
     */
    int addRules(const std::string& list) {
        int ok = 0;
        size_t start = 0;
        while (start <= list.size()) {
            size_t end = list.find_first_of(",;", start);
            if (end == std::string::npos) end = list.size();
            std::string item = list.substr(start, end - start);
            // trim
            auto b = item.find_first_not_of(" \t");
            auto e = item.find_last_not_of(" \t");
            if (b != std::string::npos) {
                if (addRule(item.substr(b, e - b + 1))) ok++;
            }
            start = end + 1;
        }
        return ok;
    }

    /**
     * @brief 检查 IP 是否命中任一规则
     * @param ip IP 地址字符串（IPv4 或 IPv6）
     * @return 命中返回 true
     */
    bool matches(const std::string& ip) const {
        if (ip.empty()) return false;

        struct in_addr a4;
        if (inet_pton(AF_INET, ip.c_str(), &a4) == 1) {
            uint32_t addr = ntohl(a4.s_addr);
            std::lock_guard<std::mutex> lk(mu_);
            for (auto& r : rules_) {
                if (!r.isV6 && (addr & maskV4(r.prefixLen)) == r.v4Addr)
                    return true;
            }
            return false;
        }

        struct in6_addr a6;
        if (inet_pton(AF_INET6, ip.c_str(), &a6) == 1) {
            std::lock_guard<std::mutex> lk(mu_);
            for (auto& r : rules_) {
                if (r.isV6 && matchV6((const uint8_t*)&a6, r.v6Addr, r.prefixLen))
                    return true;
            }
            return false;
        }
        return false;
    }

    /// 清空所有规则
    void clear() {
        std::lock_guard<std::mutex> lk(mu_);
        rules_.clear();
    }

    /// 当前规则数
    size_t ruleCount() const {
        std::lock_guard<std::mutex> lk(mu_);
        return rules_.size();
    }

    /// 导出所有规则原始字符串（管理界面回显）
    std::vector<std::string> rules() const {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<std::string> out;
        out.reserve(rules_.size());
        for (auto& r : rules_) out.push_back(r.raw);
        return out;
    }

private:
    /// IPv4 掩码：prefixLen 位掩码（主机序）
    static uint32_t maskV4(int prefix) {
        return prefix == 0 ? 0 : (0xFFFFFFFFu << (32 - prefix));
    }

    /// IPv6 掩码归零主机位
    static void applyMaskV6(uint8_t addr[16], int prefix) {
        for (int i = 0; i < 16; i++) {
            int bits = prefix - i * 8;
            if (bits <= 0)      addr[i] = 0;
            else if (bits < 8)  addr[i] &= (uint8_t)(0xFF << (8 - bits));
        }
    }

    /// IPv6 前缀匹配
    static bool matchV6(const uint8_t addr[16], const uint8_t net[16], int prefix) {
        for (int i = 0; i < 16; i++) {
            int bits = prefix - i * 8;
            if (bits <= 0) return true;
            if (bits >= 8) {
                if (addr[i] != net[i]) return false;
            } else {
                uint8_t m = (uint8_t)(0xFF << (8 - bits));
                return (addr[i] & m) == (net[i] & m);
            }
        }
        return true;
    }

    mutable std::mutex mu_;
    std::vector<Rule>  rules_;
};
