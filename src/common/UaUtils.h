/**
 * @file UaUtils.h
 * @brief User-Agent 解析工具 — 识别浏览器和操作系统
 * 
 * 功能概述：
 *   - 浏览器识别：识别 Edge、Opera、Firefox、IE、Chrome、Safari 等
 *   - 操作系统识别：识别 Windows、macOS、Linux、iOS、Android 等
 *   - 设备信息：用于登录日志和设备管理
 * 
 * 使用示例：
 *   std::string ua = req->getHeader("User-Agent");
 *   auto parsed = UaUtils::parse(ua);
 *   std::cout << "Browser: " << parsed.browser << std::endl;
 *   std::cout << "OS: " << parsed.os << std::endl;
 * 
 * 识别的浏览器：
 *   - Edge、Opera、Firefox、Internet Explorer、Chrome、Safari
 * 
 * 识别的操作系统：
 *   - Windows 10/11、Windows 8.1、Windows 8、Windows 7、Windows Vista、Windows
 *   - iPhone、iPad、Android、Mac OS X、Linux
 */

#pragma once
#include <string>

/**
 * @namespace UaUtils
 * @brief User-Agent 解析工具命名空间
 * 
 * 提供 User-Agent 字符串的解析功能，用于识别客户端浏览器和操作系统。
 */
namespace UaUtils {

    /**
     * @struct UaParsed
     * @brief 解析后的 User-Agent 信息
     */
    struct UaParsed {
        std::string browser;  ///< 浏览器名称
        std::string os;       ///< 操作系统名称
    };

    /**
     * @brief 解析 User-Agent 字符串
     * 
     * 从 User-Agent 字符串中识别浏览器和操作系统。
     * 浏览器识别顺序：Edge > Opera > Firefox > IE > Chrome > Safari
     * 
     * @param ua User-Agent 字符串（通常来自 HTTP 请求头）
     * @return 解析结果，包含浏览器和操作系统信息
     */
    inline UaParsed parse(const std::string &ua) {
        UaParsed r;

    // --- Browser（顺序重要：Edge > Opera > Firefox > IE > Chrome > Safari）---
        if (ua.find("Edg/") != std::string::npos || ua.find("Edge/") != std::string::npos)
            r.browser = "Edge";
        else if (ua.find("OPR/") != std::string::npos || ua.find("Opera/") != std::string::npos)
            r.browser = "Opera";
        else if (ua.find("Firefox/") != std::string::npos)
            r.browser = "Firefox";
        else if (ua.find("MSIE") != std::string::npos || ua.find("Trident/") != std::string::npos)
            r.browser = "Internet Explorer";
        else if (ua.find("Chrome/") != std::string::npos)
            r.browser = "Chrome";
        else if (ua.find("Safari/") != std::string::npos)
            r.browser = "Safari";
        else if (ua.empty())
            r.browser = "Unknown";
        else
            r.browser = ua.substr(0, std::min((int)ua.size(), 40));

        // --- OS ---
        if (ua.find("Windows NT 10.0") != std::string::npos || ua.find("Windows NT 11.0") != std::string::npos)
            r.os = "Windows 10/11";
        else if (ua.find("Windows NT 6.3") != std::string::npos)
            r.os = "Windows 8.1";
        else if (ua.find("Windows NT 6.2") != std::string::npos)
            r.os = "Windows 8";
        else if (ua.find("Windows NT 6.1") != std::string::npos)
            r.os = "Windows 7";
        else if (ua.find("Windows NT 6.0") != std::string::npos)
            r.os = "Windows Vista";
        else if (ua.find("Windows") != std::string::npos)
            r.os = "Windows";
        else if (ua.find("iPhone") != std::string::npos)
            r.os = "iPhone";
        else if (ua.find("iPad") != std::string::npos)
            r.os = "iPad";
        else if (ua.find("Android") != std::string::npos)
            r.os = "Android";
        else if (ua.find("Mac OS X") != std::string::npos)
            r.os = "Mac OS X";
        else if (ua.find("Linux") != std::string::npos)
            r.os = "Linux";
        else
            r.os = "Unknown";

        return r;
    }
}
