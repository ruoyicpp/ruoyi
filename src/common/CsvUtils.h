/**
 * @file CsvUtils.h
 * @brief CSV 导出工具 — 生成和导出 CSV 文件
 * 
 * 功能概述：
 *   - CSV 生成：从 JSON 或二维数组生成 CSV 格式数据
 *   - 单元格转义：自动转义含有特殊字符的单元格
 *   - UTF-8 BOM：自动添加 UTF-8 BOM，确保 Excel 正确显示中文
 *   - HTTP 响应：直接生成 CSV 下载响应
 * 
 * 使用示例：
 *   // 从 JSON 数组生成 CSV
 *   Json::Value rows;
 *   rows[0]["name"] = "张三";
 *   rows[0]["email"] = "zhangsan@example.com";
 *   
 *   std::vector<std::pair<std::string,std::string>> headers = {
 *       {"姓名", "name"},
 *       {"邮箱", "email"}
 *   };
 *   
 *   std::string csv = CsvUtils::toCsv(rows, headers);
 *   auto resp = CsvUtils::makeCsvResponse(csv, "users.csv");
 *   cb(resp);
 * 
 * 特性：
 *   - UTF-8 BOM：自动添加 EF BB BF，确保 Excel 正确显示中文
 *   - 单元格转义：逗号、引号、换行等特殊字符自动转义
 *   - 灵活生成：支持 JSON 和二维数组两种数据源
 *   - HTTP 集成：直接生成 HTTP 响应，支持自定义文件名
 */

#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <drogon/drogon.h>
#include <json/json.h>

/**
 * @namespace CsvUtils
 * @brief CSV 导出工具命名空间
 * 
 * 提供 CSV 文件生成和 HTTP 响应功能。
 * 所有函数都是内联的，无需编译链接。
 */
namespace CsvUtils {

    // 转义 CSV 单元格（含逗号/引号/换行时加双引号包裹）
    inline std::string escapeCell(const std::string &s) {
        if (s.find_first_of(",\"\r\n") == std::string::npos) return s;
        std::string out = "\"";
        for (char c : s) {
            if (c == '"') out += "\"\"";
            else out += c;
        }
        out += "\"";
        return out;
    }

    // 从 JSON 对象数组生成 CSV 字符串（第一行为 header）
    // headers: {{"显示名", "jsonKey"}, ...}
    inline std::string toCsv(const Json::Value &rows,
                              const std::vector<std::pair<std::string,std::string>> &headers) {
        std::ostringstream ss;
        // BOM（Excel 打开中文不乱码）
        ss << "\xEF\xBB\xBF";
        // header row
        for (size_t i = 0; i < headers.size(); ++i) {
            if (i) ss << ",";
            ss << escapeCell(headers[i].first);
        }
        ss << "\r\n";
        // data rows
        if (rows.isArray()) {
            for (const auto &row : rows) {
                for (size_t i = 0; i < headers.size(); ++i) {
                    if (i) ss << ",";
                    ss << escapeCell(row.get(headers[i].second, "").asString());
                }
                ss << "\r\n";
            }
        }
        return ss.str();
    }

    // 从 headers + 二维字符串数据生成 CSV（带 UTF-8 BOM）
    inline std::string generate(const std::vector<std::string> &headers,
                                const std::vector<std::vector<std::string>> &rows) {
        std::ostringstream ss;
        ss << "\xEF\xBB\xBF";
        for (size_t i = 0; i < headers.size(); ++i) {
            if (i) ss << ",";
            ss << escapeCell(headers[i]);
        }
        ss << "\r\n";
        for (const auto &row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i) ss << ",";
                ss << escapeCell(row[i]);
            }
            ss << "\r\n";
        }
        return ss.str();
    }

    // 直接返回 CSV 响应
    inline drogon::HttpResponsePtr makeCsvResponse(const std::string &csv,
                                                    const std::string &filename = "export.csv") {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setContentTypeString("text/csv; charset=utf-8");
        resp->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        resp->setBody(csv);
        return resp;
    }
}
