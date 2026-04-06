#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include "../services/DatabaseService.h"
#include "HardwareId.h"
#include "VaultClient.h"

// 设备绑定：RSA 签名硬件指纹 + 存储/验证 DB
// 重置方法：DELETE FROM sys_device_binding WHERE id=1 后重启
namespace DeviceBinding {

    struct Config {
        bool          enabled    = false;
        std::string   localKeyFile;  // 本地私钥 PEM 路径（Vault 不可用时降级）
        VaultClient::Config vault;
        std::string   vaultSecretPath;  // e.g. secret/ruoyi-cpp
        std::string   vaultKeyField;    // e.g. device_private_key
    };

    // ── Base64 ────────────────────────────────────────────────────────────
    inline std::string b64Encode(const unsigned char* data, size_t len) {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* mem = BIO_new(BIO_s_mem());
        BIO_push(b64, mem);
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO_write(b64, data, (int)len);
        BIO_flush(b64);
        char* p; long sz = BIO_get_mem_data(mem, &p);
        std::string s(p, sz);
        BIO_free_all(b64);
        return s;
    }

    inline std::vector<unsigned char> b64Decode(const std::string& s) {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* mem = BIO_new_mem_buf(s.c_str(), (int)s.size());
        BIO_push(b64, mem);
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        std::vector<unsigned char> out(s.size());
        int n = BIO_read(b64, out.data(), (int)out.size());
        BIO_free_all(b64);
        out.resize(n > 0 ? n : 0);
        return out;
    }

    // ── RSA 签名 / 验签 ───────────────────────────────────────────────────
    inline std::string rsaSign(const std::string& data, const std::string& privPem) {
        BIO* bio = BIO_new_mem_buf(privPem.c_str(), -1);
        EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (!key) return "";

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, key);
        EVP_DigestSignUpdate(ctx, data.c_str(), data.size());
        size_t sigLen = 0;
        EVP_DigestSignFinal(ctx, nullptr, &sigLen);
        std::vector<unsigned char> sig(sigLen);
        EVP_DigestSignFinal(ctx, sig.data(), &sigLen);
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(key);
        return b64Encode(sig.data(), sigLen);
    }

    inline bool rsaVerify(const std::string& data, const std::string& sigB64,
                          const std::string& pubPem) {
        auto sig = b64Decode(sigB64);
        if (sig.empty()) return false;

        BIO* bio = BIO_new_mem_buf(pubPem.c_str(), -1);
        EVP_PKEY* key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (!key) return false;

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, key);
        EVP_DigestVerifyUpdate(ctx, data.c_str(), data.size());
        int ok = EVP_DigestVerifyFinal(ctx, sig.data(), sig.size());
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(key);
        return ok == 1;
    }

    // 从私钥 PEM 提取公钥 PEM
    inline std::string pubFromPriv(const std::string& privPem) {
        BIO* bio = BIO_new_mem_buf(privPem.c_str(), -1);
        EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (!key) return "";
        BIO* mem = BIO_new(BIO_s_mem());
        PEM_write_bio_PUBKEY(mem, key);
        char* p; long sz = BIO_get_mem_data(mem, &p);
        std::string s(p, sz);
        BIO_free(mem);
        EVP_PKEY_free(key);
        return s;
    }

    // 生成 RSA-2048 私钥 PEM（Vault 和本地文件均不可用时自举）
    inline std::string generatePrivKey() {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
        EVP_PKEY_keygen_init(ctx);
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);
        EVP_PKEY* key = nullptr;
        EVP_PKEY_keygen(ctx, &key);
        EVP_PKEY_CTX_free(ctx);
        if (!key) return "";
        BIO* mem = BIO_new(BIO_s_mem());
        PEM_write_bio_PrivateKey(mem, key, nullptr, nullptr, 0, nullptr, nullptr);
        char* p; long sz = BIO_get_mem_data(mem, &p);
        std::string s(p, sz);
        BIO_free(mem);
        EVP_PKEY_free(key);
        return s;
    }

    // ── 获取私钥（Vault → 本地文件 → 自动生成）───────────────────────────
    inline std::string acquirePrivKey(const Config& cfg) {
        // 1. Vault 子进程
        if (cfg.vault.enabled) {
            auto k = VaultClient::getField(cfg.vault, cfg.vaultSecretPath, cfg.vaultKeyField);
            if (!k.empty()) { std::cout << "[DeviceBinding] 私钥来源: Vault" << std::endl; return k; }
        }
        // 2. 本地文件
        if (!cfg.localKeyFile.empty()) {
            std::ifstream f(cfg.localKeyFile);
            if (f.is_open()) {
                std::string s((std::istreambuf_iterator<char>(f)), {});
                if (!s.empty()) { std::cout << "[DeviceBinding] 私钥来源: " << cfg.localKeyFile << std::endl; return s; }
            }
        }
        // 3. 自动生成并保存到本地文件
        std::cout << "[DeviceBinding] 生成新 RSA-2048 私钥" << std::endl;
        auto k = generatePrivKey();
        if (!k.empty() && !cfg.localKeyFile.empty()) {
            std::ofstream f(cfg.localKeyFile);
            f << k;
            std::cout << "[DeviceBinding] 私钥已保存到 " << cfg.localKeyFile << std::endl;
        }
        return k;
    }

    // ── 启动时检查（返回 false 则拒绝启动）────────────────────────────────
    inline bool check(const Config& cfg) {
        if (!cfg.enabled) return true;

        auto& db = DatabaseService::instance();
        std::string fp = HardwareId::compute();
        std::cout << "[DeviceBinding] 硬件指纹: " << fp.substr(0, 16) << "..." << std::endl;

        auto res = db.query("SELECT fingerprint, signature, public_key FROM sys_device_binding WHERE id=1");

        if (res.rows() == 0) {
            // 首次注册
            std::string priv = acquirePrivKey(cfg);
            if (priv.empty()) {
                std::cout << "[DeviceBinding] 无法获取私钥，跳过设备绑定" << std::endl;
                return true;
            }
            std::string pub = pubFromPriv(priv);
            std::string sig = rsaSign(fp, priv);
            if (sig.empty()) {
                std::cout << "[DeviceBinding] RSA 签名失败，跳过设备绑定" << std::endl;
                return true;
            }
            db.execParams(
                "INSERT INTO sys_device_binding(id,fingerprint,signature,public_key,bound_at) "
                "VALUES(1,$1,$2,$3,NOW())",
                {fp, sig, pub});
            std::cout << "[DeviceBinding] ✅ 设备首次注册成功" << std::endl;
            return true;
        }

        // 验证已注册设备
        std::string storedFp  = res.str(0, 0);
        std::string storedSig = res.str(0, 1);
        std::string storedPub = res.str(0, 2);

        if (storedFp != fp) {
            std::cout << "[DeviceBinding] ❌ 硬件指纹不匹配！\n"
                      << "  期望: " << storedFp.substr(0, 16) << "...\n"
                      << "  当前: " << fp.substr(0, 16) << "...\n"
                      << "  重置方法: DELETE FROM sys_device_binding WHERE id=1; 然后重启。"
                      << std::endl;
            return false;
        }
        if (!rsaVerify(fp, storedSig, storedPub)) {
            std::cout << "[DeviceBinding] ❌ RSA 签名验证失败！数据库记录可能被篡改。" << std::endl;
            return false;
        }
        std::cout << "[DeviceBinding] ✅ 设备验证通过" << std::endl;
        return true;
    }
}
