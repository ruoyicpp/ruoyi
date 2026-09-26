/**
 * @file IpUtils.h
 * @brief IP 地址工具 — 处理 IP 提取、验证、地理位置查询等
 * 
 * 功能概述：
 *   - IP 提取：从 HTTP 请求中提取客户端真实 IP
 *   - IP 验证：判断 IP 是否为内网地址
 *   - 位置查询：异步查询 IP 地理位置
 *   - 跨平台支持：Windows 使用 WinHTTP，Linux 使用 curl
 * 
 * 核心特性：
 *   - 代理感知：支持 X-Forwarded-For、X-Real-IP 等代理头
 *   - 多源查询：pearapi.ai → ip9.com.cn → ip.360.cn → ip-api.com → ipwho.is
 *   - 异步非阻塞：地址查询在后台线程执行，不阻塞主线程
 *   - 降级策略：查询失败时返回默认值 "XX XX"
 */

#pragma once
#include <drogon/drogon.h>
#include <string>
#include <thread>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <functional>
#include "../waf/CidrMatcher.h"
#ifdef _WIN32
#  include <windows.h>
#  include <winhttp.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "winhttp.lib")
#else
#  include <arpa/inet.h>
#endif

/**
 * @class IpUtils
 * @brief IP 地址工具类
 * 
 * 提供 IP 地址提取、验证、地理位置查询等功能。
 * 支持代理环境和跨平台 HTTP 请求。
 */
class IpUtils {
public:
    /**
     * @brief 从 HTTP 请求中提取客户端真实 IP
     * 
     * 按优先级从以下位置提取 IP：
     *   1. X-Forwarded-For 请求头（代理链中的第一个 IP）
     *   2. X-Real-IP 请求头
     *   3. 直接连接的对端 IP
     * 
     * 如果 X-Forwarded-For 包含多个 IP（逗号分隔），取第一个。
     * 自动移除 IP 前后的空格。
     * 
     * @param req HTTP 请求对象
     * @return 客户端真实 IP 地址
     */
    static std::string getIpAddr(const drogon::HttpRequestPtr &req) {
        std::string peer = req->getPeerAddr().toIp();
        // 仅当直连对端是可信代理时才采信转发头；
        // 否则客户端可伪造 X-Forwarded-For 绕过 IP 白名单/封禁/风控。
        if (!trustedProxies().matches(peer)) return peer;
        std::string ip = req->getHeader("X-Forwarded-For");
        if (ip.empty()) ip = req->getHeader("X-Real-IP");
        if (ip.empty()) return peer;
        auto pos = ip.find(',');
        if (pos != std::string::npos) ip = ip.substr(0, pos);
        while (!ip.empty() && ip.front() == ' ') ip.erase(ip.begin());
        while (!ip.empty() && ip.back()  == ' ') ip.pop_back();
        // 转发头内容非法（非 IP）时回退对端 IP，
        // 防止脏数据流入封禁名单 / nftables 命令拼接
        if (!isValidIp(ip)) return peer;
        return ip;
    }

    /**
     * @brief 校验字符串是否为合法 IPv4/IPv6 地址
     * @note 供 WAF 封禁、nftables 命令拼接前的入参校验使用
     */
    static bool isValidIp(const std::string &ip) {
        if (ip.empty() || ip.size() > 45) return false;  // IPv6 最长 45
        struct in_addr  a4;
        struct in6_addr a6;
        return inet_pton(AF_INET,  ip.c_str(), &a4) == 1 ||
               inet_pton(AF_INET6, ip.c_str(), &a6) == 1;
    }

    /**
     * @brief 设置可信代理 CIDR 列表（security.trusted_proxies）
     *
     * 只有直连对端命中该名单时才采信 X-Forwarded-For / X-Real-IP。
     * 默认空列表 = 不信任任何代理头（最安全的默认）。
     * 部署在 nginx/网关后时，把代理的 IP/CIDR 填进来。
     */
    static void setTrustedProxies(const std::vector<std::string>& cidrs) {
        trustedProxies().clear();
        for (auto& c : cidrs) trustedProxies().addRule(c);
    }

    /**
     * @brief 判断 IP 是否为内网地址
     * 
     * 识别以下内网 IP 范围：
     *   - 127.0.0.1、::1、localhost（本地环回）
     *   - 10.0.0.0/8（10.0.0.0 - 10.255.255.255）
     *   - 192.168.0.0/16（192.168.0.0 - 192.168.255.255）
     *   - 172.16.0.0/12（172.16.0.0 - 172.31.255.255）
     * 
     * @param ip IP 地址字符串
     * @return 如果是内网 IP 返回 true，否则返回 false
     */
    static bool isIntranetIp(const std::string &ip) {
        if (ip.empty() || ip == "127.0.0.1" || ip == "::1" || ip == "localhost") return true;
        if (ip.rfind("10.", 0) == 0) return true;
        if (ip.rfind("192.168.", 0) == 0) return true;
        if (ip.rfind("172.", 0) == 0) {
            size_t dot2 = ip.find('.', 4);
            if (dot2 != std::string::npos) {
                int second = std::atoi(ip.substr(4, dot2 - 4).c_str());
                if (second >= 16 && second <= 31) return true;
            }
        }
        return false;
    }

    // 根据 IP 返回位置描述（内网/未知，同步降级用）
    static std::string getIpLocation(const std::string &ip) {
        if (isIntranetIp(ip)) return "内网IP";
        return "XX XX";
    }

    // ── 归属地结果缓存（自包含，不依赖 MemCache 避免 include 环）────────────
    // 成功 24h，失败 "XX XX" 负缓存 5min（避免外网抖动期每个登录都串行 25s）
    // 上限 4096 条，超限整体清空（IP 基数低，简单策略足够）
    struct LocCacheEnt {
        std::string loc;
        std::chrono::steady_clock::time_point exp;
    };
    static std::unordered_map<std::string, LocCacheEnt> &locCache() {
        static std::unordered_map<std::string, LocCacheEnt> m;
        return m;
    }
    static std::mutex &locCacheMu() { static std::mutex m; return m; }

    static bool locCacheGet(const std::string &ip, std::string &out) {
        std::lock_guard<std::mutex> lk(locCacheMu());
        auto it = locCache().find(ip);
        if (it == locCache().end()) return false;
        if (std::chrono::steady_clock::now() > it->second.exp) {
            locCache().erase(it);
            return false;
        }
        out = it->second.loc;
        return true;
    }

    static void locCachePut(const std::string &ip, const std::string &loc) {
        int ttl = (loc == "XX XX") ? 300 : 86400;
        std::lock_guard<std::mutex> lk(locCacheMu());
        if (locCache().size() > 4096) locCache().clear();
        locCache()[ip] = {loc, std::chrono::steady_clock::now() + std::chrono::seconds(ttl)};
    }

    /// 命中检测 + 包装回调：结果写回缓存后再交给上层
    static bool locCacheHitOrWrap(const std::string &ip,
                                  std::function<void(std::string)> &callback) {
        std::string c;
        if (locCacheGet(ip, c)) { callback(c); return true; }
        callback = [ip, callback](std::string loc) mutable {
            locCachePut(ip, loc);
            callback(std::move(loc));
        };
        return false;
    }

    // 异步获取 IP 位置：pearapi.ai → ip9.com.cn → ip.360.cn → ip-api.com → ipwho.is → "XX XX"
#ifdef _WIN32
    // Windows: WinHTTP 同步请求跑在 detached 线程，绕过 Drogon event-loop 兼容问题
    static std::string winHttpGet(const std::wstring &host, const std::wstring &path, bool https) {
        HINTERNET hS = WinHttpOpen(L"Mozilla/5.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hS) return "";
        INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        HINTERNET hC = WinHttpConnect(hS, host.c_str(), port, 0);
        if (!hC) { WinHttpCloseHandle(hS); return ""; }
        HINTERNET hR = WinHttpOpenRequest(hC, L"GET", path.c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
        if (!hR) { WinHttpCloseHandle(hC); WinHttpCloseHandle(hS); return ""; }
        if (https) {
            DWORD sf = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE
                     | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            WinHttpSetOption(hR, WINHTTP_OPTION_SECURITY_FLAGS, &sf, sizeof(sf));
        }
        if (!WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(hR, nullptr)) {
            WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS); return "";
        }
        std::string body; DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hR, &avail) && avail > 0) {
            std::string buf(avail, '\0'); DWORD rd = 0;
            WinHttpReadData(hR, &buf[0], avail, &rd);
            body.append(buf.data(), rd);
        }
        WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
        return body;
    }

    static std::string queryIpLocationBlocking(const std::string &ip) {
        // 1. pearapi.ai
        try {
            std::wstring path = L"/api/ip/high/?ip=" + std::wstring(ip.begin(), ip.end());
            std::string body = winHttpGet(L"api.pearapi.ai", path, true);
            if (!body.empty()) {
                Json::Value j; Json::CharReaderBuilder rb; std::string e;
                std::istringstream ss(body);
                if (Json::parseFromStream(rb, ss, &j, &e) && j["code"].asInt() == 200) {
                    auto &d = j["data"];
                    std::string pro = d.get("province","").asString();
                    std::string city= d.get("city","").asString();
                    std::string dist= d.get("district","").asString();
                    std::string loc;
                    if (!pro.empty())  loc = pro;
                    if (!city.empty() && city != pro)  { if (!loc.empty()) loc+=" "; loc+=city; }
                    if (!dist.empty() && dist != city && dist != pro) { if (!loc.empty()) loc+=" "; loc+=dist; }
                    if (!loc.empty()) return loc;
                }
            }
        } catch (...) {}
        // 2. ip-api.com (HTTP)
        try {
            std::wstring path = L"/json/" + std::wstring(ip.begin(), ip.end()) + L"?lang=zh-CN&fields=status,regionName,city";
            std::string body = winHttpGet(L"ip-api.com", path, false);
            if (!body.empty()) {
                Json::Value j; Json::CharReaderBuilder rb; std::string e;
                std::istringstream ss(body);
                if (Json::parseFromStream(rb, ss, &j, &e) && j["status"].asString() == "success") {
                    std::string region = j.get("regionName","").asString();
                    std::string city   = j.get("city","").asString();
                    std::string loc;
                    if (!region.empty()) loc = region;
                    if (!city.empty() && city != region) { if (!loc.empty()) loc+=" "; loc+=city; }
                    if (!loc.empty()) return loc;
                }
            }
        } catch (...) {}
        return "XX XX";
    }

    static void getIpLocationAsync(const std::string &ip,
                                    std::function<void(std::string)> callback) {
        if (isIntranetIp(ip)) { callback("内网IP"); return; }
        if (locCacheHitOrWrap(ip, callback)) return;
        std::thread([ip, cb = std::move(callback)]() {
            cb(queryIpLocationBlocking(ip));
        }).detach();
    }
#else
    // Linux: Drogon 异步 HTTP client
    static void getIpLocationAsync(const std::string &ip,
                                    std::function<void(std::string)> callback) {
        if (isIntranetIp(ip)) { callback("内网IP"); return; }
        if (locCacheHitOrWrap(ip, callback)) return;
        try {
            auto client = drogon::HttpClient::newHttpClient("https://api.pearapi.ai");
            auto extReq = drogon::HttpRequest::newHttpRequest();
            extReq->setPath("/api/ip/high/");
            extReq->setParameter("ip", ip);
            extReq->setMethod(drogon::Get);
            extReq->addHeader("User-Agent", "Mozilla/5.0");
            client->sendRequest(extReq,
                [ip, callback](drogon::ReqResult result,
                               const drogon::HttpResponsePtr &resp) mutable {
                    if (result == drogon::ReqResult::Ok && resp) {
                        try {
                            auto j = resp->getJsonObject();
                            if (j && (*j)["code"].asInt() == 200) {
                                auto &d = (*j)["data"];
                                std::string pro = d.get("province","").asString();
                                std::string city= d.get("city","").asString();
                                std::string dist= d.get("district","").asString();
                                std::string loc;
                                if (!pro.empty())  loc = pro;
                                if (!city.empty() && city != pro)  { if (!loc.empty()) loc+=" "; loc+=city; }
                                if (!dist.empty() && dist != city && dist != pro) { if (!loc.empty()) loc+=" "; loc+=dist; }
                                if (!loc.empty()) { callback(std::move(loc)); return; }
                            }
                        } catch (...) {}
                    }
                    // 第2跳：ip9.com.cn（HTTPS 国内源，含运营商，60次/分钟限速）
                    try {
                        auto c9 = drogon::HttpClient::newHttpClient("https://ip9.com.cn");
                        auto r9 = drogon::HttpRequest::newHttpRequest();
                        r9->setPath("/get");
                        r9->setParameter("ip", ip);
                        r9->setMethod(drogon::Get);
                        r9->addHeader("User-Agent", "Mozilla/5.0");
                        c9->sendRequest(r9,
                            [ip, callback](drogon::ReqResult res9,
                                           const drogon::HttpResponsePtr &rsp9) mutable {
                                if (res9 == drogon::ReqResult::Ok && rsp9) {
                                    try {
                                        auto j9 = rsp9->getJsonObject();
                                        if (j9 && (*j9)["ret"].asInt() == 200) {
                                            auto &d9 = (*j9)["data"];
                                            std::string prov = d9.get("prov","").asString();
                                            std::string city = d9.get("city","").asString();
                                            std::string loc;
                                            if (!prov.empty()) loc = prov;
                                            if (!city.empty() && city != prov) { if (!loc.empty()) loc+=" "; loc+=city; }
                                            if (!loc.empty()) { callback(std::move(loc)); return; }
                                        }
                                    } catch (...) {}
                                }
                                // 第3跳：ip.360.cn（HTTP 需 UA+Referer，精确到区县）
                                query360(ip, callback);
                            }, 5.0);
                        return;
                    } catch (...) {}
                    // ip9 构造失败直接进第3跳
                    query360(ip, callback);
                }, 5.0);
        } catch (...) { callback(getIpLocation(ip)); }
    }

    /// 第3跳：ip.360.cn（data="省市区\t运营商"，取 \t 前）
    static void query360(const std::string &ip,
                         std::function<void(std::string)> callback) {
        try {
            auto c = drogon::HttpClient::newHttpClient("http://ip.360.cn");
            auto r = drogon::HttpRequest::newHttpRequest();
            r->setPath("/IPQuery/ipquery");
            r->setParameter("ip", ip);
            r->setMethod(drogon::Get);
            r->addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
            r->addHeader("Referer", "https://ip.360.cn/");
            c->sendRequest(r,
                [ip, callback](drogon::ReqResult res,
                               const drogon::HttpResponsePtr &rsp) mutable {
                    if (res == drogon::ReqResult::Ok && rsp) {
                        try {
                            auto j = rsp->getJsonObject();
                            if (j && (*j)["errno"].asInt() == 0) {
                                std::string data = (*j).get("data","").asString();
                                auto tab = data.find('\t');
                                if (tab != std::string::npos) data = data.substr(0, tab);
                                while (!data.empty() && data.back() == ' ') data.pop_back();
                                if (!data.empty()) { callback(std::move(data)); return; }
                            }
                        } catch (...) {}
                    }
                    queryIpApi(ip, callback);
                }, 5.0);
        } catch (...) { queryIpApi(ip, callback); }
    }

    /// 第4跳：ip-api.com（提取为私有函数便于链式回退）
    static void queryIpApi(const std::string &ip,
                           std::function<void(std::string)> callback) {
        try {
            auto c2 = drogon::HttpClient::newHttpClient("http://ip-api.com");
            auto r2 = drogon::HttpRequest::newHttpRequest();
            r2->setPath("/json/" + ip);
            r2->setParameter("lang","zh-CN");
            r2->setParameter("fields","status,regionName,city");
            r2->setMethod(drogon::Get);
            c2->sendRequest(r2,
                            [ip, callback](drogon::ReqResult res2,
                                           const drogon::HttpResponsePtr &rsp2) mutable {
                                if (res2 == drogon::ReqResult::Ok && rsp2) {
                                    try {
                                        auto j2 = rsp2->getJsonObject();
                                        if (j2 && (*j2)["status"].asString() == "success") {
                                            std::string region = (*j2).get("regionName","").asString();
                                            std::string city   = (*j2).get("city","").asString();
                                            std::string loc;
                                            if (!region.empty()) loc = region;
                                            if (!city.empty() && city != region) { if (!loc.empty()) loc+=" "; loc+=city; }
                                            if (!loc.empty()) { callback(std::move(loc)); return; }
                                        }
                                    } catch (...) {}
                                }
                                // 第5跳：ipwho.is（HTTPS 兜底）
                                try {
                                    auto c3 = drogon::HttpClient::newHttpClient("https://ipwho.is");
                                    auto r3 = drogon::HttpRequest::newHttpRequest();
                                    r3->setPath("/" + ip);
                                    r3->setParameter("lang","zh-CN");
                                    r3->setParameter("fields","success,region,city");
                                    r3->setMethod(drogon::Get);
                                    c3->sendRequest(r3,
                                        [ip, callback](drogon::ReqResult res3,
                                                       const drogon::HttpResponsePtr &rsp3) mutable {
                                            if (res3 == drogon::ReqResult::Ok && rsp3) {
                                                try {
                                                    auto j3 = rsp3->getJsonObject();
                                                    if (j3 && (*j3)["success"].asBool()) {
                                                        std::string region = (*j3).get("region","").asString();
                                                        std::string city   = (*j3).get("city","").asString();
                                                        std::string loc;
                                                        if (!region.empty()) loc = region;
                                                        if (!city.empty() && city != region) { if (!loc.empty()) loc+=" "; loc+=city; }
                                                        if (!loc.empty()) { callback(std::move(loc)); return; }
                                                    }
                                                } catch (...) {}
                                            }
                                            callback(getIpLocation(ip));
                                        }, 5.0);
                                } catch (...) { callback(getIpLocation(ip)); }
                            }, 5.0);
        } catch (...) { callback(getIpLocation(ip)); }
    }
#endif

    // 判断 IP 是否匹配某个段，支持 * 通配符
    static bool isMatchedIp(const std::string &blackList, const std::string &ip) {
        if (blackList.empty() || ip.empty()) return false;
    // 按 ; 分割
        size_t start = 0;
        while (start < blackList.size()) {
            auto end = blackList.find(';', start);
            if (end == std::string::npos) end = blackList.size();
            std::string pattern = blackList.substr(start, end - start);
            if (!pattern.empty() && matchPattern(pattern, ip)) return true;
            start = end + 1;
        }
        return false;
    }

private:
    /// 可信代理匹配器（函数内 static，避免静态初始化顺序问题）
    static CidrMatcher& trustedProxies() {
        static CidrMatcher m;
        return m;
    }

    static bool matchPattern(const std::string &pattern, const std::string &ip) {
        if (pattern == "*") return true;
        if (pattern == ip)  return true;
    // 支持 * 通配符，如 192.168.*
        auto pos = pattern.find('*');
        if (pos != std::string::npos) {
            std::string prefix = pattern.substr(0, pos);
            return ip.rfind(prefix, 0) == 0;
        }
        return false;
    }
};
