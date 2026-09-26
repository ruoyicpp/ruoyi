/**
 * @file WafEngine.h
 * @brief WAF 引擎 — HTTP 请求规则匹配与拦截
 *
 * 功能概述：
 *   - 规则引擎：基于正则表达式的请求特征匹配（URI/UA/Body/Args）
 *   - 内置规则：SQL 注入、XSS、路径穿越、扫描器特征、命令注入
 *   - 自定义规则：config.json 可追加自定义正则规则
 *   - 动作策略：log（仅记录）/ block（拦截 403）/ ban（拦截并封禁 IP）
 *   - 拦截日志：NDJSON 落盘 logs/waf/，供检索与控制台查询
 *   - 防 ReDoS：Linux 下可用 RE2（宏 RUOYI_WAF_RE2），默认 std::regex + 长度截断
 *
 * 设计说明：
 *   - 检查顺序：IP 白名单 → IP 黑名单(CIDR) → 静态规则匹配 → 放行
 *   - 性能：规则预编译，单请求检查 < 0.5ms；Body 仅检查前 8KB
 *   - 与现有 XssUtils 互补：XssUtils 做输入净化，WafEngine 做请求拦截
 *   - Windows 编译：全部代码跨平台，RE2 仅 Linux 启用
 *
 * 使用示例：
 *   // main.cc 初始化
 *   WafEngine::instance().init(cfg["security"]["waf"]);
 *   // 过滤器中调用
 *   auto verdict = WafEngine::instance().inspect(req);
 *   if (verdict.action == WafAction::Block) { 返回 403; }
 *
 * 配置项（config.json → security.waf）：
 *   - enabled: 总开关（默认 false）
 *   - mode: "log" | "block"（默认 log，先观察再拦截）
 *   - ban_on_hit: 命中后是否自动封禁 IP（默认 false）
 *   - ban_seconds: 自动封禁时长（默认 3600）
 *   - max_body_check: Body 检查长度上限（默认 8192）
 *   - whitelist_cidrs / blacklist_cidrs: CIDR 名单
 *   - custom_rules: [{name, pattern, target, action}]
 */

#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <regex>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <fstream>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <trantor/utils/Logger.h>
#include "CidrMatcher.h"
#include "RiskStore.h"
#include "NftBan.h"
#include "../common/IpUtils.h"
#include "../audit/AuditQueue.h"

#ifdef RUOYI_WAF_RE2
#  include <re2/re2.h>
#endif

/// WAF 判定动作
enum class WafAction {
    Pass,    ///< 放行
    Log,     ///< 放行但记录日志
    Block,   ///< 拦截（403）
    Ban,     ///< 拦截并封禁 IP
    Captcha  ///< 跳转验证码（返回 449 + captcha_url，前端引导验证）
};

/// WAF 检查结果
struct WafVerdict {
    WafAction   action = WafAction::Pass;
    std::string ruleName;   ///< 命中的规则名
    std::string matched;    ///< 命中的内容片段（截断）
    std::string target;     ///< 命中的目标（uri/ua/body/args）
};

/**
 * @class WafEngine
 * @brief WAF 引擎单例
 *
 * 在请求进入业务逻辑前做规则匹配，识别常见攻击特征。
 */
class WafEngine {
public:
    static WafEngine& instance() {
        static WafEngine inst;
        return inst;
    }

    /// 规则匹配目标
    enum class Target { Uri, UserAgent, Args, Body, Header };

    struct Rule {
        std::string name;
        std::string pattern;
        Target      target  = Target::Uri;
        WafAction   action  = WafAction::Block;
        bool        enabled = true;   ///< 单规则开关（管理 API 可动态启停）
#ifdef RUOYI_WAF_RE2
        std::unique_ptr<RE2> re2;
#else
        std::regex  re;
#endif
    };

    /**
     * @brief 初始化 WAF 引擎
     * @param cfg config.json 的 security.waf 节点
     * @note 仅 Linux 版本可用；其他平台直接跳过（enabled_ 保持 false）
     */
    void init(const Json::Value& cfg) {
#ifndef __linux__
        (void)cfg;
        LOG_INFO << "[WAF] skipped: feature only available on Linux build";
        return;
#endif
        enabled_    = cfg.get("enabled", false).asBool();
        blockMode_  = cfg.get("mode", "log").asString() == "block";
        banOnHit_   = cfg.get("ban_on_hit", false).asBool();
        banSecs_    = cfg.get("ban_seconds", 3600).asInt();
        maxBody_    = cfg.get("max_body_check", 8192).asInt();
        logDir_     = cfg.get("log_dir", "logs/waf").asString();
        captchaUrl_ = cfg.get("captcha_url", "").asString();

        // 热重载幂等：清空旧规则和名单再重建（防重复追加）
        {
            std::unique_lock<std::shared_mutex> lk(mu_);
            rules_.clear();
        }
        {
            std::unique_lock<std::shared_mutex> lk(listMu_);
            uriWhitelist_.clear();
            uriBlacklist_.clear();
            uaBlacklist_.clear();
            whitelist_.clear();   // CIDR 名单也要清，否则每次 reload 重复追加
            blacklist_.clear();
        }

        // URI 白名单（跳过 WAF 检查，如健康检查/静态资源前缀）
        for (auto& u : cfg["uri_whitelist"]) uriWhitelist_.insert(u.asString());
        // URI 黑名单（直接拦截，支持 * 后缀前缀匹配）
        for (auto& u : cfg["uri_blacklist"]) uriBlacklist_.push_back(u.asString());
        // UA 黑名单（子串匹配，命中即拦）
        for (auto& u : cfg["ua_blacklist"])   uaBlacklist_.push_back(u.asString());

        if (!enabled_) {
            LOG_INFO << "[WAF] disabled";
            return;
        }

        // CIDR 名单
        for (auto& c : cfg["whitelist_cidrs"])
            whitelist_.addRule(c.asString());
        for (auto& c : cfg["blacklist_cidrs"])
            blacklist_.addRule(c.asString());

        loadBuiltinRules();

        // 自定义规则
        for (auto& r : cfg["custom_rules"]) {
            addRule(r.get("name", "custom").asString(),
                    r.get("pattern", "").asString(),
                    parseTarget(r.get("target", "uri").asString()),
                    parseAction(r.get("action", "block").asString()));
        }

        // 建日志目录
        std::string cmd = "mkdir -p \"" + logDir_ + "\"";
#ifdef _WIN32
        cmd = "if not exist \"" + logDir_ + "\" mkdir \"" + logDir_ + "\"";
#endif
        std::system(cmd.c_str());

        LOG_INFO << "[WAF] enabled mode=" << (blockMode_ ? "block" : "log")
                 << " rules=" << rules_.size()
                 << " wl_cidrs=" << whitelist_.ruleCount()
                 << " bl_cidrs=" << blacklist_.ruleCount();
    }

    /**
     * @brief 检查请求（在每个请求入口调用）
     * @param req Drogon 请求对象
     * @return 判定结果
     */
    WafVerdict inspect(const drogon::HttpRequestPtr& req) {
        WafVerdict v;
        if (!enabled_) return v;

        std::string ip = IpUtils::getIpAddr(req);

        std::string uri = req->path();
        std::string ua  = req->getHeader("User-Agent");

        // 1. IP 白名单 / URI 白名单直接放行
        if (whitelist_.matches(ip)) return v;
        {
            std::shared_lock<std::shared_mutex> lk(listMu_);
            for (auto& p : uriWhitelist_)
                if (uri.rfind(p, 0) == 0) return v;   // 前缀匹配
        }

        // 2. IP 黑名单 / 风控封禁 / URI 黑名单 / UA 黑名单 → 直接拦截
        if (blacklist_.matches(ip) || RiskStore::instance().isBanned(ip)) {
            v.action = WafAction::Block;
            v.ruleName = "ip_blacklist";
            v.target = "ip";
            v.matched = ip;
            logHit(req, v, ip);
            return v;
        }
        {
            std::shared_lock<std::shared_mutex> lk(listMu_);
            for (auto& p : uriBlacklist_) {
                // 支持 "xxx*" 前缀匹配和精确匹配
                if (!p.empty() && p.back() == '*') {
                    if (uri.rfind(p.substr(0, p.size() - 1), 0) == 0) {
                        v.action = WafAction::Block; v.ruleName = "uri_blacklist";
                        v.target = "uri"; v.matched = uri;
                        logHit(req, v, ip); statsHits_++;
                        return v;
                    }
                } else if (uri == p) {
                    v.action = WafAction::Block; v.ruleName = "uri_blacklist";
                    v.target = "uri"; v.matched = uri;
                    logHit(req, v, ip); statsHits_++;
                    return v;
                }
            }
            for (auto& pat : uaBlacklist_) {
                if (!pat.empty() && ua.find(pat) != std::string::npos) {
                    v.action = WafAction::Block; v.ruleName = "ua_blacklist";
                    v.target = "ua"; v.matched = ua.substr(0, 128);
                    logHit(req, v, ip); statsHits_++;
                    return v;
                }
            }
        }

        // 3. 规则匹配（shared_lock：多读并发，addRule 时独占）
        // Args/Body 先 URL 解码再匹配——否则 union%20select / union+select 直接绕过
        std::string args = urlDecode(req->query());
        auto bs = req->body().substr(0, (size_t)maxBody_);
        std::string body = urlDecode(std::string(bs.data(), bs.size()));

        std::shared_lock<std::shared_mutex> rl(mu_);
        for (auto& r : rules_) {
            if (!r.enabled) continue;   // 单规则开关
            const std::string* text = nullptr;
            switch (r.target) {
                case Target::Uri:       text = &uri;  break;
                case Target::UserAgent: text = &ua;   break;
                case Target::Args:      text = &args; break;
                case Target::Body:      text = &body; break;
                default: continue;
            }
            if (text->empty()) continue;

            if (matchRule(r, *text, v.matched)) {
                v.ruleName = r.name;
                v.target   = targetName(r.target);
                v.action   = blockMode_ ? r.action : WafAction::Log;

                logHit(req, v, ip);
                statsHits_++;

                // 命中后自动封禁（Captcha 动作不封禁，只跳转验证）
                if (banOnHit_ && v.action != WafAction::Log &&
                    v.action != WafAction::Captcha) {
                    RiskStore::instance().ban(ip, banSecs_, "waf:" + r.name);
                    // 同步内核层封禁（nftables 未启用/无权限时静默降级）
                    NftBan::instance().banIp(ip, banSecs_);
                    v.action = WafAction::Ban;
                }
                return v;
            }
        }
        statsPass_++;
        return v;
    }

    /// 统计
    uint64_t hits() const { return statsHits_.load(); }
    uint64_t passed() const { return statsPass_.load(); }
    bool isEnabled() const { return enabled_; }

    /// 动态添加规则（管理 API 用）
    bool addRule(const std::string& name, const std::string& pattern,
                 Target target, WafAction action) {
        try {
            Rule r;
            r.name = name; r.pattern = pattern;
            r.target = target; r.action = action;
#ifdef RUOYI_WAF_RE2
            r.re2 = std::make_unique<RE2>(pattern);
            if (!r.re2->ok()) return false;
#else
            r.re = std::regex(pattern, std::regex::icase | std::regex::optimize);
#endif
            std::unique_lock<std::shared_mutex> lk(mu_);
            rules_.push_back(std::move(r));
            return true;
        } catch (...) { return false; }
    }

    /// 规则数量
    size_t ruleCount() const {
        std::shared_lock<std::shared_mutex> lk(mu_);
        return rules_.size();
    }

    /// 规则信息（管理 API 列表用）
    struct RuleInfo {
        std::string name, pattern, target, action;
        bool enabled;
    };
    /// 列出所有规则
    std::vector<RuleInfo> listRules() const {
        std::shared_lock<std::shared_mutex> lk(mu_);
        std::vector<RuleInfo> out;
        out.reserve(rules_.size());
        for (auto& r : rules_)
            out.push_back({r.name, r.pattern, targetName(r.target),
                           actionName(r.action), r.enabled});
        return out;
    }

    /// 启用/禁用单条规则
    bool setRuleEnabled(const std::string& name, bool enabled) {
        std::unique_lock<std::shared_mutex> lk(mu_);
        for (auto& r : rules_) {
            if (r.name == name) { r.enabled = enabled; return true; }
        }
        return false;
    }

    /// 删除规则（内置规则也可删）
    bool removeRule(const std::string& name) {
        std::unique_lock<std::shared_mutex> lk(mu_);
        auto it = std::find_if(rules_.begin(), rules_.end(),
                               [&](const Rule& r){ return r.name == name; });
        if (it == rules_.end()) return false;
        rules_.erase(it);
        return true;
    }

    // ── URI/UA 名单管理（管理 API 用）──────────────────────────────────
    void addUriRule(bool white, const std::string& uri) {
        std::unique_lock<std::shared_mutex> lk(listMu_);
        if (white) uriWhitelist_.insert(uri);
        else       uriBlacklist_.push_back(uri);
    }
    bool removeUriRule(bool white, const std::string& uri) {
        std::unique_lock<std::shared_mutex> lk(listMu_);
        if (white) return uriWhitelist_.erase(uri) > 0;
        auto& v = uriBlacklist_;
        auto it = std::find(v.begin(), v.end(), uri);
        if (it == v.end()) return false;
        v.erase(it); return true;
    }
    void addUaRule(const std::string& ua) {
        std::unique_lock<std::shared_mutex> lk(listMu_);
        uaBlacklist_.push_back(ua);
    }
    bool removeUaRule(const std::string& ua) {
        std::unique_lock<std::shared_mutex> lk(listMu_);
        auto& v = uaBlacklist_;
        auto it = std::find(v.begin(), v.end(), ua);
        if (it == v.end()) return false;
        v.erase(it); return true;
    }
    std::vector<std::string> uriWhitelist() const {
        std::shared_lock<std::shared_mutex> lk(listMu_);
        return {uriWhitelist_.begin(), uriWhitelist_.end()};
    }
    std::vector<std::string> uriBlacklist() const {
        std::shared_lock<std::shared_mutex> lk(listMu_);
        return uriBlacklist_;
    }
    std::vector<std::string> uaBlacklist() const {
        std::shared_lock<std::shared_mutex> lk(listMu_);
        return uaBlacklist_;
    }

    /// 验证码跳转地址（Captcha 动作用）
    const std::string& captchaUrl() const { return captchaUrl_; }

    CidrMatcher& whitelist() { return whitelist_; }
    CidrMatcher& blacklist() { return blacklist_; }

private:
    WafEngine() = default;

    /// 内置规则集（OWASP 常见攻击特征）
    void loadBuiltinRules() {
        struct Builtin { const char* name; const char* pat; Target t; };
        static const Builtin builtins[] = {
            // SQL 注入（Args + Body：POST JSON 是主要攻击面）
            {"sqli_union",   "(union\\s+(all\\s+)?select)",        Target::Args},
            {"sqli_select",  "(select\\s+.+\\s+from\\s+)",         Target::Args},
            {"sqli_comment", "('(\\s|/\\*|--)|(;\\s*drop\\s))",    Target::Args},
            {"sqli_sleep",   "(sleep\\s*\\(|benchmark\\s*\\()",    Target::Args},
            {"sqli_union_b",  "(union\\s+(all\\s+)?select)",       Target::Body},
            {"sqli_sleep_b",  "(sleep\\s*\\(|benchmark\\s*\\()",   Target::Body},
            // XSS（Args + Body）
            {"xss_script",   "(<script|javascript:|on(error|load|click)\\s*=)", Target::Args},
            {"xss_script_b", "(<script|javascript:|on(error|load|click)\\s*=)", Target::Body},
            // 路径穿越
            {"path_traversal", "(\\.\\./|\\.\\.\\\\|%2e%2e)",      Target::Uri},
            // 命令注入
            {"cmd_inject",   "(;\\s*(cat|ls|id|whoami|wget|curl)\\s|`[^`]+`|\\$\\()", Target::Args},
            // 扫描器特征 UA
            {"scanner_ua",   "(sqlmap|nmap|nikto|acunetix|nessus|masscan|hydra|dirbuster|gobuster)", Target::UserAgent},
            // 敏感文件探测
            {"sensitive_file", "(\\.env|\\.git/|wp-config|/\\.svn/|/phpmyadmin)", Target::Uri},
            // Webshell 上传特征
            {"webshell",     "(eval\\s*\\(|assert\\s*\\(|base64_decode\\s*\\()", Target::Body},
        };
        for (auto& b : builtins)
            addRule(b.name, b.pat, b.t, WafAction::Block);
    }

    /// URL 解码（%XX 和 + 转空格）；解码失败的原样保留
    static std::string urlDecode(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '%' && i + 2 < s.size()) {
                int hi = hexVal(s[i+1]), lo = hexVal(s[i+2]);
                if (hi >= 0 && lo >= 0) {
                    out += (char)((hi << 4) | lo);
                    i += 2;
                    continue;
                }
            }
            out += (c == '+') ? ' ' : c;
        }
        return out;
    }

    /// 执行单条规则匹配
    bool matchRule(const Rule& r, const std::string& text, std::string& matched) {
#ifdef RUOYI_WAF_RE2
        // PartialMatch 直接传输出参数指针（内部自动包 Arg），不能传 Arg*
        if (RE2::PartialMatch(text, *r.re2, &matched)) {
            if (matched.size() > 128) matched.resize(128);
            return true;
        }
        return false;
#else
        std::smatch sm;
        if (std::regex_search(text, sm, r.re)) {
            matched = sm.str().substr(0, 128);
            return true;
        }
        return false;
#endif
    }

    /// 拦截日志（NDJSON，一行一条）
    void logHit(const drogon::HttpRequestPtr& req, const WafVerdict& v,
                const std::string& ip) {
        try {
            Json::Value j;
            j["ts"]     = (Json::Int64)std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch()).count();
            j["ip"]     = ip;
            j["method"] = req->methodString();
            j["uri"]    = req->path();
            j["ua"]     = req->getHeader("User-Agent");
            j["rule"]   = v.ruleName;
            j["target"] = v.target;
            j["matched"]= v.matched;
            j["action"] = actionName(v.action);

            std::string line = Json::writeString(Json::StreamWriterBuilder(), j);
            std::lock_guard<std::mutex> lk(logMu_);
            std::ofstream f(logDir_ + "/waf-" + dateStr() + ".ndjson",
                            std::ios::app);
            f << line;

            // 异步上报 Manticore 审计（未启用时 enqueue 为空操作）
            j["event_type"] = "waf_hit";
            AuditQueue::instance().enqueue(std::move(j));
        } catch (...) {}
    }

    static std::string dateStr() {
        char buf[16];
        std::time_t t = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "%Y%m%d", std::localtime(&t));
        return buf;
    }

    static Target parseTarget(const std::string& s) {
        if (s == "ua" || s == "user_agent") return Target::UserAgent;
        if (s == "args" || s == "query")    return Target::Args;
        if (s == "body")                    return Target::Body;
        return Target::Uri;
    }
    static WafAction parseAction(const std::string& s) {
        if (s == "log")     return WafAction::Log;
        if (s == "ban")     return WafAction::Ban;
        if (s == "captcha") return WafAction::Captcha;
        return WafAction::Block;
    }
    static const char* actionName(WafAction a) {
        switch (a) {
            case WafAction::Pass:    return "pass";
            case WafAction::Log:     return "log";
            case WafAction::Block:   return "block";
            case WafAction::Ban:     return "ban";
            case WafAction::Captcha: return "captcha";
        }
        return "block";
    }
    static const char* targetName(Target t) {
        switch (t) {
            case Target::Uri:       return "uri";
            case Target::UserAgent: return "ua";
            case Target::Args:      return "args";
            case Target::Body:      return "body";
            default:                return "header";
        }
    }

    std::atomic<bool> enabled_{false};
    std::atomic<bool> blockMode_{false};   ///< false=log 观察模式
    std::atomic<bool> banOnHit_{false};
    std::atomic<int>  banSecs_{3600};
    std::atomic<int>  maxBody_{8192};
    std::string logDir_    = "logs/waf";
    std::string captchaUrl_;          ///< 验证码跳转地址（Captcha 动作）

    std::vector<Rule> rules_;
    mutable std::shared_mutex mu_;
    std::mutex logMu_;
    CidrMatcher whitelist_;
    CidrMatcher blacklist_;
    // URI/UA 名单（独立锁，与规则锁分离）
    mutable std::shared_mutex        listMu_;
    std::unordered_set<std::string>  uriWhitelist_;   ///< URI 前缀白名单
    std::vector<std::string>         uriBlacklist_;   ///< URI 黑名单（*后缀=前缀匹配）
    std::vector<std::string>         uaBlacklist_;    ///< UA 子串黑名单
    std::atomic<uint64_t> statsHits_{0};
    std::atomic<uint64_t> statsPass_{0};
};
