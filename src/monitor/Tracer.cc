/**
 * @file Tracer.cc
 * @brief OpenTelemetry 风格链路追踪实现
 */

#include "Tracer.h"

#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

namespace Tracing {
namespace {

std::unique_ptr<Tracer>& tracerStorage() {
    static std::unique_ptr<Tracer> tracer;
    return tracer;
}

std::string generateRandomHex(size_t bytesLen) {
    static thread_local std::mt19937_64 gen{std::random_device{}()};
    std::uniform_int_distribution<unsigned int> dis(0, 255);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytesLen; ++i) {
        ss << std::setw(2) << dis(gen);
    }
    return ss.str();
}

} // namespace

Tracer& Tracer::instance() {
    auto& tracer = tracerStorage();
    if (!tracer) {
        tracer = std::make_unique<InMemoryTracer>();
    }
    return *tracer;
}

void Tracer::setInstance(std::unique_ptr<Tracer> tracer) {
    tracerStorage() = std::move(tracer);
}

Scope::Scope(const std::string& name)
    : span_(Tracer::instance().createSpan(name)) {
}

Scope::Scope(std::shared_ptr<Span> span)
    : span_(std::move(span)) {
}

Scope::~Scope() {
    if (!ended_ && span_) {
        end();
    }
}

Scope::Scope(Scope&& other) noexcept
    : span_(std::move(other.span_)), ended_(other.ended_) {
    other.ended_ = true;
}

Scope& Scope::operator=(Scope&& other) noexcept {
    if (this != &other) {
        if (!ended_ && span_) {
            end();
        }
        span_ = std::move(other.span_);
        ended_ = other.ended_;
        other.ended_ = true;
    }
    return *this;
}

void Scope::setAttribute(const std::string& key, const std::string& value) {
    if (span_) {
        span_->attributes[key] = value;
    }
}

void Scope::setTag(const std::string& key, const std::string& value) {
    if (span_) {
        span_->tags[key] = value;
    }
}

void Scope::setStatus(SpanStatus status, const std::string& errorMsg) {
    if (span_) {
        span_->status = status;
        span_->errorMessage = errorMsg;
    }
}

void Scope::addEvent(const std::string& name, const std::map<std::string, std::string>& attrs) {
    if (!span_) {
        return;
    }
    std::ostringstream ss;
    ss << name;
    if (!attrs.empty()) {
        ss << "{";
        bool first = true;
        for (const auto& [k, v] : attrs) {
            if (!first) {
                ss << ",";
            }
            ss << k << "=" << v;
            first = false;
        }
        ss << "}";
    }
    span_->tags["event." + name] = ss.str();
}

void Scope::end() {
    if (!span_ || ended_) {
        return;
    }
    span_->endTime = std::chrono::steady_clock::now();
    Tracer::instance().exportSpan(*span_);
    ended_ = true;
}

std::shared_ptr<Span> InMemoryTracer::createSpan(const std::string& name) {
    auto span = std::make_shared<Span>();
    span->name = name;
    span->traceId = generateTraceId();
    span->spanId = generateSpanId();
    span->startTime = std::chrono::steady_clock::now();
    for (const auto& [k, v] : globalAttributes_) {
        span->attributes[k] = v;
    }
    return span;
}

void InMemoryTracer::exportSpan(const Span& span) {
    spans_.push_back(span);
}

void InMemoryTracer::setGlobalAttribute(const std::string& key, const std::string& value) {
    globalAttributes_[key] = value;
}

std::string InMemoryTracer::generateTraceId() {
    return generateRandomHex(16);
}

std::string InMemoryTracer::generateSpanId() {
    return generateRandomHex(8);
}

void InMemoryTracer::clear() {
    spans_.clear();
}

JaegerExporter::JaegerExporter(const std::string& agentHost, int agentPort)
    : agentHost_(agentHost), agentPort_(agentPort) {
}

void JaegerExporter::setServiceName(const std::string& name) {
    serviceName_ = name;
}

void JaegerExporter::flush() {
    clear();
}

ZipkinExporter::ZipkinExporter(const std::string& endpoint)
    : endpoint_(endpoint) {
}

void ZipkinExporter::flush() {
    clear();
}

} // namespace Tracing
