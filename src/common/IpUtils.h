#pragma once
#include <drogon/drogon.h>
#include <string>

class IpUtils {
public:
    static std::string getIpAddr(const drogon::HttpRequestPtr &req) {
    // 优先读取 X-Forwarded-For
        std::string ip = req->getHeader("X-Forwarded-For");
        if (ip.empty()) ip = req->getHeader("X-Real-IP");
        if (ip.empty()) ip = req->getPeerAddr().toIp();
    // 校验单个 IP 是否匹配黑白名单（支持通配符）
        auto pos = ip.find(',');
        if (pos != std::string::npos) ip = ip.substr(0, pos);
    // 工具函数
        while (!ip.empty() && ip.front() == ' ') ip.erase(ip.begin());
        return ip;
    }


    // 判断是否内网 IP
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

    // 异步获取 IP 位置：优先 pearktrue 高精度 API（3s 超时），失败降级本地
    // callback(location)，在 Drogon IO 线程回调
    static void getIpLocationAsync(const std::string &ip,
                                    std::function<void(std::string)> callback) {
        if (isIntranetIp(ip)) { callback("内网IP"); return; }
        try {
            auto client = drogon::HttpClient::newHttpClient("https://api.pearktrue.cn");
            auto extReq = drogon::HttpRequest::newHttpRequest();
            extReq->setPath("/api/ip/high/");
            extReq->setParameter("ip", ip);
            extReq->setMethod(drogon::Get);
            client->sendRequest(extReq,
                [ip, callback](drogon::ReqResult result,
                               const drogon::HttpResponsePtr &resp) mutable {
                    if (result == drogon::ReqResult::Ok && resp) {
                        try {
                            auto j = resp->getJsonObject();
                            if (j && j->isObject() && (*j)["code"].asInt() == 200) {
                                auto &data = (*j)["data"];
                                std::string province = data.get("province", "").asString();
                                std::string city     = data.get("city",     "").asString();
                                std::string district = data.get("district", "").asString();
                                if (district.empty()) district = data.get("area",   "").asString();
                                if (district.empty()) district = data.get("county", "").asString();
                                // 兜底：尝试根节点
                                if (province.empty()) province = (*j).get("province", "").asString();
                                if (city.empty())     city     = (*j).get("city",     "").asString();
                                if (district.empty()) district = (*j).get("district","").asString();
                                std::string loc;
                                if (!province.empty()) loc = province;
                                if (!city.empty() && city != province) {
                                    if (!loc.empty()) loc += " ";
                                    loc += city;
                                }
                                if (!district.empty() && district != city && district != province) {
                                    if (!loc.empty()) loc += " ";
                                    loc += district;
                                }
                                if (!loc.empty()) { callback(std::move(loc)); return; }
                            }
                        } catch (...) {}
                    }
                    callback(getIpLocation(ip)); // 降级
                }, 3.0);
        } catch (...) {
            callback(getIpLocation(ip));
        }
    }

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
