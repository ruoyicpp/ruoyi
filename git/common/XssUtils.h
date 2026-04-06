#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <json/json.h>

// XSS 防御 + SQL 注入防御工具
// 主防线：控制器层全部使用参数化查询 ($N)
// 辅助：标识符白名单、输入净化、注入特征告警
class XssUtils {
public:
    // ── HTML 转义（输出到 HTML 页面时使用）────────────────────────────────────
    static std::string htmlEscape(const std::string& s) {
        std::string r;
        r.reserve(s.size() + 16);
        for (unsigned char c : s) {
            switch (c) {
                case '&':  r += "&amp;";  break;
                case '<':  r += "&lt;";   break;
                case '>':  r += "&gt;";   break;
                case '"':  r += "&quot;"; break;
                case '\'': r += "&#x27;"; break;
                case '/':  r += "&#x2F;"; break;
                default:   r += c;
            }
        }
        return r;
    }

    // ── XSS 净化（存入数据库前清洗用户输入）──────────────────────────────────
    // 移除 <script>、危险标签、事件属性、javascript: 协议
    static std::string sanitize(const std::string& input) {
        if (input.empty()) return input;
        std::string s = input;

        // 1. 移除 <script>...</script> 块（大小写不敏感，简单状态机）
        s = stripTag(s, "script");
        s = stripTag(s, "iframe");
        s = stripTag(s, "object");
        s = stripTag(s, "embed");
        s = stripTag(s, "link");
        s = stripTag(s, "style");
        s = stripTag(s, "base");
        s = stripTag(s, "form");

        // 2. 移除自闭合危险标签
        s = stripSelfClosing(s, "input");
        s = stripSelfClosing(s, "img");
        s = stripSelfClosing(s, "meta");

        // 3. 移除 on* 事件属性（onerror=, onclick=, onload= 等）
        s = stripOnEvents(s);

        // 4. 替换 javascript: 协议（多种变体）
        s = replaceNoCase(s, "javascript:", "");
        s = replaceNoCase(s, "vbscript:",   "");
        s = replaceNoCase(s, "data:text/html", "");

        return s;
    }

    // 递归净化 JSON 对象中所有字符串值
    static void sanitizeJson(Json::Value& val) {
        if (val.isString()) {
            val = sanitize(val.asString());
        } else if (val.isObject()) {
            for (auto& key : val.getMemberNames())
                sanitizeJson(val[key]);
        } else if (val.isArray()) {
            for (auto& item : val) sanitizeJson(item);
        }
    }

    // ── SQL 注入防御 ──────────────────────────────────────────────────────────
    // 验证列名/表名是否为安全标识符（只允许字母、数字、下划线）
    // 用于动态构建 ORDER BY、GROUP BY 时过滤列名
    static bool isSafeIdentifier(const std::string& name) {
        if (name.empty() || name.size() > 64) return false;
        return std::all_of(name.begin(), name.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '_';
        });
    }

    // 从允许列表中选择排序列，不在列表中则返回第一个默认值
    static std::string safeSortCol(const std::string& col,
                                   const std::vector<std::string>& allowed,
                                   const std::string& def = "") {
        for (auto& a : allowed) if (col == a) return col;
        return def.empty() ? (allowed.empty() ? "" : allowed[0]) : def;
    }

    // 安全的排序方向（只允许 ASC/DESC）
    static std::string safeSortDir(const std::string& dir) {
        std::string up = dir;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        return (up == "ASC" || up == "DESC") ? up : "ASC";
    }

    // 检测 SQL 注入特征（用于日志告警，不能代替参数化查询）
    // 返回 true 表示输入中发现可疑 SQL 特征
    static bool hasSqlSignature(const std::string& input) {
        if (input.empty()) return false;
        std::string s = toLower(input);
        // 常见注入关键字 + 特殊字符组合
        static const std::vector<std::string> patterns = {
            "'--", "';", "' or", "' and", "\"--", "\";",
            "union select", "union all select",
            "select * from", "select 1 from",
            "insert into", "delete from", "drop table", "drop database",
            "create table", "alter table",
            "exec(", "execute(", "xp_cmdshell", "sp_executesql",
            "char(", "nchar(", "varchar(", "0x", "concat(",
            "/*", "*/", "benchmark(", "sleep(", "waitfor delay",
            "load_file(", "into outfile", "into dumpfile",
        };
        for (auto& p : patterns)
            if (s.find(p) != std::string::npos) return true;
        return false;
    }

private:
    // 大小写不敏感字符串替换
    static std::string replaceNoCase(std::string s, const std::string& from, const std::string& to) {
        std::string ls = toLower(s);
        std::string lf = toLower(from);
        size_t pos = 0;
        while ((pos = ls.find(lf, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            ls.replace(pos, from.size(), to);
            pos += to.size();
        }
        return s;
    }

    // 移除 <tag ...>...</tag> 块（大小写不敏感）
    static std::string stripTag(const std::string& s, const std::string& tag) {
        std::string result = s;
        std::string ls = toLower(result);
        std::string openTag  = "<" + tag;
        std::string closeTag = "</" + tag;
        while (true) {
            auto start = ls.find(openTag);
            if (start == std::string::npos) break;
            auto end = ls.find(">", start);
            // self-closing
            if (end != std::string::npos && ls[end-1] == '/') {
                result.erase(start, end - start + 1);
                ls.erase(start, end - start + 1);
                continue;
            }
            auto closeStart = ls.find(closeTag, start);
            if (closeStart != std::string::npos) {
                auto closeEnd = ls.find(">", closeStart);
                if (closeEnd != std::string::npos) {
                    result.erase(start, closeEnd - start + 1);
                    ls.erase(start, closeEnd - start + 1);
                    continue;
                }
            }
            // 没有闭合标签，只移除开始标签
            if (end != std::string::npos) {
                result.erase(start, end - start + 1);
                ls.erase(start, end - start + 1);
            } else break;
        }
        return result;
    }

    // 移除 <tag .../> 或 <tag ...> 自闭合标签
    static std::string stripSelfClosing(const std::string& s, const std::string& tag) {
        std::string result = s;
        std::string ls = toLower(result);
        std::string openTag = "<" + tag;
        while (true) {
            auto start = ls.find(openTag);
            if (start == std::string::npos) break;
            auto end = ls.find(">", start);
            if (end == std::string::npos) break;
            result.erase(start, end - start + 1);
            ls.erase(start, end - start + 1);
        }
        return result;
    }

    // 移除事件属性 on*="..." 或 on*='...'
    static std::string stripOnEvents(std::string s) {
        // 简单状态机：扫描 "on" + 字母 + ... + "=" + 引号内容
        std::string result;
        result.reserve(s.size());
        size_t i = 0;
        std::string ls = toLower(s);
        while (i < s.size()) {
            if (i + 2 < s.size() && ls[i] == 'o' && ls[i+1] == 'n' && std::isalpha((unsigned char)ls[i+2])) {
                // 找 = 号
                size_t j = i + 2;
                while (j < s.size() && std::isalpha((unsigned char)s[j])) ++j;
                // skip spaces
                while (j < s.size() && s[j] == ' ') ++j;
                if (j < s.size() && s[j] == '=') {
                    ++j;
                    while (j < s.size() && s[j] == ' ') ++j;
                    // skip quoted value
                    if (j < s.size() && (s[j] == '"' || s[j] == '\'')) {
                        char q = s[j++];
                        while (j < s.size() && s[j] != q) ++j;
                        if (j < s.size()) ++j; // skip closing quote
                    }
                    i = j; // skip entire attribute
                    continue;
                }
            }
            result += s[i++];
        }
        return result;
    }

    static std::string toLower(const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
        return r;
    }
};
