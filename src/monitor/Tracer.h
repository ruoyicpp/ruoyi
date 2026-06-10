/**
 * @file Tracer.h
 * @brief 链路追踪 — OpenTelemetry 风格的分布式追踪实现
 * 
 * 功能概述：
 *   - 链路追踪：记录请求在系统中的完整调用链路
 *   - Span 管理：创建、导出、管理追踪跨度
 *   - 多后端支持：支持 Jaeger、Zipkin 等追踪系统
 *   - 属性标签：为 Span 添加属性和标签
 *   - 事件记录：记录 Span 中发生的事件
 * 
 * 核心概念：
 *   - Trace：完整的请求链路，由多个 Span 组成
 *   - Span：链路中的一个操作，记录开始时间、结束时间、状态等
 *   - Scope：Span 的作用域，自动管理 Span 的生命周期
 * 
 * 支持的后端：
 *   - InMemoryTracer：内存存储，用于开发和测试
 *   - JaegerExporter：导出到 Jaeger 追踪系统
 *   - ZipkinExporter：导出到 Zipkin 追踪系统
 * 
 * 使用示例：
 *   {
 *       TRACE_SCOPE("http_request");
 *       TRACE_ADD_ATTRIBUTE("method", "GET");
 *       TRACE_ADD_ATTRIBUTE("path", "/api/users");
 *       // 业务逻辑
 *   } // Scope 析构时自动结束 Span
 * 
 * @see MetricsCollector - 指标采集
 * @see JobScheduler - 定时任务调度
 */

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * @namespace Tracing
 * @brief 链路追踪命名空间
 */
namespace Tracing {

/**
 * @enum SpanStatus
 * @brief Span 状态枚举
 */
enum class SpanStatus {
    Unset,  ///< 未设置
    Ok,     ///< 成功
    Error   ///< 错误
};

/**
 * @struct Span
 * @brief 追踪跨度
 * 
 * 记录一个操作的追踪信息，包括时间、状态、属性等。
 */
struct Span {
    std::string name;                              ///< Span 名称
    std::string traceId;                           ///< 链路 ID
    std::string spanId;                            ///< Span ID
    std::string parentSpanId;                      ///< 父 Span ID
    std::chrono::steady_clock::time_point startTime;  ///< 开始时间
    std::chrono::steady_clock::time_point endTime;    ///< 结束时间
    SpanStatus status = SpanStatus::Unset;         ///< Span 状态
    std::string errorMessage;                      ///< 错误信息
    std::map<std::string, std::string> attributes; ///< 属性
    std::map<std::string, std::string> tags;       ///< 标签

    /**
     * @brief 获取 Span 耗时（微秒）
     * @return 耗时（微秒）
     */
    int64_t durationMicros() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    }
};

class Scope;

/**
 * @class Tracer
 * @brief 链路追踪器基类
 * 
 * 抽象基类，定义链路追踪的接口。
 */
class Tracer {
public:
    /**
     * @brief 获取全局追踪器实例
     * @return Tracer 引用
     */
    static Tracer& instance();

    /**
     * @brief 设置全局追踪器实例
     * @param tracer 新的追踪器实例
     */
    static void setInstance(std::unique_ptr<Tracer> tracer);

    virtual ~Tracer() = default;

    /**
     * @brief 创建新的 Span
     * @param name Span 名称
     * @return Span 指针
     */
    virtual std::shared_ptr<Span> createSpan(const std::string& name) = 0;

    /**
     * @brief 导出 Span
     * @param span 要导出的 Span
     */
    virtual void exportSpan(const Span& span) = 0;

    /**
     * @brief 设置全局属性
     * @param key 属性键
     * @param value 属性值
     */
    virtual void setGlobalAttribute(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 生成链路 ID
     * @return 链路 ID
     */
    virtual std::string generateTraceId() = 0;

    /**
     * @brief 生成 Span ID
     * @return Span ID
     */
    virtual std::string generateSpanId() = 0;
};

/**
 * @class Scope
 * @brief Span 作用域
 * 
 * 自动管理 Span 的生命周期，在析构时自动结束 Span。
 */
class Scope {
public:
    /**
     * @brief 从名称创建 Scope
     * @param name Span 名称
     */
    explicit Scope(const std::string& name);

    /**
     * @brief 从 Span 创建 Scope
     * @param span Span 指针
     */
    explicit Scope(std::shared_ptr<Span> span);

    /**
     * @brief 析构函数，自动结束 Span
     */
    ~Scope();

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&& other) noexcept;
    Scope& operator=(Scope&& other) noexcept;

    /**
     * @brief 设置属性
     * @param key 属性键
     * @param value 属性值
     */
    void setAttribute(const std::string& key, const std::string& value);

    /**
     * @brief 设置标签
     * @param key 标签键
     * @param value 标签值
     */
    void setTag(const std::string& key, const std::string& value);

    /**
     * @brief 设置状态
     * @param status Span 状态
     * @param errorMsg 错误信息（可选）
     */
    void setStatus(SpanStatus status, const std::string& errorMsg = "");

    /**
     * @brief 添加事件
     * @param name 事件名称
     * @param attrs 事件属性（可选）
     */
    void addEvent(const std::string& name, const std::map<std::string, std::string>& attrs = {});

    /**
     * @brief 手动结束 Span
     */
    void end();

    /**
     * @brief 获取 Span 引用
     * @return Span 引用
     */
    Span& span() { return *span_; }

    /**
     * @brief 获取 Span 常引用
     * @return Span 常引用
     */
    const Span& span() const { return *span_; }

    /**
     * @brief 检查 Scope 是否有效
     * @return 是否有效
     */
    bool valid() const { return static_cast<bool>(span_); }

private:
    std::shared_ptr<Span> span_;  ///< Span 指针
    bool ended_ = false;          ///< 是否已结束
};

/**
 * @class InMemoryTracer
 * @brief 内存追踪器
 * 
 * 将 Span 存储在内存中，用于开发和测试。
 */
class InMemoryTracer : public Tracer {
public:
    std::shared_ptr<Span> createSpan(const std::string& name) override;
    void exportSpan(const Span& span) override;
    void setGlobalAttribute(const std::string& key, const std::string& value) override;
    std::string generateTraceId() override;
    std::string generateSpanId() override;

    /**
     * @brief 获取所有 Span
     * @return Span 列表
     */
    const std::vector<Span>& spans() const { return spans_; }

    /**
     * @brief 清空所有 Span
     */
    void clear();

private:
    std::map<std::string, std::string> globalAttributes_;  ///< 全局属性
    std::vector<Span> spans_;                              ///< Span 列表
};

/**
 * @class JaegerExporter
 * @brief Jaeger 导出器
 * 
 * 将 Span 导出到 Jaeger 追踪系统。
 */
class JaegerExporter : public InMemoryTracer {
public:
    /**
     * @brief 构造函数
     * @param agentHost Jaeger Agent 主机（默认 localhost）
     * @param agentPort Jaeger Agent 端口（默认 6831）
     */
    JaegerExporter(const std::string& agentHost = "localhost", int agentPort = 6831);

    /**
     * @brief 设置服务名称
     * @param name 服务名称
     */
    void setServiceName(const std::string& name);

    /**
     * @brief 刷新数据到 Jaeger
     */
    void flush();

private:
    std::string agentHost_;                        ///< Agent 主机
    int agentPort_;                                ///< Agent 端口
    std::string serviceName_ = "ruoyi-cpp";        ///< 服务名称
};

/**
 * @class ZipkinExporter
 * @brief Zipkin 导出器
 * 
 * 将 Span 导出到 Zipkin 追踪系统。
 */
class ZipkinExporter : public InMemoryTracer {
public:
    /**
     * @brief 构造函数
     * @param endpoint Zipkin 端点（默认 http://localhost:9411/api/v2/spans）
     */
    explicit ZipkinExporter(const std::string& endpoint = "http://localhost:9411/api/v2/spans");

    /**
     * @brief 刷新数据到 Zipkin
     */
    void flush();

private:
    std::string endpoint_;  ///< Zipkin 端点
};

} // namespace Tracing

/**
 * @def TRACE_SCOPE
 * @brief 创建追踪作用域
 * @param name Span 名称
 */
#define TRACE_SCOPE(name) Tracing::Scope _tracer_scope(name)

/**
 * @def TRACE_ADD_ATTRIBUTE
 * @brief 添加追踪属性
 * @param key 属性键
 * @param value 属性值
 */
#define TRACE_ADD_ATTRIBUTE(key, value) _tracer_scope.setAttribute(key, value)

/**
 * @def TRACE_ADD_TAG
 * @brief 添加追踪标签
 * @param key 标签键
 * @param value 标签值
 */
#define TRACE_ADD_TAG(key, value) _tracer_scope.setTag(key, value)
