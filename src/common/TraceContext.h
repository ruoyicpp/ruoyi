#pragma once
// ════════════════════════════════════════════════════════════════════════════
// TraceContext.h — W3C Trace Context (traceparent) + 简易 Span 导出
//
// 标准: https://www.w3.org/TR/trace-context/
// traceparent: 00-<traceId[32hex]>-<spanId[16hex]>-<flags[2hex]>
//
// 用法：
//   auto ctx = TraceContext::fromRequest(req);   // 从请求头解析
//   ctx.injectResponse(resp);                    // 注入响应头 X-Trace-Id
//   auto child = ctx.childSpan("db.query");      // 创建子 span
//   child.finish("SELECT ...", 12);              // 记录完成，打印 OTLP-like 日志
// ════════════════════════════════════════════════════════════════════════════
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <openssl/rand.h>
#include <drogon/drogon.h>

class TraceContext {
public:
    std::string traceId;   // 16 字节 = 32 hex chars
    std::string spanId;    // 8  字节 = 16 hex chars
    std::string parentId;  // 父 spanId（可空）
    bool        sampled = true;

    // ── 生成 ─────────────────────────────────────────────────────────────
    static TraceContext newTrace() {
        TraceContext ctx;
        ctx.traceId  = randomHex(16);
        ctx.spanId   = randomHex(8);
        ctx.sampled  = true;
        return ctx;
    }

    // 从 HTTP 请求头解析 traceparent（无则新建）
    static TraceContext fromRequest(const drogon::HttpRequestPtr &req) {
        std::string tp = req->getHeader("traceparent");
        if (tp.empty()) tp = req->getHeader("x-trace-id");
        if (!tp.empty()) {
            auto ctx = parse(tp);
            if (!ctx.traceId.empty()) return ctx;
        }
        return newTrace();
    }

    // ── 注入响应 ─────────────────────────────────────────────────────────
    void injectResponse(const drogon::HttpResponsePtr &resp) const {
        resp->addHeader("traceparent", toTraceparent());
        resp->addHeader("X-Trace-Id",  traceId);
        resp->addHeader("X-Span-Id",   spanId);
    }

    // ── 子 Span ─────────────────────────────────────────────────────────
    struct Span {
        std::string name;
        std::string traceId;
        std::string spanId;
        std::string parentSpanId;
        std::chrono::steady_clock::time_point startTime;

        // 完成并输出 JSON span（可接入 OTLP collector）
        void finish(const std::string &attr = "", long statusCode = 0) const {
            auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            // 结构化日志（可改接 OTLP HTTP exporter）
            std::cout << "[SPAN] {\"trace\":\"" << traceId
                      << "\",\"span\":\"" << spanId
                      << "\",\"parent\":\"" << parentSpanId
                      << "\",\"name\":\"" << name
                      << "\",\"duration_us\":" << dur
                      << ",\"status\":" << statusCode;
            if (!attr.empty()) std::cout << ",\"attr\":\"" << attr << "\"";
            std::cout << "}" << std::endl;
        }
    };

    Span childSpan(const std::string &name) const {
        return Span{name, traceId, randomHex(8), spanId,
                    std::chrono::steady_clock::now()};
    }

    std::string toTraceparent() const {
        return "00-" + traceId + "-" + spanId + "-" + (sampled ? "01" : "00");
    }

    // ── 保存到请求属性（跨中间件传递）────────────────────────────────────
    void storeInRequest(const drogon::HttpRequestPtr &req) const {
        req->addHeader("x-trace-id-internal", traceId + ":" + spanId);
    }

    static TraceContext loadFromRequest(const drogon::HttpRequestPtr &req) {
        std::string v = req->getHeader("x-trace-id-internal");
        if (v.empty()) return fromRequest(req);
        auto pos = v.find(':');
        if (pos == std::string::npos) return fromRequest(req);
        TraceContext ctx;
        ctx.traceId = v.substr(0, pos);
        ctx.spanId  = v.substr(pos + 1);
        return ctx;
    }

private:
    static std::string randomHex(size_t bytes) {
        std::vector<unsigned char> buf(bytes);
        RAND_bytes(buf.data(), (int)bytes);
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        for (auto b : buf) ss << std::setw(2) << (int)b;
        return ss.str();
    }

    static TraceContext parse(const std::string &tp) {
        // 格式: 00-<32hex>-<16hex>-<2hex>
        TraceContext ctx;
        if (tp.size() < 55) return ctx;
        if (tp.substr(0, 3) != "00-") return ctx;
        ctx.traceId = tp.substr(3, 32);
        if (tp[35] != '-') return ctx;
        ctx.spanId  = tp.substr(36, 16);
        if (tp[52] != '-') return ctx;
        ctx.sampled = (tp.substr(53, 2) == "01");
        return ctx;
    }
};

// ════════════════════════════════════════════════════════════════════════════
// TraceFilter — Drogon 过滤器，自动注入 trace context
// ════════════════════════════════════════════════════════════════════════════
class TraceFilter : public drogon::HttpFilter<TraceFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&stop,
                  drogon::FilterChainCallback &&next) override {
        auto ctx = TraceContext::fromRequest(req);
        ctx.storeInRequest(req);
        next();
    }
};
