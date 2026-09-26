/**
 * @file WafCtrl.h
 * @brief WAF 管理控制器 — 拦截日志查询、封禁管理、规则管理、统计
 *
 * 功能概述：
 *   - 拦截日志：查询 logs/waf/*.ndjson 拦截记录（分页 + 条件过滤）
 *   - 封禁管理：查询/手动封禁/解封 IP（RiskStore + NftBan 双层）
 *   - 规则管理：查看内置规则、动态添加自定义规则
 *   - 统计信息：命中数、放行数、封禁数、规则数
 *   - CIDR 名单：黑白名单网段的查看与追加
 *
 * API 端点：
 *   GET    /monitor/waf/stats          - WAF 统计信息
 *   GET    /monitor/waf/logs           - 拦截日志（?date=&ip=&rule=&page=&size=）
 *   GET    /monitor/waf/bans           - 当前封禁列表
 *   POST   /monitor/waf/ban            - 手动封禁 {ip, seconds, reason}
 *   DELETE /monitor/waf/ban/{ip}       - 解封 IP
 *   GET    /monitor/waf/rules          - 规则列表
 *   POST   /monitor/waf/rules          - 添加自定义规则 {name,pattern,target,action}
 *   GET    /monitor/waf/cidrs          - CIDR 黑白名单
 *   POST   /monitor/waf/cidrs          - 添加 CIDR {list:"white|black", cidr:"x.x.x.x/n"}
 *
 * 权限要求：monitor:waf:* 系列权限
 */

#pragma once
#include <drogon/HttpController.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include "../common/AjaxResult.h"
#include "../common/OperLogUtils.h"
#include "../filters/PermFilter.h"
#include "WafEngine.h"
#include "RiskStore.h"
#include "NftBan.h"

/**
 * @class WafCtrl
 * @brief WAF 管理控制器
 *
 * 提供 WAF 引擎的完整管理接口，供 Web 控制台调用。
 */
class WafCtrl : public drogon::HttpController<WafCtrl> {
public:
    /// 非 Linux 平台统一返回"不可用"（功能仅在 Linux 构建中启用）
    static bool wafUnavailable(std::function<void(const drogon::HttpResponsePtr&)>& cb) {
#ifndef __linux__
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            Json::Value("WAF 功能仅在 Linux 构建中可用"));
        resp->setStatusCode(drogon::k501NotImplemented);
        cb(resp);
        return true;
#else
        (void)cb;
        return false;
#endif
    }

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(WafCtrl::stats,      "/monitor/waf/stats",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::logs,       "/monitor/waf/logs",         drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::bans,       "/monitor/waf/bans",         drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::ban,        "/monitor/waf/ban",          drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::unban,      "/monitor/waf/ban/{ip}",     drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::rules,      "/monitor/waf/rules",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::addRule,    "/monitor/waf/rules",        drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::toggleRule, "/monitor/waf/rules/toggle", drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::delRule,    "/monitor/waf/rules/{name}", drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::cidrs,      "/monitor/waf/cidrs",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::addCidr,    "/monitor/waf/cidrs",        drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::uriList,    "/monitor/waf/uri-list",     drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::addUri,     "/monitor/waf/uri-list",     drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::delUri,     "/monitor/waf/uri-list",     drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::uaList,     "/monitor/waf/ua-list",      drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::addUa,      "/monitor/waf/ua-list",      drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(WafCtrl::delUa,      "/monitor/waf/ua-list",      drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    /// GET /monitor/waf/stats — 统计信息
    void stats(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:list");
        if (wafUnavailable(cb)) return;
        Json::Value j;
        j["enabled"]      = WafEngine::instance().isEnabled();
        j["hits"]         = (Json::UInt64)WafEngine::instance().hits();
        j["passed"]       = (Json::UInt64)WafEngine::instance().passed();
        j["rules"]        = (Json::UInt64)WafEngine::instance().ruleCount();
        j["bans"]         = (Json::UInt64)RiskStore::instance().bannedList().size();
        j["nftables"]     = NftBan::instance().isAvailable();
        RESP_OK(cb, j);
    }

    /// GET /monitor/waf/logs — 拦截日志查询（读 NDJSON 文件）
    void logs(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:list");
        if (wafUnavailable(cb)) return;
        auto date   = req->getParameter("date");   // YYYYMMDD，默认今天
        auto ipF    = req->getParameter("ip");
        auto ruleF  = req->getParameter("rule");
        int  page   = std::atoi(req->getParameter("page").c_str());
        int  size   = std::atoi(req->getParameter("size").c_str());
        if (page <= 0) page = 1;
        if (size <= 0 || size > 200) size = 50;

        if (date.empty()) {
            char buf[16];
            std::time_t t = std::time(nullptr);
            std::strftime(buf, sizeof(buf), "%Y%m%d", std::localtime(&t));
            date = buf;
        }

        std::string path = "logs/waf/waf-" + date + ".ndjson";
        std::ifstream f(path);
        if (!f) { RESP_ERR(cb, "日志文件不存在: " + path); return; }

        // 倒序读取（最新在前），过滤后分页
        std::vector<Json::Value> matched;
        std::string line;
        Json::CharReaderBuilder rb;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            Json::Value j; std::string err;
            std::istringstream ss(line);
            if (!Json::parseFromStream(rb, ss, &j, &err)) continue;
            if (!ipF.empty()   && j["ip"].asString()   != ipF)   continue;
            if (!ruleF.empty() && j["rule"].asString() != ruleF) continue;
            matched.push_back(j);
        }
        std::reverse(matched.begin(), matched.end());

        Json::Value arr(Json::arrayValue);
        int start = (page - 1) * size;
        for (int i = start; i < (int)matched.size() && i < start + size; i++)
            arr.append(matched[i]);

        Json::Value out;
        out["total"] = (Json::UInt64)matched.size();
        out["page"]  = page;
        out["size"]  = size;
        out["list"]  = arr;
        RESP_OK(cb, out);
    }

    /// GET /monitor/waf/bans — 当前封禁列表
    void bans(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:list");
        if (wafUnavailable(cb)) return;
        Json::Value arr(Json::arrayValue);
        for (auto& [ip, info] : RiskStore::instance().bannedList()) {
            Json::Value j;
            j["ip"]        = ip;
            j["expire_at"] = (Json::Int64)info.expireAt;
            j["reason"]    = info.reason;
            j["hit_count"] = info.hitCount;
            arr.append(j);
        }
        RESP_OK(cb, arr);
    }

    /// POST /monitor/waf/ban — 手动封禁 {ip, seconds, reason}
    void ban(const drogon::HttpRequestPtr& req,
             std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:ban");
        if (wafUnavailable(cb)) return;
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("ip")) { RESP_ERR(cb, "缺少 ip 参数"); return; }
        std::string ip     = (*body)["ip"].asString();
        int         secs   = body->get("seconds", 3600).asInt();
        std::string reason = body->get("reason", "manual").asString();
        if (ip.empty()) { RESP_ERR(cb, "ip 不能为空"); return; }

        RiskStore::instance().ban(ip, secs, reason);
        bool kernel = NftBan::instance().banIp(ip, secs);   // 内核层（Linux，失败自动忽略）
        LOG_OPER_PARAM(req, "WAF封禁", BusinessType::INSERT, ip);

        Json::Value j;
        j["app_layer"]    = true;
        j["kernel_layer"] = kernel;
        RESP_OK(cb, j);
    }

    /// DELETE /monitor/waf/ban/{ip} — 解封
    void unban(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb,
               const std::string& ip) {
        CHECK_PERM(req, cb, "monitor:waf:ban");
        if (wafUnavailable(cb)) return;
        RiskStore::instance().unban(ip);
        NftBan::instance().unbanIp(ip);
        LOG_OPER_PARAM(req, "WAF解封", BusinessType::REMOVE, ip);
        RESP_MSG(cb, "解封成功");
    }

    /// GET /monitor/waf/rules — 完整规则列表（含启停状态）
    void rules(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:list");
        if (wafUnavailable(cb)) return;
        Json::Value arr(Json::arrayValue);
        for (auto& r : WafEngine::instance().listRules()) {
            Json::Value j;
            j["name"]    = r.name;
            j["pattern"] = r.pattern;
            j["target"]  = r.target;
            j["action"]  = r.action;
            j["enabled"] = r.enabled;
            arr.append(j);
        }
        RESP_OK(cb, arr);
    }

    /// POST /monitor/waf/rules/toggle — 启用/禁用单条规则 {name, enabled}
    void toggleRule(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:edit");
        if (wafUnavailable(cb)) return;
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("name")) { RESP_ERR(cb, "缺少 name"); return; }
        std::string name = (*body)["name"].asString();
        bool enabled = body->get("enabled", true).asBool();
        if (!WafEngine::instance().setRuleEnabled(name, enabled)) {
            RESP_ERR(cb, "规则不存在: " + name); return;
        }
        LOG_OPER_PARAM(req, "WAF规则开关", BusinessType::UPDATE,
                       name + (enabled ? "=on" : "=off"));
        RESP_MSG(cb, enabled ? "已启用" : "已禁用");
    }

    /// DELETE /monitor/waf/rules/{name} — 删除规则
    void delRule(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 const std::string& name) {
        CHECK_PERM(req, cb, "monitor:waf:edit");
        if (wafUnavailable(cb)) return;
        if (!WafEngine::instance().removeRule(name)) {
            RESP_ERR(cb, "规则不存在: " + name); return;
        }
        LOG_OPER_PARAM(req, "WAF规则", BusinessType::REMOVE, name);
        RESP_MSG(cb, "删除成功");
    }

    /// GET /monitor/waf/uri-list — URI 黑白名单
    void uriList(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:list");
        if (wafUnavailable(cb)) return;
        Json::Value j;
        for (auto& u : WafEngine::instance().uriWhitelist()) j["whitelist"].append(u);
        for (auto& u : WafEngine::instance().uriBlacklist()) j["blacklist"].append(u);
        RESP_OK(cb, j);
    }

    /// POST /monitor/waf/uri-list — 添加 URI {list:"white|black", uri:"/path" 或 "/prefix/*"}
    void addUri(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:edit");
        if (wafUnavailable(cb)) return;
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("uri")) { RESP_ERR(cb, "缺少 uri"); return; }
        std::string uri  = (*body)["uri"].asString();
        bool white = body->get("list", "black").asString() == "white";
        if (uri.empty()) { RESP_ERR(cb, "uri 不能为空"); return; }
        WafEngine::instance().addUriRule(white, uri);
        LOG_OPER_PARAM(req, "WAF_URI名单", BusinessType::INSERT,
                       std::string(white ? "white:" : "black:") + uri);
        RESP_MSG(cb, "添加成功");
    }

    /// DELETE /monitor/waf/uri-list — 删除 URI {list:"white|black", uri:"/path"}
    void delUri(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:edit");
        if (wafUnavailable(cb)) return;
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("uri")) { RESP_ERR(cb, "缺少 uri"); return; }
        std::string uri  = (*body)["uri"].asString();
        bool white = body->get("list", "black").asString() == "white";
        if (!WafEngine::instance().removeUriRule(white, uri)) {
            RESP_ERR(cb, "URI 不在名单中"); return;
        }
        LOG_OPER_PARAM(req, "WAF_URI名单", BusinessType::REMOVE, uri);
        RESP_MSG(cb, "删除成功");
    }

    /// GET /monitor/waf/ua-list — UA 黑名单
    void uaList(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:list");
        if (wafUnavailable(cb)) return;
        Json::Value arr(Json::arrayValue);
        for (auto& u : WafEngine::instance().uaBlacklist()) arr.append(u);
        RESP_OK(cb, arr);
    }

    /// POST /monitor/waf/ua-list — 添加 UA 黑名单 {ua:"sqlmap"}（子串匹配）
    void addUa(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:edit");
        if (wafUnavailable(cb)) return;
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("ua")) { RESP_ERR(cb, "缺少 ua"); return; }
        std::string ua = (*body)["ua"].asString();
        if (ua.empty()) { RESP_ERR(cb, "ua 不能为空"); return; }
        WafEngine::instance().addUaRule(ua);
        LOG_OPER_PARAM(req, "WAF_UA名单", BusinessType::INSERT, ua);
        RESP_MSG(cb, "添加成功");
    }

    /// DELETE /monitor/waf/ua-list — 删除 UA 黑名单 {ua:"sqlmap"}
    void delUa(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:edit");
        if (wafUnavailable(cb)) return;
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("ua")) { RESP_ERR(cb, "缺少 ua"); return; }
        if (!WafEngine::instance().removeUaRule((*body)["ua"].asString())) {
            RESP_ERR(cb, "UA 不在黑名单中"); return;
        }
        LOG_OPER_PARAM(req, "WAF_UA名单", BusinessType::REMOVE,
                       (*body)["ua"].asString());
        RESP_MSG(cb, "删除成功");
    }

    /// POST /monitor/waf/rules — 添加自定义规则
    void addRule(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:edit");
        if (wafUnavailable(cb)) return;
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("pattern")) { RESP_ERR(cb, "缺少 pattern"); return; }
        std::string name    = body->get("name", "custom").asString();
        std::string pattern = (*body)["pattern"].asString();
        std::string target  = body->get("target", "uri").asString();
        std::string action  = body->get("action", "block").asString();

        auto t = target == "ua"   ? WafEngine::Target::UserAgent :
                 target == "args" ? WafEngine::Target::Args :
                 target == "body" ? WafEngine::Target::Body :
                                    WafEngine::Target::Uri;
        // parseAction 是 private，这里直接映射
        WafAction a = action == "log" ? WafAction::Log :
                      action == "ban" ? WafAction::Ban : WafAction::Block;

        if (!WafEngine::instance().addRule(name, pattern, t, a)) {
            RESP_ERR(cb, "规则编译失败（正则语法错误）");
            return;
        }
        LOG_OPER_PARAM(req, "WAF规则", BusinessType::INSERT, name);
        RESP_MSG(cb, "添加成功");
    }

    /// GET /monitor/waf/cidrs — CIDR 名单
    void cidrs(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:list");
        if (wafUnavailable(cb)) return;
        Json::Value j;
        for (auto& r : WafEngine::instance().whitelist().rules())
            j["whitelist"].append(r);
        for (auto& r : WafEngine::instance().blacklist().rules())
            j["blacklist"].append(r);
        RESP_OK(cb, j);
    }

    /// POST /monitor/waf/cidrs — 添加 CIDR {list:"white|black", cidr:"x.x.x.x/n"}
    void addCidr(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:waf:edit");
        if (wafUnavailable(cb)) return;
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("cidr")) { RESP_ERR(cb, "缺少 cidr"); return; }
        std::string list = body->get("list", "black").asString();
        std::string cidr = (*body)["cidr"].asString();

        bool ok = (list == "white")
            ? WafEngine::instance().whitelist().addRule(cidr)
            : WafEngine::instance().blacklist().addRule(cidr);
        if (!ok) { RESP_ERR(cb, "CIDR 格式错误"); return; }
        LOG_OPER_PARAM(req, "WAF网段", BusinessType::INSERT, list + ":" + cidr);
        RESP_MSG(cb, "添加成功");
    }
};
