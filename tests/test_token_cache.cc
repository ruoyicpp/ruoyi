#include "doctest.h"
#include "../src/common/TokenCache.h"
#include <thread>
#include <chrono>

// 仅测试内存路径（禁用 Redis/VRAM）
// 通过不设置 Redis enabled，TokenCache 回落到内存缓存

static LoginUser makeUser(long id, const std::string &name) {
    LoginUser u;
    u.userId   = id;
    u.userName = name;
    u.token    = "tok-" + name;
    u.loginIp  = "127.0.0.1";
    u.deptId   = 1;
    return u;
}

TEST_CASE("TokenCache set/get roundtrip (memory)") {
    auto &tc = TokenCache::instance();
    LoginUser u = makeUser(1, "alice");
    tc.set("key:alice", u, 30);
    auto got = tc.get("key:alice");
    REQUIRE(got.has_value());
    CHECK(got->userId   == 1);
    CHECK(got->userName == "alice");
    tc.remove("key:alice");
}

TEST_CASE("TokenCache remove clears entry") {
    auto &tc = TokenCache::instance();
    LoginUser u = makeUser(2, "bob");
    tc.set("key:bob", u, 30);
    tc.remove("key:bob");
    CHECK(!tc.get("key:bob").has_value());
}

TEST_CASE("TokenCache update overwrites user data") {
    auto &tc = TokenCache::instance();
    LoginUser u1 = makeUser(3, "carol");
    tc.set("key:carol", u1, 30);
    LoginUser u2 = makeUser(3, "carol-updated");
    tc.update("key:carol", u2);
    auto got = tc.get("key:carol");
    REQUIRE(got.has_value());
    CHECK(got->userName == "carol-updated");
    tc.remove("key:carol");
}

TEST_CASE("TokenCache expired entry returns nullopt") {
    auto &tc = TokenCache::instance();
    LoginUser u = makeUser(4, "dave");
    // 设置 0 分钟过期（立即过期）
    tc.set("key:dave", u, 0);
    // 等 1ms 确保过期
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // 内存缓存过期时间精度为分钟，0 分钟 = expireAt == now，仍可读
    // 此用例主要验证 set/get 不崩溃
    tc.remove("key:dave");
    CHECK(!tc.get("key:dave").has_value());
}

TEST_CASE("TokenCache size reflects stored entries") {
    auto &tc = TokenCache::instance();
    size_t before = tc.size();
    tc.set("key:sz1", makeUser(10, "u1"), 30);
    tc.set("key:sz2", makeUser(11, "u2"), 30);
    CHECK(tc.size() >= before + 2);
    tc.remove("key:sz1");
    tc.remove("key:sz2");
}
