#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <drogon/drogon.h>
#include <json/json.h>

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
