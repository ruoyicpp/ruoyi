#pragma once
#include <string>
#include <drogon/drogon.h>
#include "../services/DatabaseService.h"
#include "IpUtils.h"
#include "../filters/PermFilter.h"

enum class BusinessType : int {
    OTHER   = 0,
    INSERT  = 1,
    UPDATE  = 2,
    REMOVE  = 3,
    GRANT   = 4,
    EXPORT  = 5,
    IMPORT  = 6,
    FORCE   = 7,
    GENCODE = 8,
    CLEAN   = 9,
};

// 操作日志工具（对�?C# [Log] AOP 属性）
// 用法: OperLogUtils::write(req, "用户管理", BusinessType::INSERT, operParam);
namespace OperLogUtils {

    inline std::string truncate(const std::string &s, size_t maxLen = 2000) {
        return s.size() <= maxLen ? s : s.substr(0, maxLen);
    }

    inline std::string getOperParam(const drogon::HttpRequestPtr &req) {
        auto body = req->getJsonObject();
        if (body) {
            Json::StreamWriterBuilder wb;
            wb["indentation"] = "";
            return truncate(Json::writeString(wb, *body));
        }
        // 没有 JSON body 时取 query string
        std::string q;
        for (auto &[k, v] : req->getParameters()) {
            if (!q.empty()) q += "&";
            q += k + "=" + v;
        }
        return truncate(q);
    }

    inline void write(const drogon::HttpRequestPtr &req,
                      const std::string &title,
                      BusinessType businessType,
                      const std::string &operParam = "",
                      int status = 0,
                      const std::string &jsonResult = "",
                      const std::string &errorMsg = "") {
        std::string ip       = IpUtils::getIpAddr(req);
        std::string operName = PermissionChecker::getUserName(req);
        std::string url      = std::string(req->getPath());
        std::string method   = req->getMethodString();
        std::string param    = operParam.empty() ? getOperParam(req) : truncate(operParam);
        std::string result   = truncate(jsonResult);
        DatabaseService::instance().execParams(
            "INSERT INTO sys_oper_log"
            "(title,business_type,method,request_method,oper_type,oper_name,dept_name,"
            " oper_url,oper_ip,oper_location,oper_param,json_result,status,error_msg,oper_time) "
            "VALUES($1,$2,$3,$4,0,$5,'',$6,$7,'',$8,$9,$10,$11,NOW())",
            {title,
             std::to_string(static_cast<int>(businessType)),
             title,          // method 字段�?title（C++ 无反射，存模块名�?             method,
             operName,
             url,
             ip,
             param,
             result,
             std::to_string(status),
             errorMsg});
    }

    // 便捷宏（�?handler 内调用，自动提取参数�?    // LOG_OPER(req, "用户管理", BusinessType::INSERT)
}

// 快捷宏，在控制器函数体内使用
#define LOG_OPER(req, title, btype) \
    OperLogUtils::write((req), (title), (btype))

#define LOG_OPER_PARAM(req, title, btype, param) \
    OperLogUtils::write((req), (title), (btype), (param))
