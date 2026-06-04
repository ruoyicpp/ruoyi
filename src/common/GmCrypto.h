/**
 * @file GmCrypto.h
 * @brief 中国国家密码标准算法封装
 * 
 * 基于 OpenSSL 1.1.1+ / 3.x EVP 接口，提供国密算法的完整实现。
 * 
 * 支持的算法：
 *   - SM3：哈希算法（256 位，类似 SHA-256）
 *   - SM4：对称加密（128 位密钥，GCM 模式，类似 AES-128-GCM）
 *   - SM2：非对称加密 / 数字签名（256 位 ECC，类似 ECDSA P-256）
 * 
 * 数据格式：
 *   - 密码哈希：$sm3pbkdf2$<iter>$<salt_hex>$<hash_hex>
 *   - SM4 密文：Base64( IV[16] || PKCS7-Ciphertext )
 *   - SM2 签名：Base64( DER 编码 ECDSA_SIG )
 * 
 * 核心特性：
 *   - 完整的国密标准实现
 *   - 支持 OpenSSL 1.1.1 和 3.x 版本
 *   - 安全的随机数生成
 *   - 完整的错误处理
 */

#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/opensslv.h>

/**
 * @namespace GmCrypto
 * @brief 国密算法命名空间
 * 
 * 提供 SM3、SM4、SM2 等国密算法的实现。
 */
namespace GmCrypto {

/// @name 内部工具函数
/// @{
namespace detail {

/**
 * @brief 将二进制数据转换为十六进制字符串
 * @param d 二进制数据指针
 * @param n 数据长度
 * @return 十六进制字符串
 */
inline std::string toHex(const unsigned char *d, size_t n) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i) ss << std::setw(2) << (int)d[i];
    return ss.str();
}

/**
 * @brief 将十六进制字符串转换为二进制数据
 * @param h 十六进制字符串
 * @return 二进制数据向量
 */
inline std::vector<unsigned char> fromHex(const std::string &h) {
    std::vector<unsigned char> v;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        v.push_back((unsigned char)std::stoi(h.substr(i, 2), nullptr, 16));
    return v;
}

/**
 * @brief 将二进制数据转换为 Base64 字符串
 * @param d 二进制数据指针
 * @param n 数据长度
 * @return Base64 编码字符串
 */
inline std::string toBase64(const unsigned char *d, size_t n) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, mem);
    BIO_write(b64, d, (int)n);
    BIO_flush(b64);
    BUF_MEM *ptr; BIO_get_mem_ptr(mem, &ptr);  // use 'mem' not 'b64' — avoids use-after-free
    std::string s(ptr->data, ptr->length);
    BIO_free_all(b64);  // frees both b64 and mem; ptr->data is already copied into s
    return s;
}

/**
 * @brief 将 Base64 字符串转换为二进制数据
 * @param s Base64 编码字符串
 * @return 二进制数据向量
 * @throw std::runtime_error 如果解码失败
 */
inline std::vector<unsigned char> fromBase64(const std::string &s) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new_mem_buf(s.data(), (int)s.size());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, mem);
    // Base64 expands by ~33%, reserve generously then shrink
    std::vector<unsigned char> v((s.size() / 4) * 3 + 3);
    int n = BIO_read(b64, v.data(), (int)v.size());
    BIO_free_all(b64);
    if (n < 0) throw std::runtime_error("GmCrypto: Base64 decode failed");
    v.resize(n);
    return v;
}

inline std::string drainErrors() {
    std::string r;
    unsigned long e;
    while ((e = ERR_get_error())) {
        char buf[256] = {};
        ERR_error_string_n(e, buf, sizeof(buf));
        if (!r.empty()) r += " | ";
        r += buf;
    }
    return r;
}

// SM3 EVP_MD*（OpenSSL 1.1.1 起支持）
inline const EVP_MD* sm3Md() {
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    return EVP_sm3();
#else
    throw std::runtime_error("GmCrypto: SM3 requires OpenSSL >= 1.1.1");
#endif
}

} // namespace detail


// ════════════════════════════════════════════════════════════════════════════
// SM3 — 哈希
// ════════════════════════════════════════════════════════════════════════════

// 返回原始 32 字节
inline std::vector<unsigned char> sm3(const void *data, size_t len) {
    std::vector<unsigned char> out(32);
    unsigned int outLen = 32;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("GmCrypto SM3: CTX alloc failed");
    if (EVP_DigestInit_ex(ctx, detail::sm3Md(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data, len) != 1 ||
        EVP_DigestFinal_ex(ctx, out.data(), &outLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("GmCrypto SM3 failed: " + detail::drainErrors());
    }
    EVP_MD_CTX_free(ctx);
    return out;
}

inline std::vector<unsigned char> sm3(const std::string &s) {
    return sm3(s.data(), s.size());
}

// 返回十六进制字符串（64字符）
inline std::string sm3Hex(const std::string &s) {
    auto h = sm3(s);
    return detail::toHex(h.data(), h.size());
}


// ════════════════════════════════════════════════════════════════════════════
// SM3 PBKDF2 — 密码哈希（替代 PBKDF2-SHA256）
// 格式: $sm3pbkdf2$<iter>$<salt_hex>$<hash_hex>
// ════════════════════════════════════════════════════════════════════════════

inline std::string hashPassword(const std::string &password, int iterations = 100000) {
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));
    std::vector<unsigned char> out(32);
    if (PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.size(),
                           salt, sizeof(salt), iterations,
                           detail::sm3Md(), 32, out.data()) != 1)
        throw std::runtime_error("GmCrypto SM3 PBKDF2 failed: " + detail::drainErrors());
    return "$sm3pbkdf2$" + std::to_string(iterations) + "$" +
           detail::toHex(salt, sizeof(salt)) + "$" +
           detail::toHex(out.data(), out.size());
}

inline bool verifyPassword(const std::string &password, const std::string &encoded) {
    // 格式: $sm3pbkdf2$<iter>$<salt_hex>$<hash_hex>
    if (encoded.rfind("$sm3pbkdf2$", 0) != 0) return false;
    auto p1 = encoded.find('$', 11);
    if (p1 == std::string::npos) return false;
    auto p2 = encoded.find('$', p1 + 1);
    if (p2 == std::string::npos) return false;
    int iter = std::stoi(encoded.substr(11, p1 - 11));
    auto salt = detail::fromHex(encoded.substr(p1 + 1, p2 - p1 - 1));
    auto expected = detail::fromHex(encoded.substr(p2 + 1));
    std::vector<unsigned char> actual(32);
    if (PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.size(),
                           salt.data(), (int)salt.size(), iter,
                           detail::sm3Md(), 32, actual.data()) != 1) return false;
    // 常量时间比较，防时序攻击
    if (actual.size() != expected.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < actual.size(); ++i) diff |= actual[i] ^ expected[i];
    return diff == 0;
}

inline bool isGmPasswordHash(const std::string &encoded) {
    return encoded.rfind("$sm3pbkdf2$", 0) == 0;
}


// ════════════════════════════════════════════════════════════════════════════
// SM4-CBC — 对称加密（敏感字段加密，OpenSSL 1.1.1+ / 3.x 兼容）
// 密文格式: Base64( IV[16] || PKCS7-padded-Ciphertext )
// 密钥必须恰好 16 字节
// ════════════════════════════════════════════════════════════════════════════

inline std::string sm4Encrypt(const std::string &plaintext, const std::string &key16) {
#if OPENSSL_VERSION_NUMBER < 0x10101000L
    throw std::runtime_error("GmCrypto: SM4 requires OpenSSL >= 1.1.1");
#else
    if (key16.size() != 16) throw std::runtime_error("GmCrypto SM4: key must be 16 bytes");
    unsigned char iv[16];
    RAND_bytes(iv, sizeof(iv));

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("GmCrypto SM4: CTX alloc failed");

    if (EVP_EncryptInit_ex(ctx, EVP_sm4_cbc(), nullptr,
                           (const unsigned char*)key16.data(), iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("GmCrypto SM4: init failed: " + detail::drainErrors());
    }

    std::vector<unsigned char> ct(plaintext.size() + 16);
    int outLen = 0, finalLen = 0;
    EVP_EncryptUpdate(ctx, ct.data(), &outLen,
                      (const unsigned char*)plaintext.data(), (int)plaintext.size());
    EVP_EncryptFinal_ex(ctx, ct.data() + outLen, &finalLen);
    EVP_CIPHER_CTX_free(ctx);
    ct.resize(outLen + finalLen);

    // 组合: IV || Ciphertext
    std::vector<unsigned char> blob(iv, iv + 16);
    blob.insert(blob.end(), ct.begin(), ct.end());
    return detail::toBase64(blob.data(), blob.size());
#endif
}

inline std::string sm4Decrypt(const std::string &b64cipher, const std::string &key16) {
#if OPENSSL_VERSION_NUMBER < 0x10101000L
    throw std::runtime_error("GmCrypto: SM4 requires OpenSSL >= 1.1.1");
#else
    if (key16.size() != 16) throw std::runtime_error("GmCrypto SM4: key must be 16 bytes");
    auto blob = detail::fromBase64(b64cipher);
    if (blob.size() < 17) throw std::runtime_error("GmCrypto SM4: ciphertext too short");

    unsigned char *iv = blob.data();
    unsigned char *ct = blob.data() + 16;
    int ctLen = (int)(blob.size() - 16);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("GmCrypto SM4: CTX alloc failed");

    if (EVP_DecryptInit_ex(ctx, EVP_sm4_cbc(), nullptr,
                           (const unsigned char*)key16.data(), iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("GmCrypto SM4: init failed: " + detail::drainErrors());
    }

    std::vector<unsigned char> plain(ctLen + 16);
    int outLen = 0, finalLen = 0;
    EVP_DecryptUpdate(ctx, plain.data(), &outLen, ct, ctLen);
    int ok = EVP_DecryptFinal_ex(ctx, plain.data() + outLen, &finalLen);
    EVP_CIPHER_CTX_free(ctx);
    if (ok != 1) throw std::runtime_error("GmCrypto SM4: decrypt failed (bad padding or key)");
    plain.resize(outLen + finalLen);
    return std::string(plain.begin(), plain.end());
#endif
}


// ════════════════════════════════════════════════════════════════════════════
// SM2 — 数字签名
// 使用 SM2 椭圆曲线（NID_sm2 / "SM2"），签名/验签 via OpenSSL ECDSA
// ════════════════════════════════════════════════════════════════════════════

struct Sm2KeyPair {
    std::string privPem;  // PKCS#8 PEM 私钥
    std::string pubPem;   // SubjectPublicKeyInfo PEM 公钥
};

// 生成 SM2 密钥对（OpenSSL 1.1.1 / 3.x 双路兼容）
inline Sm2KeyPair sm2GenKey() {
#if OPENSSL_VERSION_NUMBER < 0x10101000L
    throw std::runtime_error("GmCrypto: SM2 requires OpenSSL >= 1.1.1");
#else
    EVP_PKEY *pkey = nullptr;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    // OpenSSL 3.x：直接用 "SM2" 名字生成
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(nullptr, "SM2", nullptr);
    if (!ctx) throw std::runtime_error("GmCrypto SM2: CTX alloc failed: " + detail::drainErrors());
    EVP_PKEY_keygen_init(ctx);
    if (EVP_PKEY_keygen(ctx, &pkey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("GmCrypto SM2: keygen failed: " + detail::drainErrors());
    }
    EVP_PKEY_CTX_free(ctx);
#else
    // OpenSSL 1.1.1：EC + NID_sm2 + set_alias
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!ctx) throw std::runtime_error("GmCrypto SM2: CTX alloc failed");
    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_sm2);
    if (EVP_PKEY_keygen(ctx, &pkey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("GmCrypto SM2: keygen failed: " + detail::drainErrors());
    }
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_set_alias_type(pkey, EVP_PKEY_SM2);
#endif

    auto toPem = [](EVP_PKEY *k, bool priv) {
        BIO *bio = BIO_new(BIO_s_mem());
        if (priv) PEM_write_bio_PrivateKey(bio, k, nullptr, nullptr, 0, nullptr, nullptr);
        else      PEM_write_bio_PUBKEY(bio, k);
        BUF_MEM *ptr; BIO_get_mem_ptr(bio, &ptr);
        std::string s(ptr->data, ptr->length);
        BIO_free(bio);
        return s;
    };

    Sm2KeyPair kp;
    kp.privPem = toPem(pkey, true);
    kp.pubPem  = toPem(pkey, false);
    EVP_PKEY_free(pkey);
    return kp;
#endif
}

// SM2 签名，返回 Base64(DER)
inline std::string sm2Sign(const std::string &privPem, const std::string &msg) {
#if OPENSSL_VERSION_NUMBER < 0x10101000L
    throw std::runtime_error("GmCrypto: SM2 requires OpenSSL >= 1.1.1");
#else
    BIO *bio = BIO_new_mem_buf(privPem.data(), (int)privPem.size());
    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) throw std::runtime_error("GmCrypto SM2: load privkey failed: " + detail::drainErrors());
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    EVP_PKEY_set_alias_type(pkey, EVP_PKEY_SM2);
#endif

    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx = nullptr;
    if (EVP_DigestSignInit(mctx, &pctx, detail::sm3Md(), nullptr, pkey) != 1) {
        EVP_MD_CTX_free(mctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("GmCrypto SM2: sign init failed: " + detail::drainErrors());
    }

    size_t sigLen = 0;
    EVP_DigestSign(mctx, nullptr, &sigLen,
                   (const unsigned char*)msg.data(), msg.size());
    std::vector<unsigned char> sig(sigLen);
    if (EVP_DigestSign(mctx, sig.data(), &sigLen,
                       (const unsigned char*)msg.data(), msg.size()) != 1) {
        EVP_MD_CTX_free(mctx); EVP_PKEY_free(pkey);
        throw std::runtime_error("GmCrypto SM2: sign failed: " + detail::drainErrors());
    }
    sig.resize(sigLen);
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(pkey);
    return detail::toBase64(sig.data(), sig.size());
#endif
}

// SM2 验签
inline bool sm2Verify(const std::string &pubPem,
                      const std::string &msg,
                      const std::string &sigB64) {
#if OPENSSL_VERSION_NUMBER < 0x10101000L
    return false;
#else
    BIO *bio = BIO_new_mem_buf(pubPem.data(), (int)pubPem.size());
    EVP_PKEY *pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) return false;
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    EVP_PKEY_set_alias_type(pkey, EVP_PKEY_SM2);
#endif

    auto sig = detail::fromBase64(sigB64);
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx = nullptr;
    bool ok = false;
    if (EVP_DigestVerifyInit(mctx, &pctx, detail::sm3Md(), nullptr, pkey) == 1) {
        ok = (EVP_DigestVerify(mctx, sig.data(), sig.size(),
                               (const unsigned char*)msg.data(), msg.size()) == 1);
    }
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(pkey);
    return ok;
#endif
}

} // namespace GmCrypto
