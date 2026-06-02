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
 *   - 多源查询：支持多个 IP 地址库（pearapi.ai、ip-api.com、pconline）
 *   - 异步非阻塞：地址查询在后台线程执行，不阻塞主线程
 *   - 降级策略：查询失败时返回默认值 "XX XX"
 */

#pragma once
#include <drogon/drogon.h>
#include <string>
#include <thread>
#include <sstream>
#ifdef _WIN32
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
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
        std::string ip = req->getHeader("X-Forwarded-For");
        if (ip.empty()) ip = req->getHeader("X-Real-IP");
        if (ip.empty()) ip = req->getPeerAddr().toIp();
        auto pos = ip.find(',');
        if (pos != std::string::npos) ip = ip.substr(0, pos);
        while (!ip.empty() && ip.front() == ' ') ip.erase(ip.begin());
        return ip;
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

    // 异步获取 IP 位置：pearapi.ai → ip-api.com → pconline → "XX XX"
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
        std::thread([ip, cb = std::move(callback)]() {
            cb(queryIpLocationBlocking(ip));
        }).detach();
    }
#else
    // Linux: Drogon 异步 HTTP client
    static void getIpLocationAsync(const std::string &ip,
                                    std::function<void(std::string)> callback) {
        if (isIntranetIp(ip)) { callback("内网IP"); return; }
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
                                callback(getIpLocation(ip));
                            }, 3.0);
                    } catch (...) { callback(getIpLocation(ip)); }
                }, 3.0);
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
