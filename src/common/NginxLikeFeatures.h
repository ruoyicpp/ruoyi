// =============================================================
// NginxLikeFeatures.h — 在 drogon 内原生实现 nginx 最常用功能
// 1. limit_conn   每 IP 并发连接数限流
// 2. allow/deny   全站 IP 白/黑名单（支持 CIDR）
// 3. proxy_pass   简单反向代理（前缀匹配 + 转发到 upstream）
// 4. access_log   nginx-combined 格式访问日志（便于 GoAccess 等工具分析）
//
// 配置（config.json 顶层增加 "nginx_like" 段）：
//   "nginx_like": {
//     "limit_conn":  { "enabled": false, "max_per_ip": 50 },
//     "ip_acl":      { "enabled": false, "default": "allow",
//                       "deny": ["1.2.3.0/24"], "allow": ["10.0.0.0/8"] },
//     "proxy_pass":  [ { "path_prefix": "/up/", "upstream": "http://127.0.0.1:9001",
//                         "strip_prefix": true } ],
//     "access_log":  { "enabled": true, "path": "logs/access.log" }
//   }
//
// 加载顺序（main.cc 中）：
//   NginxLikeFeatures::registerAll(cfgRoot);
//   建议放在 CORS 之后、IP 限流之前，proxy_pass 优先级最高
// =============================================================
#pragma once

#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include "IpUtils.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ruoyi { namespace nginx_like {

// ── IPv4 CIDR 范围匹配 ─────────────────────────────────────────────────────
struct CidrV4 {
    uint32_t addr = 0;
    uint32_t mask = 0;
    bool     any  = false;  // "all" / "0.0.0.0/0"

    static CidrV4 parse(const std::string& s) {
        CidrV4 r;
        if (s == "all" || s == "0.0.0.0/0") { r.any = true; r.mask = 0; return r; }
        std::string ipPart = s; int bits = 32;
        auto p = s.find('/');
        if (p != std::string::npos) {
            ipPart = s.substr(0, p);
            try { bits = std::stoi(s.substr(p + 1)); }
            catch (...) { bits = 32; }
            if (bits < 0 || bits > 32) bits = 32;
        }
        // parse a.b.c.d
        unsigned int a = 0, b = 0, c = 0, d = 0;
        if (std::sscanf(ipPart.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
            // 无效 IP → 不匹配任何东西
            r.addr = 0; r.mask = 0xFFFFFFFFu; return r;
        }
        r.addr = (a << 24) | (b << 16) | (c << 8) | d;
        r.mask = (bits == 0) ? 0u : (0xFFFFFFFFu << (32 - bits));
        return r;
    }

    bool match(uint32_t ip) const {
        if (any) return true;
        return (ip & mask) == (addr & mask);
    }

    static uint32_t ipToUint(const std::string& ip) {
        unsigned int a = 0, b = 0, c = 0, d = 0;
        if (std::sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return 0;
        return (a << 24) | (b << 16) | (c << 8) | d;
    }
};

// ── 1. allow / deny 全站 IP 白黑名单 ──────────────────────────────────────
class IpAcl {
public:
    static IpAcl& instance() { static IpAcl x; return x; }

    bool enabled  = false;
    bool defaultAllow = true;
    std::vector<CidrV4> allowList;
    std::vector<CidrV4> denyList;

    void load(const Json::Value& cfg) {
        enabled      = cfg.get("enabled", false).asBool();
        defaultAllow = (cfg.get("default", "allow").asString() != "deny");
        denyList.clear(); allowList.clear();
        if (cfg.isMember("deny") && cfg["deny"].isArray())
            for (auto& v : cfg["deny"])  denyList.push_back(CidrV4::parse(v.asString()));
        if (cfg.isMember("allow") && cfg["allow"].isArray())
            for (auto& v : cfg["allow"]) allowList.push_back(CidrV4::parse(v.asString()));
    }

    // 返回 true 表示放行
    bool check(const std::string& ip) const {
        if (!enabled) return true;
        uint32_t u = CidrV4::ipToUint(ip);
        // 显式 deny → 立即拒绝
        for (auto& r : denyList)  if (r.match(u)) return false;
        // 显式 allow → 放行
        for (auto& r : allowList) if (r.match(u)) return true;
        return defaultAllow;
    }
};

// ── 2. limit_conn 每 IP 并发连接限流 ─────────────────────────────────────
class ConnLimit {
public:
    static ConnLimit& instance() { static ConnLimit x; return x; }

    bool enabled   = false;
    int  maxPerIp  = 50;

    void load(const Json::Value& cfg) {
        enabled  = cfg.get("enabled", false).asBool();
        maxPerIp = cfg.get("max_per_ip", 50).asInt();
        if (maxPerIp <= 0) maxPerIp = 50;
    }

    // 返回 true 表示获取成功（未超限）
    bool acquire(const std::string& ip) {
        if (!enabled) return true;
        std::lock_guard<std::mutex> lk(mu_);
        int& c = counts_[ip];
        if (c >= maxPerIp) return false;
        ++c;
        return true;
    }
    void release(const std::string& ip) {
        if (!enabled) return;
        std::lock_guard<std::mutex> lk(mu_);
        auto it = counts_.find(ip);
        if (it == counts_.end()) return;
        if (--it->second <= 0) counts_.erase(it);
    }

private:
    std::mutex mu_;
    std::unordered_map<std::string, int> counts_;
};

// ── 3. proxy_pass 简单反向代理 ─────────────────────────────────────────────
struct ProxyRule {
    std::string                       pathPrefix;
    std::string                       upstream;     // 例: http://127.0.0.1:9001
    bool                              stripPrefix = true;
    std::shared_ptr<drogon::HttpClient> client;     // 复用 keepalive
};

class ProxyPass {
public:
    static ProxyPass& instance() { static ProxyPass x; return x; }

    bool enabled = false;
    std::vector<ProxyRule> rules;

    void load(const Json::Value& arr) {
        rules.clear();
        if (!arr.isArray()) return;
        for (auto& r : arr) {
            ProxyRule pr;
            pr.pathPrefix  = r.get("path_prefix", "").asString();
            pr.upstream    = r.get("upstream", "").asString();
            pr.stripPrefix = r.get("strip_prefix", true).asBool();
            if (pr.pathPrefix.empty() || pr.upstream.empty()) continue;
            // 启动时建好 HttpClient（drogon 要求在 event loop 线程；这里先延后到 register 时）
            rules.push_back(std::move(pr));
        }
        enabled = !rules.empty();
    }

    void buildClients() {
        for (auto& r : rules) {
            r.client = drogon::HttpClient::newHttpClient(r.upstream);
            r.client->setPipeliningDepth(0);  // 关闭流水线，简化错误处理
        }
    }

    // 匹配到则执行代理并通过 cb 返回响应；返回 true 表示已处理
    bool tryHandle(const drogon::HttpRequestPtr& req,
                   const std::function<void(const drogon::HttpResponsePtr&)>& cb) {
        if (!enabled) return false;
        std::string path = req->path();
        for (auto& r : rules) {
            if (path.rfind(r.pathPrefix, 0) != 0) continue;
            std::string newPath = r.stripPrefix
                ? path.substr(r.pathPrefix.size())
                : path;
            if (newPath.empty() || newPath[0] != '/') newPath = "/" + newPath;
            // 构建转发请求（保留方法/头/体）
            auto fwd = drogon::HttpRequest::newHttpRequest();
            fwd->setMethod(req->getMethod());
            fwd->setPath(newPath);
            for (auto& kv : req->getHeaders()) {
                // 跳过 hop-by-hop 头
                std::string k = kv.first;
                std::string lk; lk.reserve(k.size());
                for (auto c : k) lk.push_back((char)std::tolower((unsigned char)c));
                if (lk == "host" || lk == "connection" || lk == "keep-alive" ||
                    lk == "proxy-authenticate" || lk == "proxy-authorization" ||
                    lk == "te" || lk == "trailer" || lk == "transfer-encoding" ||
                    lk == "upgrade" || lk == "content-length")
                    continue;
                fwd->addHeader(k, kv.second);
            }
            // X-Forwarded-For
            std::string xff = req->getHeader("X-Forwarded-For");
            std::string ip  = IpUtils::getIpAddr(req);
            fwd->addHeader("X-Forwarded-For", xff.empty() ? ip : (xff + ", " + ip));
            fwd->addHeader("X-Real-IP", ip);
            // 透传原始 query
            std::string q = req->query();
            if (!q.empty()) fwd->setParameter("__raw_query__", q);
            // body
            auto body = req->body();
            if (!body.empty()) fwd->setBody(std::string(body));
            // 发送
            r.client->sendRequest(fwd,
                [cb](drogon::ReqResult result,
                     const drogon::HttpResponsePtr& upstreamResp) {
                    if (result != drogon::ReqResult::Ok || !upstreamResp) {
                        auto err = drogon::HttpResponse::newHttpResponse();
                        err->setStatusCode(drogon::k502BadGateway);
                        err->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                        err->setBody(R"({"code":502,"msg":"upstream unreachable"})");
                        cb(err);
                        return;
                    }
                    cb(upstreamResp);
                },
                15.0);
            return true;
        }
        return false;
    }
};

// ── 4. access_log nginx-combined 格式 ─────────────────────────────────────
class AccessLog {
public:
    static AccessLog& instance() { static AccessLog x; return x; }

    bool enabled = false;
    std::string path;

    void load(const Json::Value& cfg) {
        enabled = cfg.get("enabled", false).asBool();
        path    = cfg.get("path", "logs/access.log").asString();
    }

    // combined 格式：
    //   $remote_addr - $remote_user [$time_local] "$request"
    //   $status $body_bytes_sent "$http_referer" "$http_user_agent"
    void write(const drogon::HttpRequestPtr& req,
               const drogon::HttpResponsePtr& resp) {
        if (!enabled) return;
        std::string ip = IpUtils::getIpAddr(req);

        // 时间：[10/Oct/2000:13:55:36 +0000]
        std::time_t t = std::time(nullptr);
        struct tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char tbuf[64]; std::strftime(tbuf, sizeof tbuf, "%d/%b/%Y:%H:%M:%S %z", &tm);

        // 请求行: METHOD PATH?QUERY HTTP/1.1
        std::string method = req->methodString();
        std::string path   = req->path();
        std::string query  = req->query();
        std::string reqLine = method + " " + path;
        if (!query.empty()) reqLine += "?" + query;
        reqLine += " " + std::string(req->versionString());

        int status = resp ? (int)resp->statusCode() : 0;
        size_t bytes = resp ? resp->body().size() : 0;
        std::string referer = req->getHeader("Referer");
        std::string ua      = req->getHeader("User-Agent");

        // 转义双引号
        auto esc = [](std::string s) {
            for (auto it = s.begin(); it != s.end(); ++it)
                if (*it == '"') { it = s.insert(it, '\\'); ++it; }
            return s;
        };

        std::string line;
        line.reserve(256 + reqLine.size() + ua.size());
        line += ip;
        line += " - - [";
        line += tbuf;
        line += "] \"";
        line += esc(reqLine);
        line += "\" ";
        line += std::to_string(status);
        line += " ";
        line += std::to_string(bytes);
        line += " \"";
        line += esc(referer);
        line += "\" \"";
        line += esc(ua);
        line += "\"\n";

        std::lock_guard<std::mutex> lk(mu_);
        if (!ofs_.is_open()) {
            ofs_.open(path, std::ios::app | std::ios::binary);
            if (!ofs_.is_open()) {
                LOG_WARN << "[AccessLog] cannot open " << path;
                enabled = false;
                return;
            }
        }
        ofs_ << line;
        ofs_.flush();
    }

private:
    std::mutex     mu_;
    std::ofstream  ofs_;
};

// ── 总入口：根据 config.json 一次性注册所有 advice ─────────────────────────
inline void registerAll(const Json::Value& cfgRoot) {
    if (!cfgRoot.isMember("nginx_like")) return;
    const auto& nx = cfgRoot["nginx_like"];

    // 加载配置
    if (nx.isMember("ip_acl"))     IpAcl::instance().load(nx["ip_acl"]);
    if (nx.isMember("limit_conn")) ConnLimit::instance().load(nx["limit_conn"]);
    if (nx.isMember("proxy_pass")) ProxyPass::instance().load(nx["proxy_pass"]);
    if (nx.isMember("access_log")) AccessLog::instance().load(nx["access_log"]);

    LOG_INFO << "[NginxLike] ip_acl=" << IpAcl::instance().enabled
             << " limit_conn=" << ConnLimit::instance().enabled
             << " proxy=" << (ProxyPass::instance().enabled ? ProxyPass::instance().rules.size() : 0)
             << " access_log=" << AccessLog::instance().enabled;
    std::cout << "[NginxLike] ip_acl=" << IpAcl::instance().enabled
              << " limit_conn=" << ConnLimit::instance().enabled
              << " proxy=" << (ProxyPass::instance().enabled ? ProxyPass::instance().rules.size() : 0)
              << " access_log=" << AccessLog::instance().enabled << std::endl;

    // 先注册 proxy_pass advice（优先级最高，命中则不再走后续 drogon 路由）
    if (ProxyPass::instance().enabled) {
        // HttpClient 必须在 drogon 事件循环启动后创建；用 getLoop()->runInLoop 推迟
        drogon::app().getLoop()->runInLoop([]() {
            ProxyPass::instance().buildClients();
        });
        drogon::app().registerPreRoutingAdvice(
            [](const drogon::HttpRequestPtr& req,
               drogon::AdviceCallback&& cb,
               drogon::AdviceChainCallback&& ccb) {
                if (ProxyPass::instance().tryHandle(req,
                        [cb](const drogon::HttpResponsePtr& r) { cb(r); }))
                    return;
                ccb();
            });
    }

    // IP ACL + limit_conn 合并到一个 advice，省一次 IP 提取
    if (IpAcl::instance().enabled || ConnLimit::instance().enabled) {
        drogon::app().registerPreRoutingAdvice(
            [](const drogon::HttpRequestPtr& req,
               drogon::AdviceCallback&& cb,
               drogon::AdviceChainCallback&& ccb) {
                std::string ip = IpUtils::getIpAddr(req);
                if (!IpAcl::instance().check(ip)) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k403Forbidden);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody(R"({"code":403,"msg":"IP forbidden"})");
                    cb(resp);
                    return;
                }
                if (!ConnLimit::instance().acquire(ip)) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode((drogon::HttpStatusCode)429);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody(R"({"code":429,"msg":"too many concurrent connections"})");
                    resp->addHeader("Retry-After", "5");
                    cb(resp);
                    return;
                }
                ccb();
            });
    }

    // post-handling: 释放 conn 计数 + 写 access log
    if (ConnLimit::instance().enabled || AccessLog::instance().enabled) {
        drogon::app().registerPostHandlingAdvice(
            [](const drogon::HttpRequestPtr& req,
               const drogon::HttpResponsePtr& resp) {
                std::string ip = IpUtils::getIpAddr(req);
                ConnLimit::instance().release(ip);
                AccessLog::instance().write(req, resp);
            });
    }
}

}}  // namespace ruoyi::nginx_like
