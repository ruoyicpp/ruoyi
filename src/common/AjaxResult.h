/**
 * @file AjaxResult.h
 * @brief AJAX 响应结果统一格式
 * 
 * 功能概述：
 *   - 统一 HTTP 响应格式：所有 API 响应都遵循统一的 JSON 结构
 *   - 时间戳格式化：将 PostgreSQL TIMESTAMP 转换为可读格式
 *   - 快捷响应宏：提供便捷的响应发送方法
 * 
 * 响应格式：
 *   - 成功：{ "code": 200, "msg": "操作成功", "data": {...} }
 *   - 失败：{ "code": 500, "msg": "错误信息" }
 * 
 * 对应 RuoYi.Net 中的 AjaxResult。
 */

#pragma once

#include <drogon/drogon.h>  ///< Drogon 框架

#include <json/json.h>      ///< JSON 处理库

#include <string>           ///< 标准字符串库


/**
 * @brief 格式化 PostgreSQL TIMESTAMP 为标准时间字符串
 * 
 * 将 PostgreSQL 返回的 TIMESTAMP 格式（如 "2024-06-02 09:26:00.123456"）
 * 转换为 "yyyy-MM-dd HH:mm:ss" 格式。
 * 
 * @param ts PostgreSQL TIMESTAMP 字符串
 * @return 格式化后的时间字符串，如果输入为空返回空字符串
 * 
 * @note 
 *   - 只保留前 19 个字符（年月日时分秒）
 *   - 自动处理毫秒和微秒部分
 */
inline std::string fmtTs(const std::string &ts) {
    if (ts.empty()) return "";  ///< 空值直接返回空串
    return ts.size() > 19 ? ts.substr(0, 19) : ts;  ///< 截取前 19 个字符
}

/**
 * @class AjaxResult
 * @brief AJAX 响应结果生成器
 * 
 * 提供统一的 HTTP 响应格式生成方法。
 * 所有 API 响应都通过此类生成，确保格式一致。
 * 
 * 响应结构：
 *   - code: HTTP 业务状态码（200 成功，500 失败，401 未授权等）
 *   - msg: 响应消息（成功或错误说明）
 *   - data: 响应数据（可选，仅成功响应包含）
 */
class AjaxResult {

public:

    /// @name 响应状态码常量
    /// @{
    static constexpr int CODE_SUCCESS = 200;  ///< 成功状态码

    static constexpr int CODE_ERROR   = 500;  ///< 错误状态码
    /// @}



    /// @name 成功响应方法
    /// @{

    /**
     * @brief 生成默认成功响应
     * 
     * 返回 { "code": 200, "msg": "操作成功" }
     * 
     * @return 成功响应 JSON 对象
     */
    static Json::Value success() {
        Json::Value r;  ///< 创建响应对象
        r["code"] = CODE_SUCCESS;  ///< 设置状态码为 200
        r["msg"]  = "操作成功";  ///< 设置默认成功消息
        return r;  ///< 返回响应
    }

    /**
     * @brief 生成带自定义消息的成功响应
     * 
     * 返回 { "code": 200, "msg": msg }
     * 
     * @param msg 自定义成功消息
     * @return 成功响应 JSON 对象
     */
    static Json::Value success(const std::string &msg) {
        Json::Value r;  ///< 创建响应对象
        r["code"] = CODE_SUCCESS;  ///< 设置状态码为 200
        r["msg"]  = msg;  ///< 设置自定义消息
        return r;  ///< 返回响应
    }

    /**
     * @brief 生成带数据的成功响应
     * 
     * 返回 { "code": 200, "msg": "操作成功", "data": data }
     * 
     * @param data 响应数据（JSON 对象或数组）
     * @return 成功响应 JSON 对象
     */
    static Json::Value success(const Json::Value &data) {
        Json::Value r;  ///< 创建响应对象
        r["code"] = CODE_SUCCESS;  ///< 设置状态码为 200
        r["msg"]  = "操作成功";  ///< 设置默认成功消息
        r["data"] = data;  ///< 设置响应数据
        return r;  ///< 返回响应
    }

    /**
     * @brief 生成带自定义消息和数据的成功响应
     * 
     * 返回 { "code": 200, "msg": msg, "data": data }
     * 
     * @param msg 自定义成功消息
     * @param data 响应数据（JSON 对象或数组）
     * @return 成功响应 JSON 对象
     */
    static Json::Value success(const std::string &msg, const Json::Value &data) {
        Json::Value r;  ///< 创建响应对象
        r["code"] = CODE_SUCCESS;  ///< 设置状态码为 200
        r["msg"]  = msg;  ///< 设置自定义消息
        r["data"] = data;  ///< 设置响应数据
        return r;  ///< 返回响应
    }

    /// @}

    /// @name 失败响应方法
    /// @{

    /**
     * @brief 生成默认错误响应
     * 
     * 返回 { "code": 500, "msg": msg }
     * 
     * @param msg 错误消息
     * @return 错误响应 JSON 对象
     */
    static Json::Value error(const std::string &msg) {
        Json::Value r;  ///< 创建响应对象
        r["code"] = CODE_ERROR;  ///< 设置状态码为 500
        r["msg"]  = msg;  ///< 设置错误消息
        return r;  ///< 返回响应
    }

    /**
     * @brief 生成自定义错误码的错误响应
     * 
     * 返回 { "code": code, "msg": msg }
     * 
     * @param code 自定义错误码（如 401、403、404 等）
     * @param msg 错误消息
     * @return 错误响应 JSON 对象
     */
    static Json::Value error(int code, const std::string &msg) {
        Json::Value r;  ///< 创建响应对象
        r["code"] = code;  ///< 设置自定义错误码
        r["msg"]  = msg;  ///< 设置错误消息
        return r;  ///< 返回响应
    }

    /// @}

    /**
     * @brief 生成带额外字段的成功响应
     * 
     * 用于 login、getInfo 等需要动态添加字段的接口。
     * 返回 { "code": 200, "msg": "操作成功" }
     * 
     * @return 成功响应 JSON 对象（可动态添加字段）
     * 
     * @note 调用者可以在返回后继续添加自定义字段
     */
    static Json::Value successMap() {
        Json::Value r;  ///< 创建响应对象
        r["code"] = CODE_SUCCESS;  ///< 设置状态码为 200
        r["msg"]  = "操作成功";  ///< 设置默认成功消息
        return r;  ///< 返回响应（可继续添加字段）
    }

};



/// @name 快捷响应宏
/// @{

/**
 * @def RESP_OK(cb, data)
 * @brief 发送成功响应（带数据）
 * 
 * 生成 { "code": 200, "msg": "操作成功", "data": data } 响应。
 * 
 * @param cb 响应回调函数（std::function<void(const drogon::HttpResponsePtr&)>）
 * @param data 响应数据（JSON 对象或数组）
 * 
 * @example
 *   Json::Value data;
 *   data["id"] = 123;
 *   RESP_OK(cb, data);
 */
#define RESP_OK(cb, data)    (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::success(data)))

/**
 * @def RESP_MSG(cb, msg)
 * @brief 发送成功响应（带消息）
 * 
 * 生成 { "code": 200, "msg": msg } 响应。
 * 
 * @param cb 响应回调函数
 * @param msg 成功消息（字符串）
 * 
 * @example
 *   RESP_MSG(cb, "添加成功");
 */
#define RESP_MSG(cb, msg)    (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::success(std::string(msg))))

/**
 * @def RESP_ERR(cb, msg)
 * @brief 发送错误响应
 * 
 * 生成 { "code": 500, "msg": msg } 响应。
 * 同时设置 HTTP 状态码为 500，添加 X-Business-Code 响应头。
 * 
 * @param cb 响应回调函数
 * @param msg 错误消息（字符串）
 * 
 * @example
 *   RESP_ERR(cb, "用户不存在");
 */
#define RESP_ERR(cb, msg)    do { \
    auto __re = drogon::HttpResponse::newHttpJsonResponse(AjaxResult::error(std::string(msg))); \
    __re->addHeader("X-Business-Code", "500"); \
    (cb)(__re); } while(0)

/**
 * @def RESP_401(cb)
 * @brief 发送 401 未授权响应
 * 
 * 生成 { "code": 401, "msg": "授权失败" } 响应。
 * 同时设置 HTTP 状态码为 401，添加 X-Business-Code 响应头。
 * 
 * @param cb 响应回调函数
 * 
 * @example
 *   if (!token) { RESP_401(cb); return; }
 */
#define RESP_401(cb)         do { \
    auto __re = drogon::HttpResponse::newHttpJsonResponse(AjaxResult::error(401,"授权失败")); \
    __re->setStatusCode(drogon::k401Unauthorized); \
    __re->addHeader("X-Business-Code", "401"); \
    (cb)(__re); } while(0)

/**
 * @def RESP_JSON(cb, json)
 * @brief 发送自定义 JSON 响应
 * 
 * 直接发送自定义 JSON 对象作为响应。
 * 用于需要完全自定义响应格式的场景。
 * 
 * @param cb 响应回调函数
 * @param json 自定义 JSON 对象
 * 
 * @example
 *   Json::Value custom;
 *   custom["code"] = 200;
 *   custom["data"] = {...};
 *   RESP_JSON(cb, custom);
 */
#define RESP_JSON(cb, json)  (cb)(drogon::HttpResponse::newHttpJsonResponse(json))

/// @}

