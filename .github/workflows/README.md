# RuoYi-Cpp CI/CD 工作流 (`.github/workflows`)

本目录包含项目的持续集成与持续部署（CI/CD）自动化配置文件。目前主要通过 GitHub Actions 实现了跨平台测试、第三方源码校验和静态分析。

---

## 📦 目录结构

```
.github/workflows/
├── ci.yml              # 持续集成主配置文件（包含单元测试、编译与 Lint 静态分析）
└── README.md           # 本文档
```

---

## 🚀 CI 工作流详解 (`ci.yml`)

主工作流名为 **CI**，在每次向 `main` 或 `master` 分支提交代码（Push）、提交合并请求（Pull Request）以及手动触发（workflow_dispatch）时会自动运行。

整个工作流划分为 4 个主要作业（Jobs），采用并行与有条件执行策略：

### 1. 跨平台单元测试 (`unit-tests`)
* **运行环境**：矩阵（Matrix）化多系统运行 —— **Ubuntu**、**Windows**、**macOS**。
* **作业目的**：保障 C++ 核心工具模块的跨平台正确性。
* **执行步骤**：
  1. 拉取代码。
  2. 在 Windows 平台上，自动使用预装的 `vcpkg` 安装 OpenSSL 依赖，并将其追加至环境变量。
  3. 通过 CMake 在 Release 模式下配置项目中的 `tests/` 子项目。
  4. 并行编译 `test_metrics`、`test_string_utils` 及 `test_sqlite_file_cipher` 等单元测试目标。
  5. 调用 `ctest` 集中执行断言测试并输出不匹配故障（若发生）。

### 2. SQLite3MC 自动拉取与编译验证 (`sqlite3mc-fetch`)
* **运行环境**：**Ubuntu**。
* **触发条件**：仅在主分支发生 Push 或手动触发时运行。
* **作业目的**：验证第三方大体积源码自动化拉取链路的稳定性。
* **执行步骤**：
  1. 调用项目中的 PowerShell 脚本：`./scripts/download_sqlite3mc.ps1`（验证其在 Linux PowerShell 环境下的跨平台兼容性）。
  2. 验证下载完成的 `sqlite3mc_amalgamation` 文件的完整性（自动比对 SHA256 哈希值、验证文件大小是否处于 10MB-20MB 之间）。
  3. 执行独立静态编译测试：使用 GCC 编译 `sqlite3mc_amalgamation.c` 编译出目标文件 `.o`，确保最新合并包在新版本编译器下没有任何语法或类型兼容陷阱。

### 3. 主程序烟雾测试 (`build-smoke`)
* **运行环境**：**Ubuntu**。
* **作业目的**：对主程序的编译流程进行冒烟测试（目前处于桩阶段，可根据 CI 容器内部署 Drogon 与其他底层依赖库的策略进行扩展开发）。

### 4. 静态代码分析与质量规范 (`lint`)
* **运行环境**：**Ubuntu**。
* **核心属性**：`continue-on-error: true`（静态警告不阻塞代码的集成与合并）。
* **作业目的**：通过静态检测发现代码中的潜在 Readability、Performance 和 Bugprone 隐患。
* **执行步骤**：
  1. 在宿主机安装 `clang-tidy` 分析器。
  2. 对不依赖 Drogon 运行时的独立基础模块进行现代 C++ 规范分析（匹配 C++17 标准，启用包括 `readability-*`、`performance-*`、`bugprone-*` 在内的多项高质量检测规则）。

---

## 🛠️ 本地调试单测与 CI 运行

您可以在本地开发机中模拟 CI 的执行逻辑，手动运行以下命令执行单测：

```bash
# 进入测试配置并编译运行
cmake -S tests -B build-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --config Release --parallel

# 使用 ctest 过滤运行
ctest --test-dir build-tests --output-on-failure -C Release
```
