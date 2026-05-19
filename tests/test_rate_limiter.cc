#include "doctest.h"
#include "../src/common/RateLimiter.h"
#include <thread>
#include <chrono>

static void setupLimiter(int maxReq = 5, int windowSec = 2, int banSec = 4) {
    RateLimiter::Config cfg;
    cfg.enabled       = true;
    cfg.maxRequests   = maxReq;
    cfg.windowSeconds = windowSec;
    cfg.banSeconds    = banSec;
    RateLimiter::instance().configure(cfg);
}

TEST_CASE("RateLimiter allows requests under limit") {
    setupLimiter(10, 60);
    for (int i = 0; i < 10; ++i)
        CHECK(!RateLimiter::instance().isBlocked("1.2.3.4"));
}

TEST_CASE("RateLimiter blocks after exceeding limit") {
    setupLimiter(3, 60, 300);
    RateLimiter::instance().cleanup();  // 清旧状态
    std::string ip = "10.0.0.1";
    for (int i = 0; i < 3; ++i)
        RateLimiter::instance().isBlocked(ip);
    // 第 4 次触发封禁
    bool blocked = RateLimiter::instance().isBlocked(ip);
    CHECK(blocked);
}

TEST_CASE("RateLimiter whitelist always passes") {
    RateLimiter::Config cfg;
    cfg.enabled       = true;
    cfg.maxRequests   = 1;
    cfg.windowSeconds = 60;
    cfg.banSeconds    = 300;
    cfg.whitelist     = {"192.168.1.1"};
    RateLimiter::instance().configure(cfg);
    for (int i = 0; i < 20; ++i)
        CHECK(!RateLimiter::instance().isBlocked("192.168.1.1"));
}

TEST_CASE("RateLimiter disabled allows all") {
    RateLimiter::Config cfg;
    cfg.enabled = false;
    RateLimiter::instance().configure(cfg);
    for (int i = 0; i < 100; ++i)
        CHECK(!RateLimiter::instance().isBlocked("9.9.9.9"));
    // 恢复 enabled 以免影响其他测试
    cfg.enabled = true; cfg.maxRequests = 200; cfg.windowSeconds = 60;
    RateLimiter::instance().configure(cfg);
}
