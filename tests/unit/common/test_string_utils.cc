/**
 * @file test_string_utils.cc
 * @brief 字符串工具类单元测试
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../../src/common/StringUtils.h"

using namespace std;

TEST_CASE("字符串工具 - 基础操作") {
    SUBCASE("trim 去除首尾空白") {
        CHECK(StringUtils::trim("  hello  ") == "hello");
        CHECK(StringUtils::trim("\t\tworld\n") == "world");
        CHECK(StringUtils::trim("no spaces") == "no spaces");
    }

    SUBCASE("isEmpty 判空") {
        CHECK(StringUtils::isEmpty("") == true);
        CHECK(StringUtils::isEmpty("   ") == false); // 空白字符不算空
        CHECK(StringUtils::isEmpty("text") == false);
    }

    SUBCASE("startsWith 前缀判断") {
        CHECK(StringUtils::startsWith("hello world", "hello") == true);
        CHECK(StringUtils::startsWith("hello world", "world") == false);
        CHECK(StringUtils::startsWith("", "test") == false);
    }

    SUBCASE("endsWith 后缀判断") {
        CHECK(StringUtils::endsWith("test.txt", ".txt") == true);
        CHECK(StringUtils::endsWith("test.txt", ".jpg") == false);
    }
}

TEST_CASE("字符串工具 - 分割与连接") {
    SUBCASE("split 字符串分割") {
        auto parts = StringUtils::split("a,b,c", ",");
        CHECK(parts.size() == 3);
        CHECK(parts[0] == "a");
        CHECK(parts[1] == "b");
        CHECK(parts[2] == "c");
    }

    SUBCASE("split 支持多个分隔符") {
        auto parts = StringUtils::split("a|b,c;d", "|,");
        CHECK(parts.size() >= 4);
    }

    SUBCASE("join 字符串连接") {
        vector<string> parts = {"a", "b", "c"};
        CHECK(StringUtils::join(parts, "-") == "a-b-c");
    }
}

TEST_CASE("字符串工具 - 大小写转换") {
    CHECK(StringUtils::toLower("HELLO") == "hello");
    CHECK(StringUtils::toUpper("hello") == "HELLO");
    CHECK(StringUtils::toLower("Mixed") == "mixed");
}

TEST_CASE("字符串工具 - 替换操作") {
    SUBCASE("replace 替换单处") {
        CHECK(StringUtils::replace("hello world", "world", "cpp") == "hello cpp");
    }

    SUBCASE("replaceAll 替换所有") {
        CHECK(StringUtils::replaceAll("a-b-c-d", "-", "_") == "a_b_c_d");
    }

    SUBCASE("replace 空字符串") {
        CHECK(StringUtils::replace("hello", "x", "") == "hello");
    }
}

TEST_CASE("字符串工具 - 数字转换") {
    SUBCASE("toString 数字转字符串") {
        CHECK(StringUtils::toString(123) == "123");
        CHECK(StringUtils::toString(0) == "0");
        CHECK(StringUtils::toString(-456) == "-456");
    }

    SUBCASE("toInt 字符串转整数") {
        CHECK(StringUtils::toInt("123") == 123);
        CHECK(StringUtils::toInt("-456") == -456);
        CHECK(StringUtils::toInt("invalid") == 0); // 默认值
    }

    SUBCASE("isNumeric 数字判断") {
        CHECK(StringUtils::isNumeric("12345") == true);
        CHECK(StringUtils::isNumeric("12.34") == true);
        CHECK(StringUtils::isNumeric("12a") == false);
        CHECK(StringUtils::isNumeric("") == false);
    }
}

TEST_CASE("字符串工具 - 格式化") {
    SUBCASE("format 字符串格式化") {
        string result = StringUtils::format("User %s logged in at %d", "admin", 1234);
        CHECK(result.find("admin") != string::npos);
        CHECK(result.find("1234") != string::npos);
    }

    SUBCASE("padLeft 左填充") {
        CHECK(StringUtils::padLeft("42", 5, '0') == "00042");
        CHECK(StringUtils::padLeft("12345", 3, '0') == "12345"); // 不缩短
    }

    SUBCASE("padRight 右填充") {
        CHECK(StringUtils::padRight("hi", 5, '-') == "hi---");
    }
}

TEST_CASE("字符串工具 - SQL 注入防护") {
    SUBCASE("escapeSql 转义单引号") {
        string input = "O'Brien";
        string escaped = StringUtils::escapeSql(input);
        CHECK(escaped.find("''") != string::npos);
    }

    SUBCASE("escapeSql 转义分号") {
        CHECK(StringUtils::escapeSql("a;b").find(';') == string::npos);
    }
}

TEST_CASE("字符串工具 - URL 编解码") {
    CHECK(StringUtils::urlEncode("hello world") == "hello%20world");
    CHECK(StringUtils::urlDecode("hello%20world") == "hello world");
}

TEST_CASE("字符串工具 - JSON 安全处理") {
    SUBCASE("escapeJson 转义特殊字符") {
        string input = "He said \"Hello\"";
        string escaped = StringUtils::escapeJson(input);
        CHECK(escaped.find('\\') != string::npos);
    }

    SUBCASE("escapeJson 处理换行") {
        CHECK(StringUtils::escapeJson("line1\nline2").find('\\') != string::npos);
    }
}
