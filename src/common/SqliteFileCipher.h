#pragma once
// ============================================================================
// SQLite 文件级加密 v2 —— RYENC1 自研封装格式
//
// 在 wepay-cpp PayDb 基础上升级：
//   * AES-256-CBC → AES-256-GCM      （现代认证加密）
//   * PBKDF2 10000 → 100000 迭代     （10× 防暴破）
//   * 新增 HMAC-SHA256 整体校验      （即使 GCM 标签被改也能识别篡改）
//   * 新增 RYENC1 魔法头 + 版本号    （未来算法升级平滑）
//   * 单密钥派生双子密钥             （AES key + HMAC key 隔离）
//
// 文件格式（小端字节序）:
//   +────────────────────────────────────────────────────────────────────────+
//   | offset | size | field        | desc                                     |
//   +────────────────────────────────────────────────────────────────────────+
//   |   0    |  8   | magic        | "RYENC1\0\0"                             |
//   |   8    |  4   | version      | uint32 = 1                               |
//   |  12    |  4   | kdf_iter     | uint32 = 100000                          |
//   |  16    | 16   | salt         | PBKDF2 salt (random)                     |
//   |  32    | 12   | nonce        | GCM IV (random, never reused)            |
//   |  44    |  N   | ciphertext   | AES-256-GCM(payload)                     |
//   | 44+N   | 16   | gcm_tag      | GCM authentication tag                   |
//   | 60+N   | 32   | hmac         | HMAC-SHA256(all preceding bytes)         |
//   +────────────────────────────────────────────────────────────────────────+
//
// 密钥派生：
//   master = PBKDF2-HMAC-SHA256(passphrase, salt, 100000, len=64)
//   aes_key  = master[ 0..32]   // AES-256
//   hmac_key = master[32..64]   // HMAC-SHA256
//
// 双重认证理由：
//   1) GCM tag 已经能识别 ciphertext + nonce + AAD 篡改
//   2) HMAC 额外覆盖 magic+version+kdf_iter+salt+nonce+ct+tag 整体，
//      防止有人构造一个看似合法的"较旧版本" downgrade 攻击
// ============================================================================
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <cstring>
#include <cstdint>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace SqliteFileCipher {

// ── 格式常量 ─────────────────────────────────────────────────────────────────
inline constexpr char     kMagic[8]     = {'R','Y','E','N','C','1','\0','\0'};
inline constexpr uint32_t kVersion      = 1;
inline constexpr uint32_t kKdfIter      = 100000;        // PBKDF2 迭代（建议 ≥ 10万）
inline constexpr int      kSaltLen      = 16;
inline constexpr int      kNonceLen     = 12;            // GCM 标准 nonce
inline constexpr int      kGcmTagLen    = 16;
inline constexpr int      kHmacLen      = 32;            // HMAC-SHA256
inline constexpr int      kAesKeyLen    = 32;            // AES-256
inline constexpr int      kHmacKeyLen   = 32;
inline constexpr int      kMasterKeyLen = kAesKeyLen + kHmacKeyLen;   // 64
inline constexpr int      kHeaderLen    = 8 + 4 + 4 + kSaltLen + kNonceLen;  // 44

// ── 工具函数：字节序无关写入 ───────────────────────────────────────────────
inline void writeU32LE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v       & 0xFF));
    out.push_back((uint8_t)((v >>  8) & 0xFF));
    out.push_back((uint8_t)((v >> 16) & 0xFF));
    out.push_back((uint8_t)((v >> 24) & 0xFF));
}
inline uint32_t readU32LE(const uint8_t* p) {
    return  (uint32_t)p[0]
          | ((uint32_t)p[1] << 8)
          | ((uint32_t)p[2] << 16)
          | ((uint32_t)p[3] << 24);
}

// ── 安全工具 ─────────────────────────────────────────────────────────────────
// 恒定时间比较，避免时序攻击泄露 HMAC
inline bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t n) {
    uint8_t diff = 0;
    for (size_t i = 0; i < n; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

// PBKDF2 派生 64 字节主密钥 → 拆成 (aes_key, hmac_key)
inline bool deriveKeys(const std::string& passphrase, const uint8_t* salt,
                       uint8_t aesKey[kAesKeyLen],
                       uint8_t hmacKey[kHmacKeyLen]) {
    uint8_t master[kMasterKeyLen];
    int rc = PKCS5_PBKDF2_HMAC(
        passphrase.c_str(), (int)passphrase.size(),
        salt, kSaltLen, (int)kKdfIter,
        EVP_sha256(), kMasterKeyLen, master);
    if (rc != 1) return false;
    std::memcpy(aesKey,  master,              kAesKeyLen);
    std::memcpy(hmacKey, master + kAesKeyLen, kHmacKeyLen);
    OPENSSL_cleanse(master, kMasterKeyLen);
    return true;
}

// ── 加密：plainPath → encPath（RYENC1 格式）────────────────────────────────
inline bool encryptFile(const std::string& plainPath,
                        const std::string& encPath,
                        const std::string& passphrase) {
    // 1) 读取明文
    std::ifstream fin(plainPath, std::ios::binary);
    if (!fin) return false;
    std::vector<uint8_t> plain(
        (std::istreambuf_iterator<char>(fin)),
        std::istreambuf_iterator<char>());
    fin.close();

    // 2) 生成 salt + nonce
    uint8_t salt[kSaltLen], nonce[kNonceLen];
    if (RAND_bytes(salt,  kSaltLen)  != 1) return false;
    if (RAND_bytes(nonce, kNonceLen) != 1) return false;

    // 3) PBKDF2 派生 AES key + HMAC key
    uint8_t aesKey[kAesKeyLen], hmacKey[kHmacKeyLen];
    if (!deriveKeys(passphrase, salt, aesKey, hmacKey)) return false;

    // 4) AES-256-GCM 加密
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        OPENSSL_cleanse(aesKey, kAesKeyLen);
        OPENSSL_cleanse(hmacKey, kHmacKeyLen);
        return false;
    }
    std::vector<uint8_t> cipher(plain.size() + 16);
    uint8_t tag[kGcmTagLen];
    int outLen = 0, finalLen = 0;
    bool ok = false;
    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr) != 1) break;
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, aesKey, nonce) != 1) break;
        if (EVP_EncryptUpdate(ctx, cipher.data(), &outLen,
                              plain.data(), (int)plain.size()) != 1) break;
        if (EVP_EncryptFinal_ex(ctx, cipher.data() + outLen, &finalLen) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kGcmTagLen, tag) != 1) break;
        ok = true;
    } while (false);
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(aesKey, kAesKeyLen);
    if (!ok) { OPENSSL_cleanse(hmacKey, kHmacKeyLen); return false; }
    cipher.resize(outLen + finalLen);

    // 5) 组装文件主体：header + ciphertext + tag
    std::vector<uint8_t> body;
    body.reserve(kHeaderLen + cipher.size() + kGcmTagLen);
    body.insert(body.end(), kMagic, kMagic + 8);
    writeU32LE(body, kVersion);
    writeU32LE(body, kKdfIter);
    body.insert(body.end(), salt, salt + kSaltLen);
    body.insert(body.end(), nonce, nonce + kNonceLen);
    body.insert(body.end(), cipher.begin(), cipher.end());
    body.insert(body.end(), tag, tag + kGcmTagLen);

    // 6) 计算 HMAC-SHA256(body) 追加到末尾
    uint8_t hmac[kHmacLen];
    unsigned int hmacOutLen = kHmacLen;
    HMAC(EVP_sha256(), hmacKey, kHmacKeyLen,
         body.data(), body.size(), hmac, &hmacOutLen);
    OPENSSL_cleanse(hmacKey, kHmacKeyLen);
    body.insert(body.end(), hmac, hmac + kHmacLen);

    // 7) 原子写入：先写 .tmp 再 rename
    const std::string tmp = encPath + ".tmp";
    std::ofstream fout(tmp, std::ios::binary | std::ios::trunc);
    if (!fout) return false;
    fout.write((const char*)body.data(), body.size());
    fout.close();
    if (!fout) { std::filesystem::remove(tmp); return false; }

    std::error_code ec;
    std::filesystem::rename(tmp, encPath, ec);
    if (ec) { std::filesystem::remove(tmp); return false; }
    return true;
}

// ── 解密：encPath → plainPath ──────────────────────────────────────────────
inline bool decryptFile(const std::string& encPath,
                        const std::string& plainPath,
                        const std::string& passphrase) {
    std::ifstream fin(encPath, std::ios::binary);
    if (!fin) return false;
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(fin)),
        std::istreambuf_iterator<char>());
    fin.close();

    // 1) 长度合法性检查
    const size_t kMinSize = kHeaderLen + kGcmTagLen + kHmacLen;  // 92
    if (data.size() < kMinSize) {
        std::cerr << "[FileCipher] 文件过短，非合法 RYENC1 格式" << std::endl;
        return false;
    }

    // 2) 验证 magic
    if (std::memcmp(data.data(), kMagic, 8) != 0) {
        std::cerr << "[FileCipher] 魔法头不匹配，非 RYENC1 格式" << std::endl;
        return false;
    }

    // 3) 解析头
    uint32_t ver  = readU32LE(data.data() + 8);
    uint32_t iter = readU32LE(data.data() + 12);
    if (ver != kVersion) {
        std::cerr << "[FileCipher] 版本不支持: " << ver << " (期望 " << kVersion << ")" << std::endl;
        return false;
    }
    const uint8_t* salt  = data.data() + 16;
    const uint8_t* nonce = data.data() + 32;

    // 4) 派生密钥（注意：用文件头中的 iter 而不是常量，以便未来兼容旧版）
    uint8_t aesKey[kAesKeyLen], hmacKey[kHmacKeyLen];
    {
        uint8_t master[kMasterKeyLen];
        if (PKCS5_PBKDF2_HMAC(passphrase.c_str(), (int)passphrase.size(),
                              salt, kSaltLen, (int)iter,
                              EVP_sha256(), kMasterKeyLen, master) != 1)
            return false;
        std::memcpy(aesKey,  master,              kAesKeyLen);
        std::memcpy(hmacKey, master + kAesKeyLen, kHmacKeyLen);
        OPENSSL_cleanse(master, kMasterKeyLen);
    }

    // 5) 首先验证 HMAC（恒定时间比较），防止 GCM 解密时序泄露
    const size_t bodyLen = data.size() - kHmacLen;
    uint8_t expectedHmac[kHmacLen];
    unsigned int hmacOutLen = kHmacLen;
    HMAC(EVP_sha256(), hmacKey, kHmacKeyLen,
         data.data(), bodyLen, expectedHmac, &hmacOutLen);
    OPENSSL_cleanse(hmacKey, kHmacKeyLen);
    if (!constantTimeEquals(expectedHmac, data.data() + bodyLen, kHmacLen)) {
        OPENSSL_cleanse(aesKey, kAesKeyLen);
        std::cerr << "[FileCipher] HMAC 校验失败：文件被篡改或密钥错误" << std::endl;
        return false;
    }

    // 6) 提取 ciphertext + GCM tag
    const size_t ctOffset = kHeaderLen;
    const size_t ctLen    = bodyLen - kHeaderLen - kGcmTagLen;
    const uint8_t* cipher = data.data() + ctOffset;
    const uint8_t* tag    = data.data() + ctOffset + ctLen;

    // 7) AES-256-GCM 解密 + 标签验证
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { OPENSSL_cleanse(aesKey, kAesKeyLen); return false; }
    std::vector<uint8_t> plain(ctLen + 16);
    int outLen = 0, finalLen = 0;
    bool ok = false;
    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr) != 1) break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, aesKey, nonce) != 1) break;
        if (EVP_DecryptUpdate(ctx, plain.data(), &outLen, cipher, (int)ctLen) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kGcmTagLen,
                                 const_cast<uint8_t*>(tag)) != 1) break;
        // GCM 在 Final 时验证 tag，若不匹配则返回 0
        if (EVP_DecryptFinal_ex(ctx, plain.data() + outLen, &finalLen) != 1) break;
        ok = true;
    } while (false);
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(aesKey, kAesKeyLen);
    if (!ok) {
        std::cerr << "[FileCipher] GCM 解密/标签验证失败" << std::endl;
        return false;
    }

    // 8) 写明文
    const int total = outLen + finalLen;
    std::ofstream fout(plainPath, std::ios::binary | std::ios::trunc);
    if (!fout) return false;
    fout.write((const char*)plain.data(), total);
    fout.close();
    return fout.good();
}

// ── 探测：encPath 是否为 RYENC1 加密文件 ───────────────────────────────────
inline bool isEncryptedFile(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin) return false;
    char hdr[8] = {0};
    fin.read(hdr, 8);
    return fin.gcount() == 8 && std::memcmp(hdr, kMagic, 8) == 0;
}

}  // namespace SqliteFileCipher
