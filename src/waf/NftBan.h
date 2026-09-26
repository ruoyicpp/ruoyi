/**
 * @file NftBan.h
 * @brief Linux nftables 内核层 IP 封禁 — 应用层之外的第二道防线
 *
 * 功能概述：
 *   - 内核层封禁：通过 nftables 在 netfilter 层直接 DROP 恶意 IP 的 SYN 包
 *   - 自动建表：首次使用时创建 ruoyi_waf inet 表 + blacklist set
 *   - 超时支持：nft set 元素支持 timeout，到期自动移除
 *   - 优雅降级：nft 命令不可用 / 无权限时静默降级为仅应用层封禁
 *
 * 平台说明：
 *   - 仅 Linux 有效（#ifdef __linux__ 隔离实现）
 *   - Windows 下所有方法为空操作（返回 false），保证可编译
 *   - 需要 root 或 CAP_NET_ADMIN 权限；普通用户运行时自动降级
 *
 * 实现方式：
 *   - 当前使用 nft CLI（system() 调用），零链接依赖
 *   - 后续可替换为 libmnl/libnftnl 原生 API（接口保持不变）
 *
 * 使用示例：
 *   NftBan::instance().init(true);
 *   NftBan::instance().banIp("1.2.3.4", 3600);   // 内核层封禁 1 小时
 *   NftBan::instance().unbanIp("1.2.3.4");
 *
 * 配置项（config.json → security.waf.nftables）：
 *   - enabled: 是否启用内核层封禁（默认 false）
 *   - table_name: nftables 表名（默认 ruoyi_waf）
 *   - set_name: 封禁集合名（默认 blacklist）
 */

#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <trantor/utils/Logger.h>
#include "../common/IpUtils.h"

/**
 * @class NftBan
 * @brief nftables 内核层封禁单例
 *
 * 把 WAF 判定的恶意 IP 同步到内核 netfilter，
 * 在 TCP 握手阶段直接丢弃，比应用层 403 更省资源。
 */
class NftBan {
public:
    static NftBan& instance() {
        static NftBan inst;
        return inst;
    }

    /**
     * @brief 初始化 nftables 封禁
     * @param enabled 是否启用
     * @param table nftables 表名
     * @param set 封禁集合名
     */
    void init(bool enabled, const std::string& table = "ruoyi_waf",
              const std::string& set = "blacklist") {
        enabled_ = enabled;
        table_ = table;
        set_ = set;
        // 表名/集合名会拼进 nft 命令，必须限定为安全标识符字符
        if (!validName(table_) || !validName(set_)) {
            LOG_WARN << "[NftBan] invalid table/set name, disabled: "
                     << table_ << "/" << set_;
            enabled_ = false;
            return;
        }
#ifdef __linux__
        if (!enabled_) return;
        available_ = setupTable();
        if (available_)
            LOG_INFO << "[NftBan] nftables ready: table=" << table_ << " set=" << set_;
        else
            LOG_WARN << "[NftBan] nft unavailable (need root/CAP_NET_ADMIN), "
                        "fallback to app-layer ban only";
#else
        if (enabled_)
            LOG_INFO << "[NftBan] skipped on non-Linux platform";
        enabled_ = false;
#endif
    }

    /**
     * @brief 内核层封禁 IP
     * @param ip IP 地址
     * @param seconds 封禁时长（秒），0 = 永久
     * @return 成功返回 true（不可用/无权限返回 false，调用方应降级应用层封禁）
     */
    bool banIp(const std::string& ip, int seconds) {
#ifdef __linux__
        if (!enabled_ || !available_) return false;
        // ip 会拼进 system() 命令行，先校验为合法 IP，防命令注入
        if (!IpUtils::isValidIp(ip)) {
            LOG_WARN << "[NftBan] reject invalid ip: " << ip;
            return false;
        }
        std::string cmd = "nft add element inet " + table_ + " " + set_ +
                          " { " + ip;
        if (seconds > 0) cmd += " timeout " + std::to_string(seconds) + "s";
        cmd += " } 2>/dev/null";
        bool ok = (std::system(cmd.c_str()) == 0);
        if (ok) {
            std::lock_guard<std::mutex> lk(mu_);
            banned_.push_back(ip);
            LOG_WARN << "[NftBan] kernel-banned: " << ip;
        }
        return ok;
#else
        (void)ip; (void)seconds;
        return false;
#endif
    }

    /// 解除内核层封禁
    bool unbanIp(const std::string& ip) {
#ifdef __linux__
        if (!enabled_ || !available_) return false;
        if (!IpUtils::isValidIp(ip)) return false;
        std::string cmd = "nft delete element inet " + table_ + " " + set_ +
                          " { " + ip + " } 2>/dev/null";
        return std::system(cmd.c_str()) == 0;
#else
        (void)ip;
        return false;
#endif
    }

    /// nftables 是否可用
    bool isAvailable() const { return available_; }

    /// 清空所有内核层封禁（服务停止时调用）
    void flushAll() {
#ifdef __linux__
        if (!enabled_ || !available_) return;
        std::string cmd = "nft flush set inet " + table_ + " " + set_ + " 2>/dev/null";
        std::system(cmd.c_str());
#endif
    }

private:
    NftBan() = default;

    /// nft 标识符校验：字母/数字/下划线，首字符非数字
    static bool validName(const std::string& s) {
        if (s.empty() || s.size() > 32) return false;
        if (std::isdigit((unsigned char)s[0])) return false;
        for (unsigned char c : s)
            if (!std::isalnum(c) && c != '_') return false;
        return true;
    }

#ifdef __linux__
    /// 创建 inet 表 + set + chain（幂等，已存在则跳过）
    bool setupTable() {
        // 检查 nft 命令存在
        if (std::system("command -v nft >/dev/null 2>&1") != 0) return false;

        std::string base = "inet " + table_;
        // 表 + 集合（带 timeout 支持）+ 链（input hook，drop 命中集合的包）
        std::string script =
            "nft add table " + base + " 2>/dev/null; "
            "nft add set " + base + " " + set_ +
            " { type ipv4_addr\\; flags timeout\\; } 2>/dev/null; "
            "nft add chain " + base + " input"
            " { type filter hook input priority -10\\; policy accept\\; } 2>/dev/null; "
            "nft add rule " + base + " input ip saddr @" + set_ +
            " counter drop 2>/dev/null";
        return std::system(script.c_str()) == 0;
    }
#endif

    bool        enabled_   = false;
    std::atomic<bool> available_{false};
    std::string table_ = "ruoyi_waf";
    std::string set_   = "blacklist";
    std::vector<std::string> banned_;
    std::mutex  mu_;
};
