# RuoYi-Cpp 单元测试指南

本项目使用 [doctest](https://github.com/doctest/doctest)（一个极其轻量、单头文件、极高性能的 C++ 测试框架）进行单元测试。

---

## 📦 目录结构

```
tests/
├── CMakeLists.txt              # 测试子目录构建脚本
├── doctest.h                   # doctest 单头文件测试框架
├── test_main.cc                # 测试入口文件（定义 DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN）
├── test_metrics.cc             # 性能监控、QPS 计数器测试
├── test_string_utils.cc        # 字符串工具类、格式化、裁剪函数测试
├── test_http_status.cc         # HTTP 状态码映射与轻量级快照测试
├── test_security.cc            # 密码学工具类、PBKDF2 算法与密钥生成测试
├── test_sqlite_file_cipher.cc  # SQLite 两层加密方案（RYENC1 与 sqlite3mc 接口）测试
├── test_jwt.cc                 # JWT 签名、校验及 Token 过期测试（重度依赖）
├── test_token_cache.cc         # Token 黑名单与 Redis 缓存联动测试（重度依赖）
└── test_rate_limiter.cc        # 漏桶、令牌桶限流算法测试（重度依赖）
```

---

## 🛠️ 设计原则

为了提升编译速度并降低 CI/CD 容器构建复杂度，测试用例分为三类：

1. **Self-Contained（无外部依赖测试）**
   * **示例**：`test_metrics`、`test_string_utils`、`test_http_status`。
   * **特点**：不需要连接数据库、网络或 Drogon 运行时，编译速度极快（秒级）。
2. **Moderate Dependency（轻/中度依赖测试）**
   * **示例**：`test_security`、`test_sqlite_file_cipher`。
   * **特点**：需要 OpenSSL，通过 CMake 的 `find_package(OpenSSL)` 自动探测，满足条件时自动启用编译。
3. **Heavy Dependency（重度依赖测试）**
   * **示例**：`test_jwt`、`test_token_cache`、`test_rate_limiter`。
   * **特点**：强依赖 `Drogon` 事件循环与 `jwt-cpp` 等。默认关闭，需通过 `-DRUOYI_BUILD_HEAVY_TESTS=ON` 选项手动激活。

---

## 🔨 编译与运行测试

单元测试集成在主 CMake 构建树中。

### 1. 配置并启用测试
在主项目编译时，添加 `-DRUOYI_BUILD_TESTS=ON` 开启测试：

```bash
# 生成 CMake 构建目录并启用测试
cmake -B build -DRUOYI_BUILD_TESTS=ON

# 如果需要运行重度依赖的 JWT/限流等测试，增加参数：
cmake -B build -DRUOYI_BUILD_TESTS=ON -DRUOYI_BUILD_HEAVY_TESTS=ON
```

### 2. 编译测试目标
使用 CMake 统一编译汇总目标 `ruoyi-tests`：

```bash
cmake --build build --target ruoyi-tests --parallel
```

### 3. 执行测试
你可以使用 CMake 的 `ctest` 工具，也可以直接运行编译出的二进制文件：

#### 方法 A：使用 ctest（推荐，可一键运行所有单测）
```bash
ctest --test-dir build --output-on-failure
```

#### 方法 B：直接运行特定测试程序
编译产物会生成在 `build/tests/` 目录下（如 `test_string_utils.exe`），可以直接执行并查看详细断言：
```bash
./build/tests/test_string_utils
```

---

## 📝 编写新的测试用例

要在项目中增加新的测试，只需两个步骤：

1. 在 `tests/` 目录下新建测试文件（例如 `test_new_feature.cc`），引用 `doctest.h` 并编写测试套件：
   ```cpp
   #include "doctest.h"
   #include "common/StringUtils.h" // 引用你要测试的头文件

   TEST_SUITE("NewFeature") {
       TEST_CASE("Basic Test") {
           CHECK(1 == 1);
       }
   }
   ```
2. 在 `tests/CMakeLists.txt` 中配置可执行目标：
   ```cmake
   add_executable(test_new_feature test_main.cc test_new_feature.cc)
   target_include_directories(test_new_feature PRIVATE ${TEST_INCLUDES})
   target_compile_features(test_new_feature PRIVATE cxx_std_17)
   add_test(NAME new_feature COMMAND $<TARGET_FILE:test_new_feature>)
   list(APPEND _test_targets test_new_feature) # 加入汇总目标
   ```
