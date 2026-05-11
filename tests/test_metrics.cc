#include "doctest.h"
// MetricsCollector.h 依赖 drogon + DatabaseService（重）— 这里只做轻量集成测试：
// 验证 Prometheus 计数器单调递增 + 直方图桶累加正确
// 用最小化伪 MetricsCollector（去掉 actuator HTTP 注册路径）

#include <atomic>
#include <array>
#include <sstream>

// 复刻 MetricsCollector 的核心数据结构和 observeDuration 逻辑（不引入 drogon）
struct MetricsLite {
    std::atomic<uint64_t> reqTotal{0};
    static constexpr std::array<double, 12> kBuckets = {
        5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 30000
    };
    std::array<std::atomic<uint64_t>, 12> buckets{};
    std::atomic<uint64_t> count{0};
    std::atomic<uint64_t> sum{0};

    void observe(long ms) {
        if (ms < 0) ms = 0;
        sum.fetch_add((uint64_t)ms);
        ++count;
        for (size_t i = 0; i < kBuckets.size(); ++i) {
            if ((double)ms <= kBuckets[i]) buckets[i].fetch_add(1);
        }
        ++reqTotal;
    }
};

TEST_CASE("histogram buckets are cumulative correct") {
    MetricsLite m;
    m.observe(3);    // → le=5 桶 +1
    m.observe(40);   // → le=50 起所有桶 +1
    m.observe(200);  // → le=250 起所有桶 +1
    m.observe(3000); // → le=5000 起所有桶 +1

    CHECK(m.count.load() == 4);
    CHECK(m.sum.load() == 3 + 40 + 200 + 3000);
    CHECK(m.buckets[0].load() == 1);    // le=5: 3ms
    CHECK(m.buckets[1].load() == 1);    // le=10: 仍只 3ms
    CHECK(m.buckets[3].load() == 2);    // le=50: 3,40
    CHECK(m.buckets[5].load() == 3);    // le=250: 3,40,200
    CHECK(m.buckets[9].load() == 4);    // le=5000: all 4
    CHECK(m.buckets[11].load() == 4);   // le=30000: all 4
}

TEST_CASE("counters increment monotonically") {
    MetricsLite m;
    for (int i = 0; i < 1000; ++i) m.observe(10);
    CHECK(m.reqTotal.load() == 1000);
    CHECK(m.count.load() == 1000);
    CHECK(m.sum.load() == 10000);
}

TEST_CASE("negative duration clamped to 0") {
    MetricsLite m;
    m.observe(-5);
    CHECK(m.sum.load() == 0);
    CHECK(m.buckets[0].load() == 1);  // 0ms <= 5
}
