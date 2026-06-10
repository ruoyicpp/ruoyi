// HttpStatus.h — HTTP 标准状态码全集 + 双模式自适应响应
//
// 概述：
//   本文件提供 HTTP 标准状态码常量 + 自动适配的响应宏。
//   根据前端类型（SPA / 非 SPA），响应行为自动切换，无需业务代码手动判断。
//
// 使用方式：
//   RESP_CODE(cb, HttpStatus::NOT_FOUND, "资源不存在")
//   RESP_CODE(cb, HttpStatus::UNAUTHORIZED, "未登录")
//
// 双模式行为（由主程序启动时调用 HttpStatus::setSpaMode() 决定）：
//   SPA 模式（Vue Router 等前端路由）  → HTTP 200 + X-Business-Code 头（F12 可见）
//                                        避免浏览器拦截 4xx/5xx，前端路由自行处理重定向/错误提示
//   非 SPA 模式（纯 API / 嵌入式）     → HTTP 层真实状态码（401/403/404 等）
//                                        前端直接感知 HTTP 状态，适合非浏览器客户端
//
// 兼容原有接口：
//   include AjaxResult.h 后，RESP_OK / RESP_ERR / RESP_401 完全保留，不受影响。
//
// 快捷宏：
//   RESP_CODE(cb, code, msg)        — 语义化响应（自动适配模式）
//   RESP_CODE_DATA(cb, code, msg, data)
//   RESP_400/403/404/422/429/500/503/201
//
// 二进制风格别名（全局顺序编号，与 constexpr 等价）：
//   _10111 = 400   _11000 = 401   _11010 = 403   _11011 = 404
//   _110011 = 500  _110110 = 503
#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>
#include <mutex>
#include "AjaxResult.h"   // 兼容：原有 RESP_OK / RESP_ERR / RESP_401 仍可用

namespace HttpStatus {

    // ════════════════════════════════════════════════════════════════════════════
    // HTTP 标准状态码（constexpr 常量，编译期可用）
    // ════════════════════════════════════════════════════════════════════════════

    // ── 1xx 信息性 ──────────────────────────────────────────────────────────
    constexpr int CONTINUE              = 100;  // 100 继续 | 客户端继续其请求
    constexpr int SWITCHING_PROTOCOLS  = 101;  // 101 切换协议 | 服务器正在切换协议
    constexpr int PROCESSING            = 102;  // 102 处理中 | 服务器正在处理请求
    constexpr int EARLY_HINTS          = 103;  // 103 早期提示 | 服务器返回提示信息

    // ── 2xx 成功 ────────────────────────────────────────────────────────────
    constexpr int OK                   = 200;  // 200 成功 | 请求成功
    constexpr int CREATED             = 201;  // 201 已创建 | 资源已创建
    constexpr int ACCEPTED            = 202;  // 202 已接受 | 请求已接受但处理未完成
    constexpr int NON_AUTH_INFO        = 203;  // 203 非授权信息 | 服务器返回非授权信息
    constexpr int NO_CONTENT           = 204;  // 204 无内容 | 请求成功但无返回内容
    constexpr int RESET_CONTENT        = 205;  // 205 重置内容 | 请求成功需重置文档视图
    constexpr int PARTIAL_CONTENT      = 206;  // 206 部分内容 | 服务器返回部分内容
    constexpr int MULTI_STATUS         = 207;  // 207 多状态 | 多种状态响应
    constexpr int ALREADY_REPORTED     = 208;  // 208 已报告 | 成员已在之前响应中报告
    constexpr int IM_USED             = 226;  // 226 IM已使用 | 服务器已完成请求

    // ── 3xx 重定向 ──────────────────────────────────────────────────────────
    constexpr int MULTIPLE_CHOICES     = 300;  // 300 多种选择 | 请求有多种选择
    constexpr int MOVED_PERMANENTLY    = 301;  // 301 永久移动 | 资源永久移动到新位置
    constexpr int FOUND                 = 302;  // 302 临时移动 | 资源临时移动到新位置
    constexpr int SEE_OTHER            = 303;  // 303 查看其他 | 重定向到其他资源
    constexpr int NOT_MODIFIED         = 304;  // 304 未修改 | 资源未修改使用缓存
    constexpr int USE_PROXY            = 305;  // 305 使用代理 | 必须通过代理访问
    constexpr int TEMPORARY_REDIRECT    = 307;  // 307 临时重定向 | 临时重定向保持方法
    constexpr int PERMANENT_REDIRECT   = 308;  // 308 永久重定向 | 永久重定向保持方法

    // ── 4xx 客户端错误 ──────────────────────────────────────────────────────
    constexpr int BAD_REQUEST          = 400;  // 400 错误请求 | 请求参数有误
    constexpr int UNAUTHORIZED         = 401;  // 401 未授权 | 未登录或登录已过期
    constexpr int PAYMENT_REQUIRED     = 402;  // 402 需要支付 | 预留状态码
    constexpr int FORBIDDEN            = 403;  // 403 禁止访问 | 无权限访问该资源
    constexpr int NOT_FOUND            = 404;  // 404 未找到 | 资源不存在
    constexpr int METHOD_NOT_ALLOWED    = 405;  // 405 方法不允许 | 请求方法不支持
    constexpr int NOT_ACCEPTABLE       = 406;  // 406 不可接受 | 请求格式不支持
    constexpr int PROXY_AUTH_REQUIRED  = 407;  // 407 需要代理认证 | 需通过代理认证
    constexpr int REQUEST_TIMEOUT      = 408;  // 408 请求超时 | 服务器等待超时
    constexpr int CONFLICT            = 409;  // 409 冲突 | 请求与服务器状态冲突
    constexpr int GONE                = 410;  // 410 已删除 | 资源已永久删除
    constexpr int LENGTH_REQUIRED      = 411;  // 411 需要长度 | 请求需Content-Length头
    constexpr int PRECONDITION_FAILED  = 412;  // 412 前提条件失败 | 请求前提条件不满足
    constexpr int PAYLOAD_TOO_LARGE    = 413;  // 413 请求体过大 | 提交数据超出限制
    constexpr int URI_TOO_LONG         = 414;  // 414 请求URI过长 | URL超出长度限制
    constexpr int UNSUPPORTED_MEDIA    = 415;  // 415 不支持的媒体类型 | 媒体类型不支持
    constexpr int RANGE_NOT_SATISFIABLE= 416;  // 416 请求范围不满足 | 请求范围无效
    constexpr int EXPECTATION_FAILED   = 417;  // 417 预期失败 | 服务器无法满足Expect头
    constexpr int IM_A_TEAPOT         = 418;  // 418 我是一个茶壶 | 超文本咖啡壶协议
    constexpr int MISDIRECTED_REQUEST  = 421;  // 421 误导请求 | 请求指向错误服务器
    constexpr int UNPROCESSABLE       = 422;  // 422 无法处理 | 请求格式正确但语义错误
    constexpr int LOCKED              = 423;  // 423 已锁定 | 资源被锁定无法访问
    constexpr int FAILED_DEPENDENCY   = 424;  // 424 依赖失败 | 请求依赖失败
    constexpr int TOO_EARLY          = 425;  // 425 过早 | TLS握手过早
    constexpr int UPGRADE_REQUIRED     = 426;  // 426 需要升级 | 需升级协议
    constexpr int PRECONDITION_REQUIRED= 428;  // 428 需要前提条件 | 需提供If-Match头
    constexpr int TOO_MANY_REQUESTS   = 429;  // 429 请求过多 | 请求频率超限
    constexpr int HEADER_TOO_LARGE     = 431;  // 431 请求头过大 | 请求头超出长度限制
    constexpr int LEGAL_UNAVAILABLE   = 451;  // 451 法律原因不可用 | 因法律原因不可用

    // ── 5xx 服务端错误 ──────────────────────────────────────────────────────
    constexpr int INTERNAL_ERROR      = 500;  // 500 服务器内部错误 | 服务器处理出错
    constexpr int NOT_IMPLEMENTED      = 501;  // 501 未实现 | 服务器不支持该功能
    constexpr int BAD_GATEWAY         = 502;  // 502 错误网关 | 上游服务器返回错误
    constexpr int SERVICE_UNAVAILABLE  = 503;  // 503 服务不可用 | 服务暂时不可用
    constexpr int GATEWAY_TIMEOUT      = 504;  // 504 网关超时 | 上游服务器响应超时
    constexpr int HTTP_VER_UNSUPPORTED = 505;  // 505 HTTP版本不支持 | 协议版本不支持
    constexpr int VARIANT_ALSO_NEG     = 506;  // 506 变体也协商 | 循环引用导致协商失败
    constexpr int INSUFFICIENT_STORAGE = 507;  // 507 存储空间不足 | 服务器存储空间不足
    constexpr int LOOP_DETECTED       = 508;  // 508 检测到循环 | 无限循环检测
    constexpr int NOT_EXTENDED         = 510;  // 510 未扩展 | 需要更多扩展
    constexpr int NETWORK_AUTH_REQUIRED= 511;  // 511 需要网络认证 | 需通过网络认证

    // ════════════════════════════════════════════════════════════════════════════
    // 模式配置（运行时切换，线程安全）
    //
    // 约定：由主程序在启动时调用一次 setSpaMode()，之后所有响应宏自动适配，无需重复判断。
    // 线程安全：通过 mutex 保护写操作；读取 spaMode() 为原子读（bool 读取本身原子）。
    // ════════════════════════════════════════════════════════════════════════════

    // false = 非 SPA 模式：HTTP 层返回真实状态码，JSON body 含业务码
    // true  = SPA 模式：HTTP 层固定 200，JSON body 含业务码 + X-Business-Code 头
    inline bool _spa_mode = false;

    inline std::mutex& _mtx_instance() {
        static std::mutex m;
        return m;
    }
    inline std::mutex& _mtx = _mtx_instance();

    // 获取当前模式
    inline bool spaMode() {
        return _spa_mode;
    }

    // 设置模式（通常只在启动时调用一次）
    inline void setSpaMode(bool v) {
        std::lock_guard<std::mutex> l(_mtx);
        _spa_mode = v;
    }

    // ════════════════════════════════════════════════════════════════════════════
    // 工具函数
    // ════════════════════════════════════════════════════════════════════════════

    // 业务码 → 默认中文说明（用于 msg 参数为空时自动填充）
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

    // 业务码 → drogon HTTP 状态码枚举
    // 注意：这里只映射常见错误（400/401/403/404/500），其余 4xx 统一归为 400。
    // 这样非 SPA 模式下前端能感知大多数错误，同时避免枚举缺失导致的编译问题。
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
// 响应宏（自动根据前端模式选择策略）
// ════════════════════════════════════════════════════════════════════════════

// ── JSON body 构造 ─────────────────────────────────────────────────────────
// { "code": N, "msg": "..." } 或 { "code": N, "msg": "...", "data": {...} }
namespace HttpResult {
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
    inline Json::Value fail(int code, const std::string& msg = "") {
        return make(code, msg);
    }
}

// ── 核心自动响应宏 ─────────────────────────────────────────────────────────
// 模式自动选择逻辑：
//   SPA 模式：     setStatusCode(k200OK) + addHeader("X-Business-Code", N)
//                  浏览器收到 200，不会拦截；F12 响应头可见业务码，前端自行处理
//   非 SPA 模式：   setStatusCode(toHttpCode(N))
//                  前端直接通过 HTTP 状态码判断（axios interceptor / fetch status）
#define RESP_CODE(cb, code, msg) \
    do { \
        auto __rc = drogon::HttpResponse::newHttpJsonResponse(HttpResult::make((code), std::string(msg))); \
        if (HttpStatus::spaMode()) { \
            __rc->addHeader("X-Business-Code", std::to_string(code)); \
        } else { \
            __rc->setStatusCode(HttpStatus::toHttpCode(code)); \
        } \
        (cb)(__rc); \
    } while(0)

// 带 data 的版本：JSON body 额外包含 data 字段
#define RESP_CODE_DATA(cb, code, msg, data) \
    do { \
        auto __rcd = drogon::HttpResponse::newHttpJsonResponse(HttpResult::make((code), std::string(msg), data)); \
        if (HttpStatus::spaMode()) { \
            __rcd->addHeader("X-Business-Code", std::to_string(code)); \
        } else { \
            __rcd->setStatusCode(HttpStatus::toHttpCode(code)); \
        } \
        (cb)(__rcd); \
    } while(0)

// ── 常用快捷宏（基于 RESP_CODE，行为完全一致）────────────────────────────────
#define RESP_400(cb, msg)  RESP_CODE(cb, HttpStatus::BAD_REQUEST,       msg)
#define RESP_403(cb)       RESP_CODE(cb, HttpStatus::FORBIDDEN,         "无操作权限")
#define RESP_404(cb, msg)  RESP_CODE(cb, HttpStatus::NOT_FOUND,         msg)
#define RESP_422(cb, msg)  RESP_CODE(cb, HttpStatus::UNPROCESSABLE,     msg)
#define RESP_429(cb)       RESP_CODE(cb, HttpStatus::TOO_MANY_REQUESTS, "请求过于频繁")
#define RESP_500(cb, msg)  RESP_CODE(cb, HttpStatus::INTERNAL_ERROR,    msg)
#define RESP_503(cb)       RESP_CODE(cb, HttpStatus::SERVICE_UNAVAILABLE, "服务暂不可用")
#define RESP_201(cb, data) RESP_CODE_DATA(cb, HttpStatus::CREATED, "创建成功", data)

// ── #define 风格别名（全局顺序编号，与 constexpr 完全等价）────────────────────
//
// 命名规则：_二进制序号  HTTP码
// 序号 = 全局第 N 个标准 HTTP 码（二进制，便于索引）
// 示例：_11011 = 404（第 27 个状态码，二进制 11011）
//
// 全局顺序映射：
//   1xx  信息性:  1-4   (1=100, 10=101, 11=102, 100=103)
//   2xx  成功:    5-14  (101=200, 110=201, ... 1110=226)
//   3xx  重定向:  15-22 (1111=300, 10000=301, ... 10110=308)
//   4xx  客户端:  23-50 (10111=400, 11000=401, ... 110010=451)
//   5xx  服务端:  51-61 (110011=500, 110100=501, ... 111101=511)
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
