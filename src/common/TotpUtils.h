#pragma once
#include <string>
#include <cstdint>
#include <ctime>
#include <openssl/hmac.h>
#include <openssl/rand.h>

// TOTP RFC 6238 实现（纯 OpenSSL，无第三方依赖）
struct TotpUtils {
    // 生成 20 字节随机 Base32 密钥
    static std::string generateSecret() {
        unsigned char buf[20];
        RAND_bytes(buf, sizeof(buf));
        return base32Encode(buf, sizeof(buf));
    }

    // 验证 6 位 OTP（允许前后 1 个时间窗口的偏差）
    static bool verify(const std::string &secret, const std::string &otp, int window = 1) {
        auto bytes = base32Decode(secret);
        if (bytes.empty()) return false;
        uint64_t T = (uint64_t)std::time(nullptr) / 30;
        for (int i = -window; i <= window; ++i) {
            if (hotp(bytes, T + i) == otp) return true;
        }
        return false;
    }

    // 生成当前 OTP（用于测试）
    static std::string current(const std::string &secret) {
        auto bytes = base32Decode(secret);
        if (bytes.empty()) return "";
        return hotp(bytes, (uint64_t)std::time(nullptr) / 30);
    }

    // 生成 otpauth URI（供前端渲染二维码）
    static std::string otpauthUri(const std::string &secret,
                                  const std::string &account,
                                  const std::string &issuer = "RuoYi-Cpp") {
        return "otpauth://totp/" + urlEncode(issuer) + ":" + urlEncode(account) +
               "?secret=" + secret + "&issuer=" + urlEncode(issuer) + "&algorithm=SHA1&digits=6&period=30";
    }

private:
    static std::string hotp(const std::vector<uint8_t> &key, uint64_t counter) {
        uint8_t msg[8];
        for (int i = 7; i >= 0; --i) { msg[i] = counter & 0xFF; counter >>= 8; }
        unsigned char hmacBuf[EVP_MAX_MD_SIZE];
        unsigned int  hmacLen = 0;
        HMAC(EVP_sha1(), key.data(), (int)key.size(), msg, 8, hmacBuf, &hmacLen);
        int offset = hmacBuf[hmacLen - 1] & 0x0F;
        uint32_t code = ((hmacBuf[offset]   & 0x7F) << 24) |
                        ((hmacBuf[offset+1] & 0xFF) << 16) |
                        ((hmacBuf[offset+2] & 0xFF) <<  8) |
                         (hmacBuf[offset+3] & 0xFF);
        code %= 1000000;
        char buf[8]; snprintf(buf, sizeof(buf), "%06u", code);
        return buf;
    }

    static std::string base32Encode(const unsigned char *data, size_t len) {
        static const char *alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        std::string out;
        int buf = 0, bits = 0;
        for (size_t i = 0; i < len; ++i) {
            buf = (buf << 8) | data[i]; bits += 8;
            while (bits >= 5) { bits -= 5; out += alpha[(buf >> bits) & 0x1F]; }
        }
        if (bits) out += alpha[(buf << (5 - bits)) & 0x1F];
        return out;
    }

    static std::vector<uint8_t> base32Decode(const std::string &s) {
        std::vector<uint8_t> out;
        int buf = 0, bits = 0;
        for (char c : s) {
            int v = -1;
            if (c >= 'A' && c <= 'Z') v = c - 'A';
            else if (c >= 'a' && c <= 'z') v = c - 'a';
            else if (c >= '2' && c <= '7') v = c - '2' + 26;
            else if (c == '=') break;
            if (v < 0) continue;
            buf = (buf << 5) | v; bits += 5;
            if (bits >= 8) { bits -= 8; out.push_back((buf >> bits) & 0xFF); }
        }
        return out;
    }

    static std::string urlEncode(const std::string &s) {
        std::string r;
        for (unsigned char c : s) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') r += c;
            else { char buf[4]; snprintf(buf, sizeof(buf), "%%%02X", c); r += buf; }
        }
        return r;
    }
};
