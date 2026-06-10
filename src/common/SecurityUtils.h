/**
 * @file SecurityUtils.h
 * @brief 安全工具类 — 处理密码加密、验证、令牌提取等安全相关功能
 * 
 * 功能概述：
 *   - 令牌提取：从 HTTP 请求中提取 JWT 令牌
 *   - 密码加密：支持 PBKDF2-SHA256 和国密 PBKDF2-SM3
 *   - 密码验证：自动识别哈希格式，兼容多种算法
 *   - 敏感字段加密：使用 SM4-GCM 加密敏感数据
 *   - 哈希计算：支持 SM3 哈希计算
 *   - 权限检查：判断用户是否为管理员
 * 
 * 核心特性：
 *   - 国密支持：可配置使用国密算法（SM3、SM4）
 *   - 算法兼容：自动识别密码哈希格式，支持平滑升级
 *   - 配置驱动：从 config.json 读取加密配置
 * 
 * 配置项（config.json）：
 *   - security.use_gm: 是否启用国密算法（默认 false）
 *   - security.sm4_key: SM4 加密密钥（默认空，不足16字节自动补零）
 */

#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <optional>
#include "Constants.h"
#include "GmCrypto.h"

struct LoginUser;

/**
 * @class SecurityUtils
 * @brief 安全工具类单例
 * 
 * 对应 RuoYi 中的 SecurityUtils，提供密码加密、验证、令牌提取等功能。
 * 支持传统 PBKDF2-SHA256 和国密 PBKDF2-SM3 两种密码哈希算法。
 */
class SecurityUtils {
public:
    /**
     * @brief 从 HTTP 请求中提取 JWT 令牌
     * 
     * 按优先级从以下位置提取令牌：
     *   1. Authorization 请求头
     *   2. token 请求头
     *   3. token 查询参数
     * 
     * 如果令牌以 TOKEN_PREFIX（"Bearer "）开头，自动移除前缀。
     * 
     * @param req HTTP 请求对象
     * @return JWT 令牌字符串，如果不存在返回空字符串
     */
    static std::string getToken(const drogon::HttpRequestPtr &req) {
        std::string token = req->getHeader("Authorization");
        if (token.empty()) token = req->getHeader("token");
        if (token.empty()) token = req->getParameter("token");
        if (!token.empty() && token.rfind(Constants::TOKEN_PREFIX, 0) == 0)
            token = token.substr(Constants::TOKEN_PREFIX.size());
        return token;
    }

    /**
     * @brief 判断用户是否为管理员
     * @param userId 用户 ID
     * @return 如果 userId 等于 ADMIN_USER_ID（通常为 1）返回 true
     */
    static bool isAdmin(long userId) { return userId == Constants::ADMIN_USER_ID; }

    /**
     * @brief 判断角色是否为管理员角色
     * @param roleId 角色 ID
     * @return 如果 roleId 等于 ADMIN_ROLE_ID（通常为 1）返回 true
     */
    static bool isAdminRole(long roleId) { return roleId == Constants::ADMIN_ROLE_ID; }

    /**
     * @brief 是否启用国密算法
     * 
     * 从 config.json 的 security.use_gm 配置项读取。
     * 结果会被缓存，避免重复读取配置。
     * 
     * @return 如果启用国密算法返回 true，否则返回 false
     */
    static bool useGm() {
        static int cached = -1;
        if (cached == -1) {
            try {
                cached = drogon::app().getCustomConfig()["security"]
                             .get("use_gm", false).asBool() ? 1 : 0;
            } catch (...) { cached = 0; }
        }
        return cached == 1;
    }

    /**
     * @brief 加密密码
     * 
     * 根据配置选择加密算法：
     *   - 如果 use_gm=true，使用 PBKDF2-SM3（国密）
     *   - 否则使用 PBKDF2-SHA256
     * 
     * @param password 明文密码
     * @return 加密后的密码哈希
     */
    static std::string encryptPassword(const std::string &password) {
        if (useGm()) return GmCrypto::hashPassword(password);
        return encryptPasswordInternal(password);
    }

    /**
     * @brief 验证密码
     * 
     * 自动识别密码哈希格式，支持多种算法：
     *   - 如果是国密格式，使用 SM3 验证
     *   - 否则使用 SHA256 验证
     * 
     * 这样可以在升级加密算法时平滑过渡。
     * 
     * @param raw 明文密码
     * @param encoded 数据库中存储的密码哈希
     * @return 密码是否匹配
     */
    static bool matchesPassword(const std::string &raw, const std::string &encoded) {
        if (GmCrypto::isGmPasswordHash(encoded))
            return GmCrypto::verifyPassword(raw, encoded);
        return matchesPasswordInternal(raw, encoded);
    }

    /**
     * @brief SM4-GCM 加密敏感字段
     * 
     * 使用 SM4 算法加密敏感数据（如手机号、身份证号等）。
     * 加密密钥从 config.json 的 security.sm4_key 读取。
     * 
     * @param plaintext 明文数据
     * @return 加密后的密文
     */
    static std::string sm4Encrypt(const std::string &plaintext) {
        return GmCrypto::sm4Encrypt(plaintext, getSm4Key());
    }

    /**
     * @brief SM4-GCM 解密敏感字段
     * 
     * @param ciphertext 加密后的密文
     * @return 解密后的明文
     */
    static std::string sm4Decrypt(const std::string &ciphertext) {
        return GmCrypto::sm4Decrypt(ciphertext, getSm4Key());
    }

    /**
     * @brief SM3 哈希计算
     * 
     * 计算输入字符串的 SM3 哈希值，返回十六进制字符串。
     * 
     * @param s 输入字符串
     * @return SM3 哈希值（十六进制）
     */
    static std::string sm3Hex(const std::string &s) { return GmCrypto::sm3Hex(s); }

private:
    static std::string getSm4Key() {
        static std::string key;
        if (key.empty()) {
            try {
                key = drogon::app().getCustomConfig()["security"]
                          .get("sm4_key", "").asString();
            } catch (...) {}
            key.resize(16, '\0');  // 不足 16 字节补零
        }
        return key;
    }

    // 原 PBKDF2-SHA256 实现（保持 .cc 文件不变）
    static std::string encryptPasswordInternal(const std::string &password);
    static bool matchesPasswordInternal(const std::string &raw, const std::string &encoded);
public:

    // 生成 token cache key
    static std::string getTokenKey(const std::string &uuid) {
        return Constants::LOGIN_TOKEN_KEY + uuid;
    }

    // ── 安全的字符串到整数转换 ────────────────────────────────────────────
    // 不会抛异常；非数字/越界返回默认值（默认 0）。用于 URL 路径参数等不可信输入。
    static long parseLong(const std::string &s, long def = 0) {
        if (s.empty()) return def;
        try { return std::stol(s); } catch (...) { return def; }
    }
    static long long parseLongLong(const std::string &s, long long def = 0) {
        if (s.empty()) return def;
        try { return std::stoll(s); } catch (...) { return def; }
    }
    static int parseInt(const std::string &s, int def = 0) {
        if (s.empty()) return def;
        try { return std::stoi(s); } catch (...) { return def; }
    }
};
