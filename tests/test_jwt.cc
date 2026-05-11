#include "doctest.h"
#include "../src/common/JwtUtils.h"
#include <thread>

// JwtUtils 配置存在 static config()，直接写入即可，绕开 loadConfig()
static void setupJwtConfig(int days = 7) {
    auto &c = JwtUtils::config();
    c.secret        = "test-secret-key-32bytes-padding!!";
    c.issuer        = "test-issuer";
    c.audience      = "test-audience";
    c.expireMinutes = 30;
    c.jwtExpireDays = days;
}

TEST_CASE("JwtUtils createToken/parseUuid roundtrip") {
    setupJwtConfig();
    std::string uuid = "test-uuid-123";
    std::string token = JwtUtils::createToken(uuid, 42, "admin", 100);
    CHECK(!token.empty());
    CHECK(JwtUtils::parseUuid(token) == uuid);
}

TEST_CASE("JwtUtils tampered token throws") {
    setupJwtConfig();
    std::string token = JwtUtils::createToken("uuid-x", 1, "u", 1);
    token.back() ^= 0x01;
    CHECK_THROWS(JwtUtils::parseUuid(token));
}

TEST_CASE("JwtUtils wrong issuer throws") {
    setupJwtConfig();
    std::string token = JwtUtils::createToken("uuid-x", 1, "u", 1);
    JwtUtils::config().issuer = "different-issuer";
    CHECK_THROWS(JwtUtils::parseUuid(token));
}
