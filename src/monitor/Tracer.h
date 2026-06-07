/**
 * @file Tracer.h
 * @brief OpenTelemetry 风格链路追踪抽象
 */

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Tracing {

enum class SpanStatus {
    Unset,
    Ok,
    Error
};

struct Span {
    std::string name;
    std::string traceId;
    std::string spanId;
    std::string parentSpanId;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    SpanStatus status = SpanStatus::Unset;
    std::string errorMessage;
    std::map<std::string, std::string> attributes;
    std::map<std::string, std::string> tags;

    int64_t durationMicros() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    }
};

class Scope;

class Tracer {
public:
    static Tracer& instance();
    static void setInstance(std::unique_ptr<Tracer> tracer);

    virtual ~Tracer() = default;

    virtual std::shared_ptr<Span> createSpan(const std::string& name) = 0;
    virtual void exportSpan(const Span& span) = 0;
    virtual void setGlobalAttribute(const std::string& key, const std::string& value) = 0;
    virtual std::string generateTraceId() = 0;
    virtual std::string generateSpanId() = 0;
};

class Scope {
public:
    explicit Scope(const std::string& name);
    explicit Scope(std::shared_ptr<Span> span);
    ~Scope();

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&& other) noexcept;
    Scope& operator=(Scope&& other) noexcept;

    void setAttribute(const std::string& key, const std::string& value);
    void setTag(const std::string& key, const std::string& value);
    void setStatus(SpanStatus status, const std::string& errorMsg = "");
    void addEvent(const std::string& name, const std::map<std::string, std::string>& attrs = {});
    void end();

    Span& span() { return *span_; }
    const Span& span() const { return *span_; }
    bool valid() const { return static_cast<bool>(span_); }

private:
    std::shared_ptr<Span> span_;
    bool ended_ = false;
};

class InMemoryTracer : public Tracer {
public:
    std::shared_ptr<Span> createSpan(const std::string& name) override;
    void exportSpan(const Span& span) override;
    void setGlobalAttribute(const std::string& key, const std::string& value) override;
    std::string generateTraceId() override;
    std::string generateSpanId() override;

    const std::vector<Span>& spans() const { return spans_; }
    void clear();

private:
    std::map<std::string, std::string> globalAttributes_;
    std::vector<Span> spans_;
};

class JaegerExporter : public InMemoryTracer {
public:
    JaegerExporter(const std::string& agentHost = "localhost", int agentPort = 6831);

    void setServiceName(const std::string& name);
    void flush();

private:
    std::string agentHost_;
    int agentPort_;
    std::string serviceName_ = "ruoyi-cpp";
};

class ZipkinExporter : public InMemoryTracer {
public:
    explicit ZipkinExporter(const std::string& endpoint = "http://localhost:9411/api/v2/spans");

    void flush();

private:
    std::string endpoint_;
};

} // namespace Tracing

#define TRACE_SCOPE(name) Tracing::Scope _tracer_scope(name)
#define TRACE_ADD_ATTRIBUTE(key, value) _tracer_scope.setAttribute(key, value)
#define TRACE_ADD_TAG(key, value) _tracer_scope.setTag(key, value)
