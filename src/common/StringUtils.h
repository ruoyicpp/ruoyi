/**
 * @file StringUtils.h
 * @brief 字符串工具类 — 提供字符串处理的常用方法
 * 
 * 功能概述：
 *   - 检查方法：判断字符串是否为空、空白、HTTP URL 等
 *   - 查找方法：忽略大小写的字符串查找
 *   - 分割合并：字符串分割和数组合并
 *   - 转换方法：大小写转换、驼峰命名法转换
 *   - 替换方法：字符串替换、去除空白
 * 
 * 核心特性：
 *   - 简洁 API：提供常用的字符串操作
 *   - 安全处理：边界检查，避免越界
 *   - 大小写无关：支持忽略大小写的查找
 */

#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <regex>

/**
 * @class StringUtils
 * @brief 字符串工具类
 * 
 * 提供字符串处理的常用静态方法。
 */
class StringUtils {
public:
    /**
     * @brief 判断字符串是否为空
     * @param s 输入字符串
     * @return 如果字符串为空返回 true
     */
    static bool isEmpty(const std::string &s) { return s.empty(); }

    /**
     * @brief 判断字符串是否非空
     * @param s 输入字符串
     * @return 如果字符串非空返回 true
     */
    static bool isNotEmpty(const std::string &s) { return !s.empty(); }

    /**
     * @brief 判断字符串是否为空白（仅包含空格、制表符、换行符等）
     * @param s 输入字符串
     * @return 如果字符串为空白返回 true
     */
    static bool isBlank(const std::string &s) {
        return s.find_first_not_of(" \t\r\n") == std::string::npos;
    }

    /**
     * @brief 判断字符串是否为 HTTP/HTTPS URL
     * @param s 输入字符串
     * @return 如果以 "http://" 或 "https://" 开头返回 true
     */
    static bool isHttp(const std::string &s) {
        return s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0;
    }

    /**
     * @brief 判断两个字符串是否相等
     * @param a 第一个字符串
     * @param b 第二个字符串
     * @return 如果相等返回 true
     */
    static bool equals(const std::string &a, const std::string &b) { return a == b; }

    /**
     * @brief 忽略大小写判断字符串是否包含子串
     * @param s 源字符串
     * @param sub 子串
     * @return 如果包含返回 true
     */
    static bool containsIgnoreCase(const std::string &s, const std::string &sub) {
        std::string sl = s, subl = sub;
        std::transform(sl.begin(), sl.end(), sl.begin(), ::tolower);
        std::transform(subl.begin(), subl.end(), subl.begin(), ::tolower);
        return sl.find(subl) != std::string::npos;
    }

    /**
     * @brief 忽略大小写判断字符串是否包含任意一个子串
     * @param s 源字符串
     * @param subs 子串列表
     * @return 如果包含任意一个子串返回 true
     */
    static bool containsAnyIgnoreCase(const std::string &s, const std::vector<std::string> &subs) {
        for (auto &sub : subs)
            if (containsIgnoreCase(s, sub)) return true;
        return false;
    }

    /**
     * @brief 提取字符串的子串
     * 
     * 类似 Java 的 substring(start, end)，包含 start 不包含 end。
     * 自动处理越界情况。
     * 
     * @param s 源字符串
     * @param start 起始位置（包含）
     * @param end 结束位置（不包含）
     * @return 子串
     */
    static std::string substring(const std::string &s, int start, int end) {
        if (start < 0) start = 0;
        if (end > (int)s.size()) end = (int)s.size();
        if (start >= end) return "";
        return s.substr(start, end - start);
    }

    /**
     * @brief 按分隔符分割字符串
     * 
     * 自动忽略空字符串。
     * 
     * @param s 源字符串
     * @param delim 分隔符字符
     * @return 分割后的字符串数组
     */
    static std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> result;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim))
            if (!item.empty()) result.push_back(item);
        return result;
    }

    /**
     * @brief 用分隔符连接字符串数组
     * 
     * @param v 字符串数组
     * @param sep 分隔符
     * @return 连接后的字符串
     */
    static std::string join(const std::vector<std::string> &v, const std::string &sep) {
        std::string r;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) r += sep;
            r += v[i];
        }
        return r;
    }

    /**
     * @brief 去除字符串首尾的空白字符
     * 
     * 移除空格、制表符、换行符等。
     * 
     * @param s 源字符串
     * @return 去除空白后的字符串
     */
    static std::string trim(const std::string &s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end   = s.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return s.substr(start, end - start + 1);
    }

    /**
     * @brief 将字符串首字母转为大写（UpperCamelCase）
     * 
     * @param s 源字符串
     * @return 首字母大写的字符串
     */
    static std::string toUpperCamelCase(const std::string &s) {
        if (s.empty()) return s;
        std::string r = s;
        r[0] = std::toupper(r[0]);
        return r;
    }

    /**
     * @brief 替换字符串中第一个匹配的子串
     * 
     * @param s 源字符串
     * @param from 要替换的子串
     * @param to 替换为的字符串
     * @return 替换后的字符串
     */
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
