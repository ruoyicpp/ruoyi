# RuoYi-Cpp 工具集

本目录包含项目辅助开发的运维与数据维护工具。目前主要提供了一个独立的 SQLite3 密文处理与校验工具。

---

## 🛠️ SQLite 密文处理工具 (`sqlite_cipher_tool`)

`sqlite_cipher_tool.cc` 是一个基于 C++ 编写的独立 CLI 命令行工具，主要配合 [SQLite3MC (SQLite3 Multiple Ciphers)](https://github.com/utelle/SQLite3MultipleCiphers) 加密库，对 `.db` 数据库文件进行**明文加密、密文解密、换密、完整性校验及环境自检**等维护操作。

### 📦 核心功能与命令格式

#### 1. 明文加密 (`encrypt`)
将一个普通的明文明文数据库，转换为加密库：
```bash
./sqlite_cipher_tool encrypt <src.db> <dst.db> <key>
```
* **`<src.db>`**: 源明文数据库路径。
* **`<dst.db>`**: 生成的目标加密数据库路径（该路径不能已存在，防止误覆盖）。
* **`<key>`**: 加密密钥（可为任意字符串，内部直接传递给 `sqlite3_key` 接口）。

#### 2. 密文解密 (`decrypt`)
将一个已加密的数据库，恢复解密为普通明文数据库：
```bash
./sqlite_cipher_tool decrypt <src.db> <key> <dst.db>
```
* **`<src.db>`**: 加密数据库路径。
* **`<key>`**: 正确的解密密钥。
* **`<dst.db>`**: 导出的目标明文数据库路径（不可已存在）。

#### 3. 密码修改/换密 (`rekey`)
直接对一个现有的加密数据库修改其密钥：
```bash
./sqlite_cipher_tool rekey <db> <old_key> <new_key>
```
* **`<db>`**: 待修改的数据库文件。
* **`<old_key>`**: 原密钥。
* **`<new_key>`**: 设定的新密钥。

#### 4. 完整性检查与验证 (`check`)
验证指定的密钥是否能正确解锁该数据库，并输出数据库内可读取的表总量：
```bash
./sqlite_cipher_tool check <db> [key]
```
* **`[key]`**: 密钥，如果是普通明文数据库，可以省略此参数。如果密钥错误，工具会直接报错。

#### 5. 链路与环境自检 (`selftest`)
运行集成在工具内部的 5 项完整性单元测试，用于诊断当前运行环境的 SQLite3MC 动态/静态库加密功能是否完全正常：
```bash
./sqlite_cipher_tool selftest
```
* **自检流程**：创建临时明文库 → 加密明文库 → 验证无 Key 访问被拒绝 → 验证错 Key 访问被拒绝 → 验证正确 Key 访问可以正常读取 3 行数据 → 清理临时文件并输出 `PASS`。

---

## 🔍 实现原理

本工具的设计极其注重**健壮性**与**兼容性**：
1. **避开 Backup API 兼容陷阱**：
   在 SQLite3MC 2.x 中，默认的底层 Cipher 模式通常与标准的 `sqlite3_backup` 备份接口存在格式不兼容。
2. **VACUUM INTO 跨 Codec 拷贝**：
   本工具在执行 `encrypt` 与 `decrypt` 时，使用 `VACUUM INTO '<path>'` SQL 语句让数据库引擎在已经解密/或保持明文的状态下直接安全地转储目标库。
3. **两步法处理**：
   * **加密**：先 `VACUUM INTO` 临时明文文件，接着打开临时明文文件调用 `sqlite3_rekey` 将其设为加密格式。
   * **解密**：使用正确的 Key 打开源加密数据库，利用 `VACUUM INTO` 导出时，临时文件自动继承当前的 Key 状态，最后再打开该文件调用 `sqlite3_rekey(db, "", 0)` 完全脱去加密外衣。

---

## 🔨 编译指南

在集成有 SQLite3MC 动态链接库或静态链接库的项目中，可以通过 C++ 编译器（如 GCC、Clang 或 MSVC）将其作为独立工具编译。

### CMake 配置参考
可在项目根 CMake 中添加此命令行目标：
```cmake
add_executable(sqlite_cipher_tool sqlite_cipher_tool.cc)
target_link_libraries(sqlite_cipher_tool PRIVATE 
    sqlite3mc   # 链接带 MultipleCiphers 支持的 SQLite3 库
)
```
