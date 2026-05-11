# SQLite 加密

ruoyi-cpp 支持两层 SQLite 加密方案，按编译选项自动切换。两种方案通过同一个配置入口 (`sqlite.encrypt_key`) 启用，业务代码无感知。

---

## 双层架构总览

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              选择哪一层？                                      │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│   编译时 src/third_party/sqlite3mc/sqlite3mc_amalgamation.c 是否存在？          │
│                                                                                │
│   ├─ 存在 → HAVE_SQLCIPHER=1 → 【页级加密】(SQLite3MC)                          │
│   │                                                                            │
│   │      磁盘上每个数据页独立 AES-GCM 加密                                     │
│   │      运行时透明读写，零明文窗口                                            │
│   │      性能：大库表现好（按需解密单页）                                       │
│   │      文件：ruoyi.db 本身就是密文                                            │
│   │                                                                            │
│   └─ 不存在 → 【文件级加密】(RYENC1 / SqliteFileCipher)                         │
│                                                                                │
│          启动时一次性解密 ruoyi.db.enc → ruoyi.db                              │
│          关闭时一次性加密 ruoyi.db → ruoyi.db.enc 并删明文                     │
│          算法：AES-256-GCM + HMAC-SHA256 + RYENC1 自研封装                     │
│          文件：磁盘只有 ruoyi.db.enc（关闭后）                                  │
│                                                                                │
└──────────────────────────────────────────────────────────────────────────────┘
```

| 维度 | 页级（sqlite3mc）| 文件级（RYENC1）|
|------|-----------------|-----------------|
| **依赖** | sqlite3mc amalgamation (12 MB 源码) | 仅 OpenSSL（项目已链接）|
| **运行时磁盘** | 始终密文 | 临时明文 `ruoyi.db` 存在 |
| **加密粒度** | 每页（默认 4 KB）| 整文件 |
| **启动开销** | 0（按需）| 解密耗时 ∝ 文件大小 |
| **关闭开销** | 0 | 加密整文件回写 |
| **WAL 兼容** | ✅ | ✅（关闭前 checkpoint）|
| **大库性能** | 优秀 | 大库启停慢 |
| **算法可升级** | sqlite3mc 全部 cipher | RYENC1 头有 version 字段 |
| **CI/容器开销** | 需多下 12 MB | 无 |

---

## 启用方式

### 1. 极简配置（任一方案都用）

`config.json` 顶级加：

```jsonc
{
  "sqlite": {
    "path": "ruoyi.db",
    "encrypt_key": "Your#Strong@Pass2026"
  }
}
```

`encrypt_key` 非空 → 自动启用加密。**走哪条路径由编译选项决定**：

- 若 `cmake configure` 时检测到 sqlite3mc 源码 → 走页级
- 否则走文件级

### 2. 高级配置（5 种密钥来源，可派生）

如果你不想把明文密钥写进 `config.json`，用 `security.sqlite.encryption.*`：

```jsonc
"security": {
  "sqlite": {
    "encryption": {
      "enabled": true,
      "source": "hwid",                      // hwid / env / vault / hwid+vault / config
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
```

密钥来源说明：

| source | 含义 | 适用 |
|--------|------|------|
| `config` | 直接读 `security.sqlite.encryption.key` | 测试环境 |
| `env` | 从环境变量 `RUOYI_SQLITE_KEY` 读 | CI / 容器 |
| `hwid` | 硬件指纹（CPU + 主板）派生 | 单机绑机 |
| `vault` | HashiCorp Vault | 集中密钥管理 |
| `hwid+vault` | 硬件指纹 ⊕ Vault salt | 最强（换机+泄漏都失败）|

**`security.sqlite.encryption.enabled = true` 时优先级高于顶级 `sqlite.encrypt_key`。**

---

## 编译选项

### 启用页级加密（推荐生产）

1. 下载 sqlite3mc amalgamation（一次性，~6 MB 压缩包，12 MB 解压）：

   ```powershell
   # 项目根目录
   .\scripts\download_sqlite3mc.ps1
   ```

   该脚本：
   - 从 GitHub Releases 下载官方 `sqlite3mc-2.1.0-sqlite-3.49.1-amalgamation.zip`
   - SHA256 校验
   - 解压到 `src/third_party/sqlite3mc/`

2. 重新 cmake configure + build：

   ```powershell
   cmake -B build-nginx -G Ninja
   cmake --build build-nginx --parallel
   ```

   日志会显示 `-- SQLite3MC:     BUILD FROM SOURCE -> ...`，并自动编出 `sqlite_cipher_tool.exe`。

### 仅启用文件级加密（开发 / 轻量场景）

不下载 amalgamation 即可。CMake configure 会回退提示：

```
sqlite3mc amalgamation not found at ..., falling back to system sqlite3
```

此时编译产物的加密走 RYENC1 文件级。

---

## RYENC1 文件格式

文件级加密产物 `ruoyi.db.enc` 采用自研封装：

```
+──────────────────────────────────────────────────────────────────────+
| offset | size | field        | desc                                  |
+──────────────────────────────────────────────────────────────────────+
|   0    |  8   | magic        | "RYENC1\0\0"                          |
|   8    |  4   | version      | uint32 LE = 1                         |
|  12    |  4   | kdf_iter     | uint32 LE = 100000                    |
|  16    | 16   | salt         | PBKDF2 salt (random per encrypt)      |
|  32    | 12   | nonce        | GCM IV (random per encrypt, never reused) |
|  44    |  N   | ciphertext   | AES-256-GCM(plaintext)                |
| 44+N   | 16   | gcm_tag      | GCM authentication tag                |
| 60+N   | 32   | hmac         | HMAC-SHA256 of all preceding bytes    |
+──────────────────────────────────────────────────────────────────────+
```

密钥派生：

```
master   = PBKDF2-HMAC-SHA256(passphrase, salt, 100000, len=64)
aes_key  = master[0..32]   // AES-256-GCM
hmac_key = master[32..64]  // HMAC-SHA256
```

双重认证理由：

1. **GCM tag**：检测密文 / nonce / AAD 任何 bit 翻转
2. **HMAC-SHA256**：额外覆盖 `magic + version + kdf_iter + salt + nonce + ct + tag`
   - 防止伪造较旧 version 头进行 downgrade 攻击
   - 防止伪造 `kdf_iter=1` 进行暴破降难度攻击

---

## sqlite_cipher_tool CLI

仅当启用页级加密（HAVE_SQLCIPHER=1）时编译。位于 `build-nginx/sqlite_cipher_tool.exe`。

### 子命令

| 命令 | 用途 | 示例 |
|------|------|------|
| `encrypt <src> <dst> <key>` | 明文 → 加密副本 | `tool encrypt plain.db enc.db "mypass"` |
| `decrypt <src> <key> <dst>` | 加密 → 明文副本 | `tool decrypt enc.db "mypass" plain.db` |
| `rekey <db> <old> <new>` | 原地换密 | `tool rekey enc.db "old" "new"` |
| `check <db> [key]` | 验证可读 | `tool check enc.db "mypass"` |
| `selftest` | 5 项自检 | `tool selftest` |

### 实现注意

- 内部用 `VACUUM INTO` + `sqlite3_rekey` 两段式，绕开 sqlite3mc 默认 cipher 与 `sqlite3_backup_init` 的兼容性问题
- `decrypt` 路径必须先用 `VACUUM INTO` 拷贝，**然后再调 `sqlite3_rekey("", 0)`** 移除加密 — 因为 `VACUUM INTO` 在 sqlite3mc 上会让目标继承源 codec

---

## 迁移路径

### 明文 ruoyi.db → 加密

**页级（推荐）：**

```powershell
# 1. 备份
copy ruoyi.db ruoyi.db.bak

# 2. 加密
.\sqlite_cipher_tool.exe encrypt ruoyi.db ruoyi-encrypted.db "YourKey"

# 3. 替换
del ruoyi.db
ren ruoyi-encrypted.db ruoyi.db

# 4. 改 config.json 加 encrypt_key 字段，重启
```

**文件级：**

```powershell
# 1. 启动一次带 encrypt_key 的服务
# 服务会读现有明文 ruoyi.db，关闭时自动产生 ruoyi.db.enc，删除明文 .db
```

### 文件级 ↔ 页级互转

**文件级 → 页级（升级到 sqlite3mc）：**

```powershell
# 1. 用旧 exe（无 HAVE_SQLCIPHER）启动并关闭，确保 ruoyi.db.enc 是 RYENC1 格式
# 2. 启动旧 exe 让它自动解密为明文 ruoyi.db
# 3. 关闭后用 cipher tool 转页级
.\sqlite_cipher_tool.exe encrypt ruoyi.db ruoyi-paged.db "YourKey"
del ruoyi.db ruoyi.db.enc
ren ruoyi-paged.db ruoyi.db
# 4. 换用带 HAVE_SQLCIPHER 编译的新 exe 启动
```

**页级 → 文件级（降级）：**

```powershell
# 1. 用带 HAVE_SQLCIPHER 的 exe 启动 + 关闭
# 2. 用 cipher tool 解密
.\sqlite_cipher_tool.exe decrypt ruoyi.db "YourKey" ruoyi-plain.db
del ruoyi.db
ren ruoyi-plain.db ruoyi.db
# 3. 换用不带 HAVE_SQLCIPHER 的 exe 启动，关闭时自动 RYENC1 加密
```

---

## 运维 FAQ

### Q1：为什么错误密钥时 `/actuator/health` 仍返回 `UP`？

如果同时配置了 PostgreSQL，PG 是主库、SQLite 是双写/fallback。SQLite 密钥错误 → SQLite 打不开但**不致命**，PG 仍工作 → health UP。

要严格检查 SQLite 加密：查启动日志 `[DB] SQLite 密钥验证失败` 或检查 `/actuator/db` 返回的 `backend` 字段。

### Q2：能否同时存在 `ruoyi.db` 和 `ruoyi.db.enc`？

**页级模式（HAVE_SQLCIPHER）**：不会产生 `.enc`，磁盘上只有加密的 `ruoyi.db`。

**文件级模式**：理论上只在运行期间存在明文 `ruoyi.db`，关闭后只剩 `.enc`。若进程异常崩溃（kill -9 / 断电），可能两个都存在 — 此时 `.enc` 是过期版本，需要重新启动让服务用 `.db` 跑 + 关闭重新加密。

### Q3：忘记密钥怎么办？

**不可恢复**。设计上没有 master key / 后门。这是加密的正确行为。

建议：
- 把密钥存到 Vault（`source: vault`）
- 或绑定硬件指纹（`source: hwid`），换机时自动失效（数据保护）

### Q4：可以热切换密钥吗？

需要停机。流程：

```powershell
# 1. 停服务
# 2. 用 cipher tool rekey
.\sqlite_cipher_tool.exe rekey ruoyi.db "oldkey" "newkey"
# 3. 更新 config.json 的 encrypt_key
# 4. 启动
```

### Q5：性能影响？

页级加密的典型开销：
- 单页 SELECT：+15% ~ +30% 延迟（AES-NI 加速时）
- 全表扫描：+20% ~ +40%
- 写操作：+10% ~ +20%

文件级加密无运行时开销，只有：
- 启动时整文件解密（10 MB 库约 50 ms）
- 关闭时整文件加密（同上）

### Q6：备份策略？

**页级**：直接拷贝加密的 `ruoyi.db` 即可，加密随行。还原也是拷贝即可。

**文件级**：拷贝 `ruoyi.db.enc`（关闭状态）。如果服务运行中要在线备份，用：

```powershell
# 在服务运行时调用 VACUUM INTO（明文）
.\sqlite_cipher_tool.exe encrypt ruoyi.db ruoyi-backup-$(Get-Date -f yyyyMMdd).db "YourKey"
```

---

## 安全注意事项

1. **不要把 `encrypt_key` 写进 git 仓库**。生产环境用 `source: env` 或 `source: vault`。
2. **运行时明文磁盘窗口**（文件级模式）：使用页级加密可彻底消除。
3. **WAL / SHM 文件**：服务关闭时自动删除。但**进程异常退出时可能残留**，含未刷盘的明文数据。重启时会自动清理。
4. **`config.json` 权限**：建议 `chmod 600`，防止本机其他账号读密钥。
5. **密钥长度**：≥ 16 字符，混合大小写 + 数字 + 符号。PBKDF2 100000 轮 + 32 字节随机 salt 提供足够保护。
6. **`sqlite_cipher_tool.exe` 不要部署到公网可达机器**。仅在受信任的运维终端使用。

---

## 测试覆盖

`tests/test_sqlite_file_cipher.cc` (10 cases / 57 assertions):

- ✅ encrypt → decrypt 字节完美往返
- ✅ RYENC1 魔法头识别
- ✅ `isEncryptedFile` 区分明文 vs 密文
- ✅ 错误密钥拒绝
- ✅ HMAC 检测密文位翻转
- ✅ HMAC 检测 header `kdf_iter` 篡改（防 downgrade）
- ✅ 随机 salt/nonce 让密文每次不同
- ✅ 256 KB 大文件往返
- ✅ 空文件边界
- ✅ 截断文件优雅失败

`sqlite_cipher_tool selftest` (5 cases):

- ✅ 明文库建立
- ✅ encrypt 产生加密文件
- ✅ 无 key 打开被拒
- ✅ 错 key 打开被拒
- ✅ 正确 key 读到原始 3 行数据

---

## 参考资料

- [SQLite3 Multiple Ciphers 官方文档](https://utelle.github.io/SQLite3MultipleCiphers/docs/)
- [GitHub Releases v2.1.0](https://github.com/utelle/SQLite3MultipleCiphers/releases/tag/v2.1.0)
- [NIST SP 800-132 — PBKDF2 推荐迭代数](https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-132.pdf)
- [RFC 5116 — AEAD (GCM 标准)](https://datatracker.ietf.org/doc/html/rfc5116)
