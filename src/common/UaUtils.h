#pragma once
#include <string>

namespace UaUtils {

    struct UaParsed {
        std::string browser;
        std::string os;
    };

    inline UaParsed parse(const std::string &ua) {
        UaParsed r;

        // --- Browser（顺序重要：Edge > Opera > Firefox > IE > Chrome > Safari�?--
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
