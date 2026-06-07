/**
 * @file RequestTracing.h
 * @brief 请求链路追踪中间件
 * 
 * 功能概述：
 *   - 请求 ID 生成：为每个请求生成唯一的追踪 ID
 *   - 链路追踪：通过 X-Request-ID 追踪请求的完整链路
 *   - ID 传递：在请求和响应中传递 X-Request-ID
 *   - 日志关联：便于在日志中关联同一请求的所有操作
 * 
 * 工作流程：
 *   1. 检查请求是否已携带 X-Request-ID
 *   2. 如果没有，自动生成一个唯一的 ID
 *   3. 将 ID 添加到请求头
 *   4. 处理请求
 *   5. 将 ID 添加到响应头
 * 
 * 使用示例：
 *   // 在 drogon 中注册中间件
 *   app.registerMiddleware<RequestTracing>();
 *   
 *   // 在日志中使用 X-Request-ID
 *   auto rid = req->getHeader("X-Request-ID");
 *   LOG_INFO << "[" << rid << "] Processing request";
 * 
 * 请求头：
 *   - X-Request-ID: 请求追踪 ID（可选，如果不提供则自动生成）
 * 
 * 响应头：
 *   - X-Request-ID: 请求追踪 ID（自动添加）
 * 
 * ID 格式：
 *   - 32 位十六进制字符串
 *   - 由两个 64 位随机数组成
 *   - 格式：xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 * 
 * 特性：
 *   - 自动生成：如果请求未提供 ID，自动生成
 *   - 链路传递：ID 在请求和响应中传递
 *   - 日志关联：便于在日志中关联同一请求
 *   - 分布式追踪：支持微服务架构中的链路追踪
 *   - 性能：极低的性能开销
 * 
 * 应用场景：
 *   - 问题诊断：通过 ID 快速定位问题
 *   - 性能分析：追踪请求的完整链路
 *   - 审计日志：记录请求的完整过程
 *   - 分布式追踪：在微服务中追踪请求
 */

#pragma once
#include <drogon/HttpMiddleware.h>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>

/**
 * @class RequestTracing
 * @brief 请求链路追踪中间件
 * 
 * 为每个请求生成或传递唯一的追踪 ID，便于追踪请求的完整链路。
 */
class RequestTracing : public drogon::HttpMiddleware<RequestTracing> {
public:
    void invoke(const drogon::HttpRequestPtr &req,
                drogon::MiddlewareNextCallback &&next,
                drogon::MiddlewareCallback &&cb) override {
        std::string rid = req->getHeader("X-Request-ID");
        if (rid.empty()) rid = generate();
        req->addHeader("X-Request-ID", rid);
        next([rid, cb = std::move(cb)](const drogon::HttpResponsePtr &resp) mutable {
            resp->addHeader("X-Request-ID", rid);
            cb(resp);
        });
    }

private:
    static std::string generate() {
        static std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<uint64_t> dist;
        std::ostringstream ss;
        ss << std::hex << std::setfill('0')
           << std::setw(16) << dist(rng)
           << std::setw(16) << dist(rng);
        return ss.str();
    }
};
