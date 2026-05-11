#include "doctest.h"
#include "../src/common/SecurityUtils.h"

// SecurityUtils.cc 内部用随机 salt + PBKDF2-SHA256，每次返回不同密文
// matchesPassword 解析嵌入 salt 验证

TEST_CASE("PBKDF2 same password produces verifiable hash") {
    std::string h = SecurityUtils::encryptPassword("admin123");
    CHECK(!h.empty());
    CHECK(SecurityUtils::matchesPassword("admin123", h));
}

TEST_CASE("PBKDF2 different password fails verification") {
    std::string h = SecurityUtils::encryptPassword("admin123");
    CHECK(!SecurityUtils::matchesPassword("wrong", h));
}

TEST_CASE("PBKDF2 random salt: same input → different ciphertext") {
    std::string h1 = SecurityUtils::encryptPassword("admin123");
    std::string h2 = SecurityUtils::encryptPassword("admin123");
    CHECK(h1 != h2);  // 随机 salt
    // 但都能验证通过
    CHECK(SecurityUtils::matchesPassword("admin123", h1));
    CHECK(SecurityUtils::matchesPassword("admin123", h2));
}

TEST_CASE("parseLong handles bad input") {
    CHECK(SecurityUtils::parseLong("123") == 123);
    CHECK(SecurityUtils::parseLong("") == 0);
    CHECK(SecurityUtils::parseLong("abc", -1) == -1);
    CHECK(SecurityUtils::parseLong("99999999999999999999", 0) == 0);
}

TEST_CASE("isAdmin checks user id constant") {
    CHECK(SecurityUtils::isAdmin(1));    // ADMIN_USER_ID = 1
    CHECK(!SecurityUtils::isAdmin(2));
}
