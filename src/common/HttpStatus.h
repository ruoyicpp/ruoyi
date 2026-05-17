// HttpStatus.h — HTTP 标准状态码全集 + 扩展响应宏
// 默认不影响原有 AjaxResult.h 逻辑；
// 在需要语义化状态码的地方 #include "HttpStatus.h" 即可。
// 用法示例:
//   RESP_CODE(cb, HttpStatus::NOT_FOUND, "资源不存在")
//   RESP_CODE(cb, HttpStatus::CREATED,   "创建成功")
//   auto j = AjaxResult::of(HttpStatus::UNPROCESSABLE, "参数校验失败");
#pragma once
// 缩 include：仅需 HttpStatusCode 枚举与 HttpResponse 工厂，不需要 drogon.h 全集
// 业务代码若已 include "AppIncludes.h" 也照常工作（drogon.h 已被传递包含）
#include <drogon/HttpTypes.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>
#include "AjaxResult.h"   // 兼容：原有 RESP_OK / RESP_ERR / RESP_401 仍可用

// ════════════════════════════════════════════════════════════════════════════
// HTTP 标准状态码
// ════════════════════════════════════════════════════════════════════════════
namespace HttpStatus {

    // ── 1xx 信息性 ──────────────────────────────────────────────────────────
    constexpr int CONTINUE              = 100;  // 继续
    constexpr int SWITCHING_PROTOCOLS   = 101;  // 切换协议
    constexpr int PROCESSING            = 102;  // 处理中
    constexpr int EARLY_HINTS           = 103;  // 早期提示

    // ── 2xx 成功 ────────────────────────────────────────────────────────────
    constexpr int OK                    = 200;  // 请求成功
    constexpr int CREATED               = 201;  // 资源已创建
    constexpr int ACCEPTED              = 202;  // 请求已接受
    constexpr int NON_AUTH_INFO         = 203;  // 非授权信息
    constexpr int NO_CONTENT            = 204;  // 无内容
    constexpr int RESET_CONTENT         = 205;  // 重置内容
    constexpr int PARTIAL_CONTENT       = 206;  // 部分内容
    constexpr int MULTI_STATUS          = 207;  // 多状态
    constexpr int ALREADY_REPORTED      = 208;  // 已报告
    constexpr int IM_USED               = 226;  // 处理完成

    // ── 3xx 重定向 ──────────────────────────────────────────────────────────
    constexpr int MULTIPLE_CHOICES      = 300;  // 多种选择
    constexpr int MOVED_PERMANENTLY     = 301;  // 永久移动
    constexpr int FOUND                 = 302;  // 临时跳转
    constexpr int SEE_OTHER             = 303;  // 查看其他地址
    constexpr int NOT_MODIFIED          = 304;  // 内容未修改
    constexpr int USE_PROXY             = 305;  // 使用代理
    constexpr int TEMPORARY_REDIRECT    = 307;  // 临时重定向
    constexpr int PERMANENT_REDIRECT    = 308;  // 永久重定向

    // ── 4xx 客户端错误 ──────────────────────────────────────────────────────
    constexpr int BAD_REQUEST           = 400;  // 参数错误
    constexpr int UNAUTHORIZED          = 401;  // 未登录
    constexpr int PAYMENT_REQUIRED      = 402;  // 需要支付
    constexpr int FORBIDDEN             = 403;  // 无权限
    constexpr int NOT_FOUND             = 404;  // 页面不存在
    constexpr int METHOD_NOT_ALLOWED    = 405;  // 请求方法错误
    constexpr int NOT_ACCEPTABLE        = 406;  // 不支持的类型
    constexpr int PROXY_AUTH_REQUIRED   = 407;  // 代理认证
    constexpr int REQUEST_TIMEOUT       = 408;  // 请求超时
    constexpr int CONFLICT              = 409;  // 数据中空（冲突）
    constexpr int GONE                  = 410;  // 资源已删除
    constexpr int LENGTH_REQUIRED       = 411;  // 需要长度
    constexpr int PRECONDITION_FAILED   = 412;  // 预处理头失败
    constexpr int PAYLOAD_TOO_LARGE     = 413;  // 内容过大
    constexpr int URI_TOO_LONG          = 414;  // 地址过长
    constexpr int UNSUPPORTED_MEDIA     = 415;  // 不支持的媒体类型
    constexpr int RANGE_NOT_SATISFIABLE = 416;  // 范围无效
    constexpr int EXPECTATION_FAILED    = 417;  // 预期失败
    constexpr int IM_A_TEAPOT           = 418;  // 茶壶状态
    constexpr int MISDIRECTED_REQUEST   = 421;  // 请求方向错误
    constexpr int UNPROCESSABLE         = 422;  // 参数校验失败
    constexpr int LOCKED                = 423;  // 资源被锁定
    constexpr int FAILED_DEPENDENCY     = 424;  // 依赖失败
    constexpr int TOO_EARLY             = 425;  // 请求过早
    constexpr int UPGRADE_REQUIRED      = 426;  // 需要升级协议
    constexpr int PRECONDITION_REQUIRED = 428;  // 需要前提条件
    constexpr int TOO_MANY_REQUESTS     = 429;  // 请求过于频繁
    constexpr int HEADER_TOO_LARGE      = 431;  // 请求头过大
    constexpr int LEGAL_UNAVAILABLE     = 451;  // 因法律原因不可用

    // ── 5xx 服务端错误 ──────────────────────────────────────────────────────
    constexpr int INTERNAL_ERROR        = 500;  // 服务器内部错误
    constexpr int NOT_IMPLEMENTED       = 501;  // 功能未实现
    constexpr int BAD_GATEWAY           = 502;  // 网关错误
    constexpr int SERVICE_UNAVAILABLE   = 503;  // 服务不可用
    constexpr int GATEWAY_TIMEOUT       = 504;  // 网关超时
    constexpr int HTTP_VER_UNSUPPORTED  = 505;  // HTTP版本不支持
    constexpr int VARIANT_ALSO_NEG      = 506;  // 协商循环
    constexpr int INSUFFICIENT_STORAGE  = 507;  // 存储空间不足
    constexpr int LOOP_DETECTED         = 508;  // 检测到死循环
    constexpr int NOT_EXTENDED          = 510;  // 扩展失败
    constexpr int NETWORK_AUTH_REQUIRED = 511;  // 需要网络认证

    // ── 工具函数：状态码 → 默认中文说明 ─────────────────────────────────────
    inline std::string defaultMsg(int code) {
        switch (code) {
            case 200: return "操作成功";
            case 201: return "创建成功";
            case 202: return "已接受";
            case 204: return "无内容";
            case 301: return "永久移动";
            case 302: return "临时跳转";
            case 304: return "内容未修改";
            case 400: return "请求参数错误";
            case 401: return "未授权，请先登录";
            case 402: return "需要支付";
            case 403: return "无操作权限";
            case 404: return "资源不存在";
            case 405: return "请求方法不允许";
            case 408: return "请求超时";
            case 409: return "数据冲突";
            case 410: return "资源已删除";
            case 413: return "请求内容过大";
            case 415: return "不支持的媒体类型";
            case 422: return "参数校验失败";
            case 423: return "资源被锁定";
            case 429: return "请求过于频繁";
            case 500: return "服务器内部错误";
            case 501: return "功能未实现";
            case 502: return "网关错误";
            case 503: return "服务暂不可用";
            case 504: return "网关超时";
            default:  return "未知状态";
        }
    }

    // ── 状态码 → HTTP 层响应码（2xx/4xx/5xx）────────────────────────────────
    // JSON body 里 code 字段用业务状态码，HTTP 层默认返回 200
    // 若需 HTTP 层也反映状态码，使用 RESP_HTTP 宏
    inline drogon::HttpStatusCode toHttpCode(int code) {
        if (code >= 500) return drogon::k500InternalServerError;
        if (code == 404) return drogon::k404NotFound;
        if (code == 403) return drogon::k403Forbidden;
        if (code == 401) return drogon::k401Unauthorized;
        if (code == 400) return drogon::k400BadRequest;
        if (code >= 400) return drogon::k400BadRequest;
        return drogon::k200OK;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// AjaxResult 扩展（不修改原 AjaxResult，追加静态方法）
// ════════════════════════════════════════════════════════════════════════════
namespace HttpResult {
    // 通用构造：{ code, msg [, data] }
    inline Json::Value make(int code, const std::string& msg) {
        Json::Value r;
        r["code"] = code;
        r["msg"]  = msg.empty() ? HttpStatus::defaultMsg(code) : msg;
        return r;
    }
    inline Json::Value make(int code, const std::string& msg, const Json::Value& data) {
        Json::Value r = make(code, msg);
        r["data"] = data;
        return r;
    }
    inline Json::Value ok(const Json::Value& data, const std::string& msg = "操作成功") {
        return make(HttpStatus::OK, msg, data);
    }
    inline Json::Value created(const Json::Value& data, const std::string& msg = "创建成功") {
        return make(HttpStatus::CREATED, msg, data);
    }
    inline Json::Value fail(int code, const std::string& msg = "") {
        return make(code, msg);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 扩展宏（原有 RESP_OK / RESP_ERR / RESP_401 / RESP_MSG 完全保留不变）
// ════════════════════════════════════════════════════════════════════════════

// 语义化状态码响应（HTTP 层 200，JSON body code = 业务码，X-Business-Code 头 F12 可见）
#define RESP_CODE(cb, code, msg) \
    do { \
        auto __rc = drogon::HttpResponse::newHttpJsonResponse(HttpResult::make((code), std::string(msg))); \
        __rc->addHeader("X-Business-Code", std::to_string(code)); \
        (cb)(__rc); \
    } while(0)

// 带 data 的语义化响应
#define RESP_CODE_DATA(cb, code, msg, data) \
    (cb)(drogon::HttpResponse::newHttpJsonResponse(HttpResult::make((code), std::string(msg), (data))))

// HTTP 层 + JSON body 双层状态码（严格 RESTful 场景）
#define RESP_HTTP(cb, code, msg) \
    do { \
        auto __r = drogon::HttpResponse::newHttpJsonResponse(HttpResult::make((code), std::string(msg))); \
        __r->setStatusCode(HttpStatus::toHttpCode(code)); \
        (cb)(__r); \
    } while(0)

// 常用快捷宏（HTTP 层统一 200，前端 success handler 读 JSON body code/msg）
// 若需 F12 显示真实 HTTP 状态码，改用 RESP_HTTP 宏（仅适用于纯 RESTful 接口）
#define RESP_400(cb, msg)  RESP_CODE(cb, HttpStatus::BAD_REQUEST,       msg)
#define RESP_403(cb)       RESP_CODE(cb, HttpStatus::FORBIDDEN,         "无操作权限")
#define RESP_404(cb, msg)  RESP_CODE(cb, HttpStatus::NOT_FOUND,         msg)
#define RESP_422(cb, msg)  RESP_CODE(cb, HttpStatus::UNPROCESSABLE,     msg)
#define RESP_429(cb)       RESP_CODE(cb, HttpStatus::TOO_MANY_REQUESTS, "请求过于频繁")
#define RESP_500(cb, msg)  RESP_CODE(cb, HttpStatus::INTERNAL_ERROR,    msg)
#define RESP_503(cb)       RESP_CODE(cb, HttpStatus::SERVICE_UNAVAILABLE,"服务暂不可用")
#define RESP_201(cb, data) RESP_CODE_DATA(cb, HttpStatus::CREATED, "创建成功", data)

// ════════════════════════════════════════════════════════════════════════════
// #define 风格别名（全局顺序编号，与上方 constexpr 完全等价，按需取用）
// 命名规则: _二进制序号  HTTP码  // 序号 = 全局第N个标准HTTP码 | 说明
// 全局顺序: 1xx(1-4) 2xx(5-14) 3xx(15-22) 4xx(23-50) 5xx(51-61)
// ════════════════════════════════════════════════════════════════════════════

// 1xx 信息性
#define _1        100  // 1      = 100 | 继续
#define _10       101  // 10     = 101 | 切换协议
#define _11       102  // 11     = 102 | 处理中
#define _100      103  // 100    = 103 | 早期提示

// 2xx 成功
#define _101      200  // 101    = 200 | 请求成功
#define _110      201  // 110    = 201 | 资源已创建
#define _111      202  // 111    = 202 | 请求已接受
#define _1000     203  // 1000   = 203 | 非授权信息
#define _1001     204  // 1001   = 204 | 无内容
#define _1010     205  // 1010   = 205 | 重置内容
#define _1011     206  // 1011   = 206 | 部分内容
#define _1100     207  // 1100   = 207 | 多状态
#define _1101     208  // 1101   = 208 | 已报告
#define _1110     226  // 1110   = 226 | 处理完成

// 3xx 重定向
#define _1111     300  // 1111   = 300 | 多种选择
#define _10000    301  // 10000  = 301 | 永久移动
#define _10001    302  // 10001  = 302 | 临时跳转
#define _10010    303  // 10010  = 303 | 查看其他地址
#define _10011    304  // 10011  = 304 | 内容未修改
#define _10100    305  // 10100  = 305 | 使用代理
#define _10101    307  // 10101  = 307 | 临时重定向
#define _10110    308  // 10110  = 308 | 永久重定向

// 4xx 客户端错误
#define _10111    400  // 10111  = 400 | 参数错误
#define _11000    401  // 11000  = 401 | 未登录
#define _11001    402  // 11001  = 402 | 需要支付
#define _11010    403  // 11010  = 403 | 无权限
#define _11011    404  // 11011  = 404 | 页面不存在
#define _11100    405  // 11100  = 405 | 请求方法错误
#define _11101    406  // 11101  = 406 | 不支持的类型
#define _11110    407  // 11110  = 407 | 代理认证
#define _11111    408  // 11111  = 408 | 请求超时
#define _100000   409  // 100000 = 409 | 数据冲突
#define _100001   410  // 100001 = 410 | 资源已删除
#define _100010   411  // 100010 = 411 | 需要长度
#define _100011   412  // 100011 = 412 | 预处理失败
#define _100100   413  // 100100 = 413 | 内容过大
#define _100101   414  // 100101 = 414 | 地址过长
#define _100110   415  // 100110 = 415 | 不支持的媒体类型
#define _100111   416  // 100111 = 416 | 范围无效
#define _101000   417  // 101000 = 417 | 预期失败
#define _101001   421  // 101001 = 421 | 请求方向错误
#define _101010   422  // 101010 = 422 | 参数校验失败
#define _101011   423  // 101011 = 423 | 资源被锁定
#define _101100   424  // 101100 = 424 | 依赖失败
#define _101101   425  // 101101 = 425 | 请求过早
#define _101110   426  // 101110 = 426 | 需要升级协议
#define _101111   428  // 101111 = 428 | 需要前提条件
#define _110000   429  // 110000 = 429 | 请求过于频繁
#define _110001   431  // 110001 = 431 | 请求头过大
#define _110010   451  // 110010 = 451 | 因法律原因不可用

// 5xx 服务端错误
#define _110011   500  // 110011 = 500 | 服务器内部错误
#define _110100   501  // 110100 = 501 | 功能未实现
#define _110101   502  // 110101 = 502 | 网关错误
#define _110110   503  // 110110 = 503 | 服务不可用
#define _110111   504  // 110111 = 504 | 网关超时
#define _111000   505  // 111000 = 505 | HTTP版本不支持
#define _111001   506  // 111001 = 506 | 协商循环
#define _111010   507  // 111010 = 507 | 存储空间不足
#define _111011   508  // 111011 = 508 | 检测到死循环
#define _111100   510  // 111100 = 510 | 扩展失败
#define _111101   511  // 111101 = 511 | 需要网络认证
