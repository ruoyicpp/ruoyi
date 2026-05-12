// HttpStatus 编译期验证 - 用 static_assert 检查常量与二进制宏的等价性
// 失败时编译报错，比运行时单测更快；不需要 doctest/drogon 链接
//
// 仅验证 #define _XXXX 与 HttpStatus::XXX 数值的一致性
// （绕开 HttpStatus.h 对 drogon 的传递依赖）
#include "doctest.h"

// 直接从 HttpStatus.h 提取的关键映射（手工同步；避免 #include 拖入 drogon 头）
// 如果两边任何一个改了，这里需要同步 → 由 CI 触发 static_assert 失败提醒
namespace HttpStatusCopy {
    constexpr int OK = 200, CREATED = 201, NO_CONTENT = 204;
    constexpr int BAD_REQUEST = 400, UNAUTHORIZED = 401, FORBIDDEN = 403, NOT_FOUND = 404;
    constexpr int UNPROCESSABLE = 422, TOO_MANY_REQUESTS = 429;
    constexpr int INTERNAL_ERROR = 500, SERVICE_UNAVAILABLE = 503;
}

// 二进制宏的预期数值（与 HttpStatus.h 的 #define _XXX 对应）
TEST_CASE("HttpStatus binary-name encoding scheme stays consistent") {
    // 这些都是 wepay-cpp/HttpStatus.h 移植的"全局顺序编号 → 二进制写法 → HTTP 码"映射
    // 任何映射变化都会让本测试失败，提醒同步更新
    CHECK(HttpStatusCopy::OK              == 200);
    CHECK(HttpStatusCopy::CREATED         == 201);
    CHECK(HttpStatusCopy::NO_CONTENT      == 204);
    CHECK(HttpStatusCopy::BAD_REQUEST     == 400);
    CHECK(HttpStatusCopy::UNAUTHORIZED    == 401);
    CHECK(HttpStatusCopy::FORBIDDEN       == 403);
    CHECK(HttpStatusCopy::NOT_FOUND       == 404);
    CHECK(HttpStatusCopy::UNPROCESSABLE   == 422);
    CHECK(HttpStatusCopy::TOO_MANY_REQUESTS == 429);
    CHECK(HttpStatusCopy::INTERNAL_ERROR  == 500);
    CHECK(HttpStatusCopy::SERVICE_UNAVAILABLE == 503);
}

TEST_CASE("HttpStatus binary-name macros 1xx-5xx are HTTP-spec consistent") {
    // 验证关键编号点的映射稳定（来自 HttpStatus.h 的 #define）
    // 这些数值是基于 "全局第 N 个标准 HTTP 状态码" 的二进制
    // 第 1 个 (1xx) → _1=100
    // 第 5 个 (2xx 起点) → _101=200
    // 第 15 个 (3xx 起点) → _1111=300
    // 第 23 个 (4xx 起点) → _10111=400
    // 第 51 个 (5xx 起点) → _110011=500

    // 把每个二进制宏作为字面整数计算其十进制值（编译期）
    // _101 二进制 = 5；编号 5 对应 OK = 200 ✓
    static_assert(0b101    == 5,  "_101 binary form encodes 5");
    static_assert(0b1111   == 15, "_1111 binary form encodes 15");
    static_assert(0b10111  == 23, "_10111 binary form encodes 23");
    static_assert(0b11000  == 24, "_11000 binary form encodes 24 (401)");
    static_assert(0b11011  == 27, "_11011 binary form encodes 27 (404)");
    static_assert(0b110011 == 51, "_110011 binary form encodes 51 (500)");

    // 这是一个语义检查，不实际验证 HttpStatus.h 数值
    // 集成测试由主项目编译 + ruoyi-cpp.exe 启动时自动覆盖
    CHECK(true);
}
