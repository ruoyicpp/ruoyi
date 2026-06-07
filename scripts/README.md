# RuoYi-Cpp 辅助脚本集

本目录包含用于自动化搭建、配置或依赖管理支持的 PowerShell 脚本。

---

## 🛠️ SQLite3MC 源码下载脚本 (`download_sqlite3mc.ps1`)

由于 SQLite3MC (Multiple Ciphers) 模块的单源合并版本（amalgamation）体积较大（约 12MB），为了保证 Git 仓库的轻量和纯净，该源码默认**不包含在代码仓库中**。

`download_sqlite3mc.ps1` 脚本提供了一键式、自动化、带哈希验证的安全下载与部署功能。

### 📋 脚本工作流程

1. **环境检测**：检查目标存储路径 `src/third_party/sqlite3mc/` 是否已有对应文件。如果 `sqlite3mc_amalgamation.c` 和 `.h` 已经就绪，将自动跳过下载。
2. **下载压缩包**：从 GitHub Releases 官方存储库安全下载指定版本（默认 v2.1.0）的合并版压缩包（约 6MB）。
3. **SHA256 完整性校验**：自动比对下载压缩包的 SHA256 哈希值，确保文件未损坏或未被第三方篡改。
4. **解压部署**：自动解压并将 C++ 编译所需的 `sqlite3mc_amalgamation.c` 与 `sqlite3mc_amalgamation.h` 提取至 `src/third_party/sqlite3mc/` 目录中。
5. **清理临时文件**：删除下载的 zip 包及解压临时文件，保持目录整洁。

---

### 🚀 运行方法

在项目根目录下，使用 PowerShell 5.1 或更高版本运行脚本：

```powershell
.\scripts\download_sqlite3mc.ps1
```

*若出现权限受限错误，可临时放开执行策略：*
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\scripts\download_sqlite3mc.ps1
```

---

### 📦 下载后的目录结构

运行成功后，会在 `@/src/third_party/sqlite3mc/` 下生成编译环境：

```
src/third_party/sqlite3mc/
├── sqlite3.h                  # C++ shim 桥接头文件 (已有)
├── sqlite3mc_amalgamation.c   # 自动下载：SQLite3 核心与加密算法源码 (~12MB)
└── sqlite3mc_amalgamation.h   # 自动下载：SQLite3 核心头文件 (~650KB)
```

准备就绪后，直接在根目录重新执行 `cmake` 生成构建并编译，即可编译出完美支持 SQLite 页级强加密的 C++ 后端：

```bash
cmake -B build
cmake --build build --parallel
```
