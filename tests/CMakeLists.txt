# =============================================================================
# tests/CMakeLists.txt — ruoyi-cpp 单元测试（doctest header-only）
#
# 设计原则：
#   1. self-contained 测试（test_metrics / test_string_utils）单独编译，零外部依赖
#   2. 重依赖测试（test_jwt / test_security）需要 OpenSSL+jwt-cpp+drogon，按需启用
#   3. 由主 CMakeLists.txt 通过 option(RUOYI_BUILD_TESTS ON) 触发 add_subdirectory(tests)
#
# 用法：
#   cmake -B build -DRUOYI_BUILD_TESTS=ON
#   cmake --build build --target ruoyi-tests
#   ctest --test-dir build --output-on-failure
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# 独立配置模式（cmake -S tests）需要 project() + 计算项目根
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ruoyi-tests CXX)
    set(RUOYI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
else()
    set(RUOYI_ROOT "${CMAKE_SOURCE_DIR}")
endif()

enable_testing()

# ── 编译选项：单测可以容忍未使用变量 ───────────────────────────────────────
if (NOT MSVC)
    add_compile_options(-Wno-unused-variable -Wno-unused-function)
endif()

set(TEST_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RUOYI_ROOT}/src
)

# ── 1) self-contained 测试 — 无外部依赖 ─────────────────────────────────────
add_executable(test_metrics
    test_main.cc
    test_metrics.cc
)
target_include_directories(test_metrics PRIVATE ${TEST_INCLUDES})
target_compile_features(test_metrics PRIVATE cxx_std_17)
add_test(NAME metrics COMMAND $<TARGET_FILE:test_metrics>)

add_executable(test_string_utils
    test_main.cc
    test_string_utils.cc
)
target_include_directories(test_string_utils PRIVATE ${TEST_INCLUDES})
target_compile_features(test_string_utils PRIVATE cxx_std_17)
add_test(NAME string_utils COMMAND $<TARGET_FILE:test_string_utils>)

# ── 2) 需要 OpenSSL 的测试 ──────────────────────────────────────────────────
# test_security 链接主项目的 SecurityUtils.cc + OpenSSL
find_package(OpenSSL QUIET)
find_package(Drogon QUIET)
if (OpenSSL_FOUND AND Drogon_FOUND)
    add_executable(test_security
        test_main.cc
        test_security.cc
        ${RUOYI_ROOT}/src/common/SecurityUtils.cc
    )
    target_include_directories(test_security PRIVATE ${TEST_INCLUDES})
    target_compile_features(test_security PRIVATE cxx_std_20)
    # SecurityUtils.h 间接 include drogon/drogon.h，需要其头路径；
    # 实际运行时只用 OpenSSL 的 PBKDF2 API，drogon 不必链接（仅头）
    target_link_libraries(test_security PRIVATE
        Drogon::Drogon
        OpenSSL::SSL OpenSSL::Crypto
    )
    add_test(NAME security COMMAND $<TARGET_FILE:test_security>)
    set(_have_test_security TRUE)
else()
    message(STATUS "[tests] OpenSSL/Drogon not found, skipping test_security")
endif()

# test_sqlite_file_cipher：只需要 OpenSSL（不依赖 Drogon），header-only
if (OpenSSL_FOUND)
    add_executable(test_sqlite_file_cipher
        test_main.cc
        test_sqlite_file_cipher.cc
    )
    target_include_directories(test_sqlite_file_cipher PRIVATE ${TEST_INCLUDES})
    target_compile_features(test_sqlite_file_cipher PRIVATE cxx_std_17)
    target_link_libraries(test_sqlite_file_cipher PRIVATE
        OpenSSL::SSL OpenSSL::Crypto
    )
    add_test(NAME sqlite_file_cipher COMMAND $<TARGET_FILE:test_sqlite_file_cipher>)
    set(_have_test_cipher TRUE)
else()
    message(STATUS "[tests] OpenSSL not found, skipping test_sqlite_file_cipher")
endif()

# ── 3) 需要 drogon + jwt-cpp 的测试 ─────────────────────────────────────────
# test_jwt 强依赖 drogon::app() 和 jwt-cpp，比较重；先留 stub，仅在
# RUOYI_BUILD_HEAVY_TESTS=ON 时启用
option(RUOYI_BUILD_HEAVY_TESTS "Build tests that depend on drogon/jwt-cpp" OFF)
if (RUOYI_BUILD_HEAVY_TESTS)
    find_package(Drogon QUIET)
    if (Drogon_FOUND)
        add_executable(test_jwt test_main.cc test_jwt.cc)
        target_include_directories(test_jwt PRIVATE ${TEST_INCLUDES})
        target_compile_features(test_jwt PRIVATE cxx_std_17)
        target_link_libraries(test_jwt PRIVATE Drogon::Drogon)
        add_test(NAME jwt COMMAND test_jwt)
    else()
        message(STATUS "[tests] Drogon not found, skipping test_jwt")
    endif()
endif()

# 汇总目标：cmake --build . --target ruoyi-tests
set(_test_targets test_metrics test_string_utils)
if (_have_test_security)
    list(APPEND _test_targets test_security)
endif()
if (_have_test_cipher)
    list(APPEND _test_targets test_sqlite_file_cipher)
endif()
add_custom_target(ruoyi-tests
    DEPENDS ${_test_targets}
    COMMENT "Build all ruoyi-cpp unit tests"
)
