#pragma once
// ============================================================================
// SQLite 加密支持（基于 sqlite3mc，MinGW 兼容）
//
// 设计：
//   - 编译期：CMake 打开 ENABLE_SQLITE_ENCRYPTION 会定义 HAVE_SQLCIPHER，
//             自编译 sqlite3mc 代替系统 sqlite3。两者 C API 兼容。
//   - 运行期：config.json 的 security.sqlite.encryption.enabled 控制是否启用。
//             未启用时，sqlite3mc 照常打开普通 SQLite 文件（零开销）。
//
// 密钥来源（可插拔）：
//   - "config"     从 config.json 直接读（最简单，不推荐生产）
//   - "env"        从环境变量读（优于 config，方便注入）
//   - "hwid"       硬件指纹派生（商业版，跟机器绑定）
//   - "vault"      从环境变量注入的 Vault 字段读取（商业版）
//   - "hwid+vault" 双因子派生（商业版推荐）
//
// 所有涉及 HWID/Vault 的代码用 __has_include 做弱依赖，开源版无这些模块
// 时会在运行时返回错误而不是编译失败。
// ============================================================================

#include <json/json.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/opensslv.h>
#include <sqlite3.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

// OpenSSL 3.x 静态链接需要显式加载 default provider 才能用 EVP_sha256() 等
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#  include <openssl/provider.h>
namespace SqliteCipher { namespace detail {
    inline void ensureOpenSslProviderLoaded() {
        static std::once_flag flag;
        std::call_once(flag, []() { OSSL_PROVIDER_load(nullptr, "default"); });
    }
}}
#else
namespace SqliteCipher { namespace detail {
    inline void ensureOpenSslProviderLoaded() {}
}}
#endif

// ── 弱引用：商业版才有 HardwareFingerprint / VaultManager ────────────────────
#if __has_include("HardwareFingerprint.h")
#  include "HardwareFingerprint.h"
#  define RUOYI_HAVE_HWID 1
#endif
#if __has_include("../services/VaultManager.h")
#  include "../services/VaultManager.h"
#  define RUOYI_HAVE_VAULT 1
#endif

namespace SqliteCipher {

struct KeyConfig {
    bool        enabled        = false;               // 是否启用加密
    std::string source         = "hwid";              // 密钥来源（默认 hwid，无外部依赖）
    std::string configKey;                            // source=config
    std::string envVar         = "RUOYI_SQLITE_KEY";  // source=env
    std::string vaultField     = "sqlite_key";        // source=vault/hwid+vault
    std::string vaultSaltKey   = "sqlite_salt";       // source=hwid+vault 的盐
    int         pbkdf2Iter     = 64000;               // 派生迭代次数
    int         cipherPageSize = 4096;                // sqlite3mc page size
    int         cipherKdfIter  = 256000;              // sqlite3mc 内部 KDF 迭代
    // Vault 不可用时的行为：
    //   true  → source=hwid+vault 无 Vault 时自动降级为 hwid 单因子（写 WARNING）
    //   false → 严格模式，缺 Vault 直接失败
    bool        allowVaultFallback = true;
};

// ── 从 config.json 解析 ───────────────────────────────────────────────────────
inline KeyConfig loadConfig(const Json::Value& root) {
    KeyConfig c;
    const auto& s = root["security"]["sqlite"]["encryption"];
    if (s.isNull()) return c;
    c.enabled        = s.get("enabled",            false             ).asBool();
    c.source         = s.get("source",             "hwid"            ).asString();
    c.configKey      = s.get("key",                ""                ).asString();
    c.envVar         = s.get("env_var",            "RUOYI_SQLITE_KEY").asString();
    c.vaultField     = s.get("vault_field",        "sqlite_key"      ).asString();
    c.vaultSaltKey   = s.get("vault_salt_field",   "sqlite_salt"     ).asString();
    c.pbkdf2Iter     = s.get("pbkdf2_iter",        64000             ).asInt();
    c.cipherPageSize = s.get("page_size",          4096              ).asInt();
    c.cipherKdfIter  = s.get("cipher_kdf_iter",    256000            ).asInt();
    c.allowVaultFallback = s.get("allow_vault_fallback", true        ).asBool();
    return c;
}

// ── PBKDF2-HMAC-SHA256 派生 32 字节密钥，返回 hex 字符串 ──────────────────────
inline std::string pbkdf2Hex(const std::string& password,
                              const std::string& salt,
                              int iter) {
    detail::ensureOpenSslProviderLoaded();
    unsigned char out[32];
    if (PKCS5_PBKDF2_HMAC(password.data(), (int)password.size(),
                           reinterpret_cast<const unsigned char*>(salt.data()),
                           (int)salt.size(),
                           iter, EVP_sha256(), sizeof(out), out) != 1) {
        throw std::runtime_error("PBKDF2 failed");
    }
    static const char* HEX = "0123456789abcdef";
    std::string r(sizeof(out) * 2, ' ');
    for (size_t i = 0; i < sizeof(out); ++i) {
        r[i * 2    ] = HEX[out[i] >> 4];
        r[i * 2 + 1] = HEX[out[i] & 0xF];
    }
    return r;
}

// ── 根据配置派生 raw key（返回给 sqlite3_key 用的字符串）────────────────────
// 失败时返回空字符串并向 err 写入原因
// 成功但有警告时，key 非空且 err 携带 "[WARN] ..." 前缀
inline std::string deriveKey(const KeyConfig& cfg, std::string* err = nullptr) {
    auto fail = [&](const std::string& m) -> std::string {
        if (err) *err = m;
        return {};
    };

    if (!cfg.enabled) return {};

    const std::string& src = cfg.source;

    // ── 1. config ─────────────────────────────────────────────────────────────
    if (src == "config") {
        if (cfg.configKey.empty())
            return fail("source=config 但 key 为空，请在 config.json 填写 "
                        "security.sqlite.encryption.key");
        return cfg.configKey;
    }

    // ── 2. env ────────────────────────────────────────────────────────────────
    if (src == "env") {
        const char* val = std::getenv(cfg.envVar.c_str());
        if (!val || !*val)
            return fail("source=env 但环境变量 " + cfg.envVar + " 未设置或为空");
        return std::string(val);
    }

    // ── 3. hwid ───────────────────────────────────────────────────────────────
    if (src == "hwid") {
#ifndef RUOYI_HAVE_HWID
        return fail("source=hwid 但项目未包含 HardwareFingerprint.h（仅商业版支持）");
#else
        std::string fp = HardwareFingerprint::compute();
        if (fp.empty())
            return fail("HardwareFingerprint::compute() 返回空，无法派生密钥");
        return pbkdf2Hex(fp, "ruoyi-sqlite-v1", cfg.pbkdf2Iter);
#endif
    }

    // ── 4. vault ──────────────────────────────────────────────────────────────
    // 过渡实现：通过环境变量注入（由启动脚本从 Vault CLI 注入）
    if (src == "vault") {
        std::string envName = "RUOYI_VAULT_" + cfg.vaultField;
        for (char& c : envName) c = (char)std::toupper((unsigned char)c);
        const char* val = std::getenv(envName.c_str());
        if (!val || !*val)
            return fail("source=vault 但环境变量 " + envName +
                        " 未设置（请由启动脚本从 Vault 注入）");
        return std::string(val);
    }

    // ── 5. hwid+vault ─────────────────────────────────────────────────────────
    if (src == "hwid+vault") {
#ifndef RUOYI_HAVE_HWID
        return fail("source=hwid+vault 但项目未包含 HardwareFingerprint.h（仅商业版支持）");
#else
        std::string fp = HardwareFingerprint::compute();
        if (fp.empty())
            return fail("HardwareFingerprint::compute() 返回空");

        std::string saltEnvName = "RUOYI_VAULT_" + cfg.vaultSaltKey;
        for (char& c : saltEnvName) c = (char)std::toupper((unsigned char)c);
        const char* saltVal = std::getenv(saltEnvName.c_str());

        if (!saltVal || !*saltVal) {
            if (!cfg.allowVaultFallback)
                return fail("source=hwid+vault 但 Vault 盐 " + saltEnvName +
                            " 未注入，且 allow_vault_fallback=false");
            // 降级为 hwid 单因子
            if (err) *err = "[WARN] Vault 盐未注入，已自动降级为 hwid 单因子派生";
            return pbkdf2Hex(fp, "ruoyi-sqlite-v1", cfg.pbkdf2Iter);
        }
        return pbkdf2Hex(fp, std::string(saltVal), cfg.pbkdf2Iter);
#endif
    }

    return fail("未知的 source 类型: " + src +
                "（支持: config / env / hwid / vault / hwid+vault）");
}

// 迁移功能由独立的 CLI 工具 sqlite_cipher_tool 提供（encrypt/decrypt/rekey），
// 主程序运行期不需要 inline 实现（见 tools/sqlite_cipher_tool.cc）

}  // namespace SqliteCipher
