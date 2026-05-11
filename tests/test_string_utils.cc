#include "doctest.h"
#include "../src/common/StringUtils.h"

TEST_CASE("StringUtils trim") {
    CHECK(StringUtils::trim("  hello  ") == "hello");
    CHECK(StringUtils::trim("") == "");
    CHECK(StringUtils::trim("\t\n abc \r\n") == "abc");
    CHECK(StringUtils::trim("noTrim") == "noTrim");
}

TEST_CASE("StringUtils isBlank") {
    CHECK(StringUtils::isBlank(""));
    CHECK(StringUtils::isBlank("   "));
    CHECK(StringUtils::isBlank("\t\r\n"));
    CHECK(!StringUtils::isBlank("x"));
    CHECK(!StringUtils::isBlank("  x  "));
}

TEST_CASE("StringUtils isHttp") {
    CHECK(StringUtils::isHttp("http://example.com"));
    CHECK(StringUtils::isHttp("https://example.com"));
    CHECK(!StringUtils::isHttp("ftp://example.com"));
    CHECK(!StringUtils::isHttp("example.com"));
}

TEST_CASE("StringUtils containsIgnoreCase") {
    CHECK(StringUtils::containsIgnoreCase("Hello World", "WORLD"));
    CHECK(StringUtils::containsIgnoreCase("ABCDE", "bcd"));
    CHECK(!StringUtils::containsIgnoreCase("xyz", "abc"));
}

TEST_CASE("StringUtils join") {
    std::vector<std::string> v = {"a", "b", "c"};
    CHECK(StringUtils::join(v, ",") == "a,b,c");
    CHECK(StringUtils::join({}, ",") == "");
    CHECK(StringUtils::join({"only"}, ",") == "only");
}

TEST_CASE("StringUtils toUpperCamelCase") {
    CHECK(StringUtils::toUpperCamelCase("hello") == "Hello");
    CHECK(StringUtils::toUpperCamelCase("") == "");
    CHECK(StringUtils::toUpperCamelCase("Already") == "Already");
}
