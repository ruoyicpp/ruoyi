// BCrypt 实现：使用 libbcrypt (header-only 版本)
// vcpkg install bcrypt  ->  提供 bcrypt/BCrypt.hpp
// 如果 vcpkg 不可用，使用下面的纯 C++ 实现（基于 OpenSSL SHA256 + 盐）
#include "SecurityUtils.h"

#ifdef HAVE_BCRYPT_HPP
#  include <bcrypt/BCrypt.hpp>

std::string SecurityUtils::encryptPassword(const std::string &password) {
    return BCrypt::generateHash(password, 10);
}

bool SecurityUtils::matchesPassword(const std::string &raw, const std::string &encoded) {
    return BCrypt::validatePassword(raw, encoded);
}

#else
// Fallback: 使用 OpenSSL PBKDF2-SHA256 模拟（兼容 MinGW，无需额外库）
// 格式: $pbkdf2$<salt_hex>$<hash_hex>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <mutex>

// OpenSSL 3.x 需要显式加载 default provider，否则 EVP_sha256() / PBKDF2 不可用
// （静态链接时尤甚——auto-init 在某些场景下不会触发）
//
// 重要：此函数必须在 main() 最早期、单线程下调用一次，
// 不能等到 drogon worker 线程触发 lazy 加载——OpenSSL 3.x 在多线程下
// 加载 provider 会与 system OpenSSL DLL（libpq.dll 间接拖入）的全局状态冲突，
// 导致 RtlReAllocateHeap 堆损坏。
#include <openssl/err.h>
#include <iostream>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#  include <openssl/provider.h>

// 暴露给 main.cc 调用的接口
extern "C" void ruoyi_init_openssl_provider() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        OSSL_PROVIDER *prov = OSSL_PROVIDER_load(nullptr, "default");
        if (!prov) {
            unsigned long e = ERR_get_error();
            char buf[256] = {0};
            ERR_error_string_n(e, buf, sizeof(buf));
            std::cerr << "[SecurityUtils] OSSL_PROVIDER_load(default) FAILED err="
                      << e << " (" << buf << ")" << std::endl;
        } else {
            std::cerr << "[SecurityUtils] OpenSSL default provider loaded "
                         "(version=" << OPENSSL_VERSION_NUMBER << ")" << std::endl;
        }
    });
}

static void ensureOpenSslProviderLoaded() {
    ruoyi_init_openssl_provider();
}

// 取出当前 OpenSSL 错误队列里所有错误，拼成字符串
static std::string drainOpenSslErrors() {
    std::string s;
    unsigned long e;
    while ((e = ERR_get_error()) != 0) {
        char buf[256] = {0};
        ERR_error_string_n(e, buf, sizeof(buf));
        if (!s.empty()) s += " | ";
        s += buf;
    }
    return s;
}
#else
extern "C" void ruoyi_init_openssl_provider() {}  // OpenSSL 1.x 自动初始化，no-op
static inline void ensureOpenSslProviderLoaded() {}
static inline std::string drainOpenSslErrors() { return {}; }
#endif

static std::string toHex(const unsigned char *data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    return oss.str();
}

static std::vector<unsigned char> fromHex(const std::string &hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned char b = (unsigned char)std::stoi(hex.substr(i, 2), nullptr, 16);
        bytes.push_back(b);
    }
    return bytes;
}

static std::vector<unsigned char> pbkdf2(const std::string &password,
                                          const unsigned char *salt, size_t saltLen,
                                          int iterations = 100000, size_t keyLen = 32) {
    ensureOpenSslProviderLoaded();
    std::vector<unsigned char> out(keyLen);
    if (PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.size(),
                           salt, (int)saltLen,
                           iterations, EVP_sha256(),
                           (int)keyLen, out.data()) != 1) {
        std::string ossl = drainOpenSslErrors();
        std::cerr << "[SecurityUtils] PBKDF2 failed: passLen=" << password.size()
                  << " saltLen=" << saltLen << " iter=" << iterations
                  << " keyLen=" << keyLen << " openssl_err=" << ossl << std::endl;
        throw std::runtime_error("PBKDF2 failed: " + ossl);
    }
    return out;
}

std::string SecurityUtils::encryptPassword(const std::string &password) {
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));
    auto hash = pbkdf2(password, salt, sizeof(salt));
    return "$pbkdf2$" + toHex(salt, sizeof(salt)) + "$" + toHex(hash.data(), hash.size());
}

bool SecurityUtils::matchesPassword(const std::string &raw, const std::string &encoded) {
    // 支持旧的 BCrypt 格式（$2a$ 开头）和新的 PBKDF2 格式
    if (encoded.rfind("$pbkdf2$", 0) == 0) {
        // 解析 $pbkdf2$<salt>$<hash>
        auto p1 = encoded.find('$', 8);
        if (p1 == std::string::npos) return false;
        std::string saltHex = encoded.substr(8, p1 - 8);
        std::string hashHex = encoded.substr(p1 + 1);
        auto salt = fromHex(saltHex);
        auto expected = fromHex(hashHex);
        auto actual = pbkdf2(raw, salt.data(), salt.size());
        // 常量时间比较
        if (actual.size() != expected.size()) return false;
        unsigned char diff = 0;
        for (size_t i = 0; i < actual.size(); ++i)
            diff |= actual[i] ^ expected[i];
        return diff == 0;
    }
    // 如果是 BCrypt 格式但没有 bcrypt 库，直接返回 false（需要重置密码）
    return false;
}
#endif
