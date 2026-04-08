#pragma once
#include <drogon/HttpMiddleware.h>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>

// X-Request-ID 链路追踪中间件
// 若请求已携带 X-Request-ID 则沿用，否则自动生成并写入响应头
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
