#pragma once
// ============================================================================
// SQLite 文件级加密（移植自 wepay-cpp/PayDb，零外部依赖，仅用 OpenSSL）
//
// 适用场景：未编译 sqlite3mc 时的回退方案
//
// 设计：
//   - 数据库文件以加密形态存于磁盘：ruoyi.db.enc
//     格式: [salt(16)] [iv(16)] [AES-256-CBC 密文...]
//   - 启动时：若 .enc 存在 → 用 PBKDF2-SHA256(10000 轮) 派生密钥 → 解密到明文 .db
//     SQLite 用 sqlite3_open 正常打开
//   - 关闭/flush 时：WAL checkpoint → 整文件加密 → 原子替换 .enc → 删除明文 .db
//
// 注意事项：
//   - 运行时磁盘上存在临时明文 .db 文件（性能优先；如要全程加密用 sqlite3mc）
//   - 大数据库启动/关闭耗时与文件大小线性相关
//   - 与 SqliteCipher::deriveKey() 配合使用：上层用 5 种 source 之一派生 passphrase
// ============================================================================
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace SqliteFileCipher {

inline constexpr int PBKDF2_ITERATIONS = 10000;
inline constexpr int AES_KEY_LEN       = 32;   // AES-256
inline constexpr int AES_IV_LEN        = 16;
inline constexpr int SALT_LEN          = 16;

// 加密：plainPath → encPath ([salt(16)] [iv(16)] [ciphertext...])
inline bool encryptFile(const std::string& plainPath,
                        const std::string& encPath,
                        const std::string& passphrase) {
    // 读明文
    std::ifstream fin(plainPath, std::ios::binary);
    if (!fin) return false;
    std::vector<unsigned char> plain(
        (std::istreambuf_iterator<char>(fin)),
        std::istreambuf_iterator<char>());
    fin.close();

    // 随机 salt + iv
    unsigned char salt[SALT_LEN], iv[AES_IV_LEN];
    if (RAND_bytes(salt, SALT_LEN) != 1) return false;
    if (RAND_bytes(iv,   AES_IV_LEN) != 1) return false;

    // PBKDF2 派生密钥
    unsigned char key[AES_KEY_LEN];
    if (PKCS5_PBKDF2_HMAC(passphrase.c_str(), (int)passphrase.size(),
                          salt, SALT_LEN, PBKDF2_ITERATIONS,
                          EVP_sha256(), AES_KEY_LEN, key) != 1)
        return false;

    // AES-256-CBC 加密
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { OPENSSL_cleanse(key, AES_KEY_LEN); return false; }
    std::vector<unsigned char> cipher(plain.size() + AES_IV_LEN + 32);
    int outLen = 0, finalLen = 0;
    bool ok = false;
    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) break;
        if (EVP_EncryptUpdate(ctx, cipher.data(), &outLen,
                              plain.data(), (int)plain.size()) != 1) break;
        if (EVP_EncryptFinal_ex(ctx, cipher.data() + outLen, &finalLen) != 1) break;
        ok = true;
    } while (false);
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(key, AES_KEY_LEN);
    if (!ok) return false;

    const int total = outLen + finalLen;

    // 写临时文件 + 原子替换
    const std::string tmp = encPath + ".tmp";
    std::ofstream fout(tmp, std::ios::binary | std::ios::trunc);
    if (!fout) return false;
    fout.write((const char*)salt,         SALT_LEN);
    fout.write((const char*)iv,           AES_IV_LEN);
    fout.write((const char*)cipher.data(), total);
    fout.close();
    if (!fout) { std::filesystem::remove(tmp); return false; }

    std::error_code ec;
    std::filesystem::rename(tmp, encPath, ec);
    if (ec) { std::filesystem::remove(tmp); return false; }
    return true;
}

// 解密：encPath → plainPath
inline bool decryptFile(const std::string& encPath,
                        const std::string& plainPath,
                        const std::string& passphrase) {
    std::ifstream fin(encPath, std::ios::binary);
    if (!fin) return false;
    std::vector<unsigned char> data(
        (std::istreambuf_iterator<char>(fin)),
        std::istreambuf_iterator<char>());
    fin.close();

    if (data.size() < (size_t)(SALT_LEN + AES_IV_LEN + 1)) return false;

    const unsigned char* salt   = data.data();
    const unsigned char* iv     = data.data() + SALT_LEN;
    const unsigned char* cipher = data.data() + SALT_LEN + AES_IV_LEN;
    const int cipherLen = (int)(data.size() - SALT_LEN - AES_IV_LEN);

    unsigned char key[AES_KEY_LEN];
    if (PKCS5_PBKDF2_HMAC(passphrase.c_str(), (int)passphrase.size(),
                          salt, SALT_LEN, PBKDF2_ITERATIONS,
                          EVP_sha256(), AES_KEY_LEN, key) != 1)
        return false;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { OPENSSL_cleanse(key, AES_KEY_LEN); return false; }
    std::vector<unsigned char> plain(cipherLen + 32);
    int outLen = 0, finalLen = 0;
    bool ok = false;
    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) != 1) break;
        if (EVP_DecryptUpdate(ctx, plain.data(), &outLen, cipher, cipherLen) != 1) break;
        if (EVP_DecryptFinal_ex(ctx, plain.data() + outLen, &finalLen) != 1) break;
        ok = true;
    } while (false);
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(key, AES_KEY_LEN);
    if (!ok) return false;

    const int total = outLen + finalLen;
    std::ofstream fout(plainPath, std::ios::binary | std::ios::trunc);
    if (!fout) return false;
    fout.write((const char*)plain.data(), total);
    fout.close();
    return fout.good();
}

// 探测：encPath 是否为加密文件（魔法检测：>= 32 字节，且明显非 SQLite 标头）
inline bool isEncryptedFile(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin) return false;
    char hdr[16] = {0};
    fin.read(hdr, 16);
    // SQLite 文件前 16 字节是 "SQLite format 3\0"
    static const char kSqliteMagic[] = "SQLite format 3";
    if (fin.gcount() == 16 && std::memcmp(hdr, kSqliteMagic, 15) == 0)
        return false;  // 是明文 SQLite
    return true;       // 否则按加密文件处理
}

}  // namespace SqliteFileCipher
