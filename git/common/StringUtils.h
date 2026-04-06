#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <regex>

class StringUtils {
public:
    static bool isEmpty(const std::string &s) { return s.empty(); }
    static bool isNotEmpty(const std::string &s) { return !s.empty(); }
    static bool isBlank(const std::string &s) {
        return s.find_first_not_of(" \t\r\n") == std::string::npos;
    }
    static bool isHttp(const std::string &s) {
        return s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0;
    }
    static bool equals(const std::string &a, const std::string &b) { return a == b; }

    static bool containsIgnoreCase(const std::string &s, const std::string &sub) {
        std::string sl = s, subl = sub;
        std::transform(sl.begin(), sl.end(), sl.begin(), ::tolower);
        std::transform(subl.begin(), subl.end(), subl.begin(), ::tolower);
        return sl.find(subl) != std::string::npos;
    }

    static bool containsAnyIgnoreCase(const std::string &s, const std::vector<std::string> &subs) {
        for (auto &sub : subs)
            if (containsIgnoreCase(s, sub)) return true;
        return false;
    }

    static std::string substring(const std::string &s, int start, int end) {
        if (start < 0) start = 0;
        if (end > (int)s.size()) end = (int)s.size();
        if (start >= end) return "";
        return s.substr(start, end - start);
    }

    static std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> result;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim))
            if (!item.empty()) result.push_back(item);
        return result;
    }

    static std::string join(const std::vector<std::string> &v, const std::string &sep) {
        std::string r;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) r += sep;
            r += v[i];
        }
        return r;
    }

    static std::string trim(const std::string &s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end   = s.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return s.substr(start, end - start + 1);
    }

    // 安全地将字符串转为整数（失败返回默认值）
    static std::string toUpperCamelCase(const std::string &s) {
        if (s.empty()) return s;
        std::string r = s;
        r[0] = std::toupper(r[0]);
        return r;
    }

    // 去除首尾空白字符
    static std::string replaceFirst(const std::string &s,
                                    const std::string &from,
                                    const std::string &to) {
        auto pos = s.find(from);
        if (pos == std::string::npos) return s;
        std::string r = s;
        r.replace(pos, from.size(), to);
        return r;
    }
};
