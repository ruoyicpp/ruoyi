# SQLite 透明加密

## 架构

```text
main.cc 启动流程
  -> SqliteCipher::loadConfig(root)      读配置
  -> SqliteCipher::deriveKey(cfg)        按 source 派生密钥
  -> DatabaseService::setCipherKey(key)  注入
  -> DatabaseService::connectSqlite()    打开并 sqlite3_key

底层：sqlite3mc amalgamation（MIT）编进 libsqlite3mc.a
      AES-256/ChaCha20 + PBKDF2-SHA512 + HMAC
```

## 关键文件

| 路径 | 作用 |
|---|---|
| `src/third_party/sqlite3mc/sqlite3mc_amalgamation.c/h` | SQLite3MC 单文件源码（14MB） |
| `src/third_party/sqlite3mc/sqlite3.h` | shim，让 `#include <sqlite3.h>` 命中 MC 版 |
| `src/common/SqliteCipher.h` | 密钥派生 + 应用的封装 |
| `src/services/DatabaseService.h` | `setCipherKey()` + `connectSqlite()` 加密分支 |
| `tools/sqlite_cipher_tool.cc` | 独立 CLI 工具（encrypt/decrypt/rekey/check/selftest） |

CMake 里由 `ENABLE_SQLITE_ENCRYPTION`（默认 `ON`）控制是否把 sqlite3mc 编进来；为 `ON` 时会定义 `HAVE_SQLCIPHER=1` 宏。

## 配置

`config.json` 里 `security.sqlite.encryption`：

```json
{
  "security": {
    "sqlite": {
      "encryption": {
        "enabled": false,
        "source": "hwid",
        "key": "",
        "env_var": "RUOYI_SQLITE_KEY",
        "vault_field": "sqlite_key",
        "vault_salt_field": "sqlite_salt",
        "pbkdf2_iter": 64000,
        "page_size": 4096,
        "cipher_kdf_iter": 256000,
        "allow_vault_fallback": true
      }
    }
  }
}
```

## 5 种密钥来源

| `source` | 密钥来自 | 适用场景 |
|---|---|---|
| `config` | 直接写 `key` 字段 | 临时测试 |
| `env` | 环境变量（默认 `RUOYI_SQLITE_KEY`） | 容器部署，避免写入磁盘 |
| `hwid` | 硬件指纹 -> PBKDF2-SHA256 | 单机商用推荐 |
| `vault` | 读环境变量 `RUOYI_VAULT_<FIELD>`（过渡实现） | Vault 托管 |
| `hwid+vault` | HWID 作 password + Vault 盐作 salt | 最高强度，有 Vault |

## `hwid` 原理

```text
key_hex = PBKDF2-HMAC-SHA256(
  password = HardwareFingerprint::compute(),
  salt     = "ruoyi-sqlite-v1",
  iter     = 64000
)
```

绑定机器：换主板 / 网卡 / 系统盘后密钥可能变化，库打不开，需提前解密或手动迁移。

## `hwid+vault` 原理

```text
salt = getenv("RUOYI_VAULT_SQLITE_SALT")
key_hex = PBKDF2-HMAC-SHA256(
  password = HardwareFingerprint::compute(),
  salt     = salt,
  iter     = 64000
)
```

双因子要求同时满足：

- 机器硬件不变
- Vault 盐仍可获取

## `allow_vault_fallback`

当 `source = hwid+vault` 但环境变量 `RUOYI_VAULT_*` 没设好时：

- `true`：自动降级为 `hwid` 单因子，日志 `WARN`，服务继续启动
- `false`：严格模式，密钥派生失败，主程序退出

## 典型部署流程

### 从零开始，直接用加密库

```powershell
notepad config.json
.\ruoyi-cpp.exe
```

### 现有明文 DB 迁移到加密 DB

```powershell
.\sqlite_cipher_tool.exe encrypt ruoyi-cpp.db ruoyi-cpp.enc.db "你的密钥"
Remove-Item ruoyi-cpp.db
Rename-Item ruoyi-cpp.enc.db ruoyi-cpp.db
.\ruoyi-cpp.exe
```

### 更换密钥

```powershell
.\sqlite_cipher_tool.exe rekey ruoyi-cpp.db "旧密码" "新密码"
```

## 验证

```powershell
.\sqlite_cipher_tool.exe selftest
```

预期结果：

```text
[selftest PASS] SQLite3MC 加密栈工作正常
```

## 恢复来源

本文件根据 `baae2931-1dd8-49da-860c-b282bb274c56.pb` 中 2026-04-17 16:14 前后的可见轨迹内容恢复。原轨迹显示最终 5 项 selftest 通过，全工程构建无错。
