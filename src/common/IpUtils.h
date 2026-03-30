#pragma once
#include <drogon/drogon.h>
#include <string>

class IpUtils {
public:
    static std::string getIpAddr(const drogon::HttpRequestPtr &req) {
        // ����ȡ X-Forwarded-For
        std::string ip = req->getHeader("X-Forwarded-For");
        if (ip.empty()) ip = req->getHeader("X-Real-IP");
        if (ip.empty()) ip = req->getPeerAddr().toIp();
        // ȡ��һ�� IP���༶����ʱ��
        auto pos = ip.find(',');
        if (pos != std::string::npos) ip = ip.substr(0, pos);
        // ȥ�ո�
        while (!ip.empty() && ip.front() == ' ') ip.erase(ip.begin());
        return ip;
    }

    // �� IP ������ƥ�䣨֧�� * ͨ�䣩
    static bool isMatchedIp(const std::string &blackList, const std::string &ip) {
        if (blackList.empty() || ip.empty()) return false;
        // �� ; �ָ�
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
        // �� * ͨ�䣺�� 192.168.*
        auto pos = pattern.find('*');
        if (pos != std::string::npos) {
            std::string prefix = pattern.substr(0, pos);
            return ip.rfind(prefix, 0) == 0;
        }
        return false;
    }
};
