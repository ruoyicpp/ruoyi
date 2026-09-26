/**
 * @file JwtUtils.h
 * @brief JWT 令牌工具 — 处理 JWT 令牌的生成、验证、解析
 * 
 * 功能概述：
 *   - 令牌生成：创建 JWT 令牌，包含用户信息和过期时间
 *   - 令牌验证：验证 JWT 签名和过期时间
 *   - 令牌解析：从 JWT 中提取用户信息
 *   - 算法支持：支持 HS256（对称）和 RS256（非对称）两种签名算法
 * 
 * 核心特性：
 *   - 双算法支持：优先使用 RS256（如果配置了私钥），否则使用 HS256
 *   - 配置管理：从 config.json 加载 JWT 配置
 *   - 密钥管理：支持从文件读取 PEM 格式的公私钥
 *   - 标准 JWT：遵循 RFC 7519 标准
 * 
 * 配置项（config.json）：
 *   - jwt.secret: HS256 对称密钥（默认 "ruoyi-cpp-secret"）
 *   - jwt.issuer: JWT 签发者（默认 "ruoyi.cpp.issuer"）
 *   - jwt.audience: JWT 受众（默认 "ruoyi.cpp.audience"）
 *   - jwt.expire_minutes: 令牌过期时间（分钟，默认 30）
 *   - jwt.jwt_expire_days: JWT 过期天数（默认 7）
 *   - jwt.private_key_file: RS256 私钥文件路径（可选）
 *   - jwt.public_key_file: RS256 公钥文件路径（可选）
 */

/**
 * @file JwtUtils.h
 * @brief JWT 令牌生成和验证工具
 * 
 * 功能概述：
 *   - JWT 令牌生成：使用 HS256 算法签名，支持自定义过期时间和声明
 *   - JWT 令牌验证：验证签名、过期时间、声明内容
 *   - 自动续期：支持令牌自动续期机制
 *   - 黑名单管理：支持令牌黑名单，防止已注销的令牌被重用
 * 
 * 使用示例：
 *   // 生成令牌
 *   std::string token = JwtUtils::generateToken(userId, "admin");
 *   
 *   // 验证令牌
 *   auto claims = JwtUtils::verifyToken(token);
 *   if (claims) {
 *       long userId = claims->get<long>("userId");
 *   }
 * 
 * 配置项（config.json）：
 *   - jwt.secret: 签名密钥（至少 16 位）
 *   - jwt.expireSeconds: 令牌过期时间（秒）
 *   - jwt.refreshThreshold: 自动续期阈值（秒）
 */

#pragma once
#include <string>
#include <chrono>
#include <stdexcept>
#include <fstream>
#include <cstdlib>
#include <random>
#include <iostream>
#include <filesystem>
#ifndef _WIN32
#  include <sys/stat.h>
#endif
#include <jwt-cpp/traits/open-source-parsers-jsoncpp/defaults.h>
#include <drogon/drogon.h>
#include "Constants.h"

///< JWT 使用 jsoncpp traits 进行 JSON 处理
using jwt_traits = jwt::traits::open_source_parsers_jsoncpp;

/**
 * @class JwtUtils
 * @brief JWT 令牌工具类
 * 
 * 对应 RuoYi 中的 JWTEncryption，提供 JWT 令牌的生成、验证、解析功能。
 * 采用单例模式，全局唯一配置实例。
 * 
 * 支持 HS256（对称密钥）和 RS256（公私钥）两种签名算法。
 */
class JwtUtils {
public:
    /**
     * @struct Config
     * @brief JWT 配置结构体
     */
    struct Config {
        std::string secret;       ///< HS256 对称密钥（兼容旧配置）
        std::string privateKey;   ///< RS256 私钥 PEM 内容（优先）
        std::string publicKey;    ///< RS256 公钥 PEM 内容
        std::string issuer;       ///< JWT 签发者
        std::string audience;     ///< JWT 受众
        int expireMinutes = 30;   ///< 令牌过期时间（分钟）
        int jwtExpireDays = 7;    ///< JWT 过期天数
        
        /**
         * @brief 是否使用 RS256 算法
         * @return 如果配置了私钥返回 true，否则使用 HS256
         */
        bool useRS256() const { return !privateKey.empty(); }
    };

    /**
     * @brief 获取 JWT 配置单例
     * @return 全局唯一的 Config 实例
     */
    static Config &config() {
        static Config cfg;
        return cfg;
    }

    /**
     * @brief 从文件读取 PEM 格式的密钥内容
     * 
     * @param path 密钥文件路径
     * @return PEM 格式的密钥内容，如果文件不存在返回空字符串
     */
    static std::string readPem(const std::string &path) {
        std::ifstream f(path);
        if (!f) return {};
        return {std::istreambuf_iterator<char>(f), {}};
    }

    /// 生成 64 字符随机 hex 密钥（256 位）
    static std::string genRandomSecret() {
        static const char hex[] = "0123456789abcdef";
        std::random_device rd;
        std::mt19937_64 gen(rd() ^ (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count());
        std::string s(64, '0');
        for (auto &ch : s) ch = hex[gen() & 0xF];
        return s;
    }

    /// 密钥写盘：trim 后单行存储，Linux 下 chmod 600
    static bool writeSecretFile(const std::string &path, std::string secret) {
        while (!secret.empty() && (secret.back() == '\n' || secret.back() == '\r' || secret.back() == ' '))
            secret.pop_back();
        std::ofstream f(path, std::ios::trunc);
        if (!f) return false;
        f << secret << "\n";
        f.close();
#ifndef _WIN32
        chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
        return true;
    }

    /**
     * @brief 从 config.json 加载 JWT 配置
     * 
     * 从 Drogon 应用配置中读取 JWT 相关配置项。
     * 应在应用启动时调用一次。
     * 
     * @note 
     *   - 如果配置了 RS256 密钥文件，会优先使用 RS256 算法
     *   - 否则使用 HS256 对称密钥
     */
    static void loadConfig() {
        auto &c = config();
        auto jwtCfg = drogon::app().getCustomConfig()["jwt"];
        c.secret        = jwtCfg.get("secret",           "ruoyi-cpp-secret").asString();
        c.issuer        = jwtCfg.get("issuer",           "ruoyi.cpp.issuer").asString();
        c.audience      = jwtCfg.get("audience",         "ruoyi.cpp.audience").asString();
        c.expireMinutes = jwtCfg.get("expire_minutes",   30).asInt();
        c.jwtExpireDays = jwtCfg.get("jwt_expire_days",  7).asInt();
        // RS256 密钥文件（可选，有则优先使用 RS256）
        std::string privFile   = jwtCfg.get("private_key_file", "").asString();
        std::string pubFile    = jwtCfg.get("public_key_file",  "").asString();
        std::string secretFile = jwtCfg.get("secret_file",      "").asString();
        // 环境变量覆盖敏感字段（优先级高于 config.json，供生产部署不下发明文 secret）
        auto envOr = [](const char* name, std::string& out) {
            const char* v = std::getenv(name);
            if (v && *v) out = v;
        };
        envOr("RUOYI_JWT_SECRET",           c.secret);
        envOr("RUOYI_JWT_PRIVATE_KEY_FILE", privFile);
        envOr("RUOYI_JWT_PUBLIC_KEY_FILE",  pubFile);
        envOr("RUOYI_JWT_SECRET_FILE",      secretFile);
        // secret_file：独立密钥文件，优先级高于 config.json 内联 secret。
        // 文件不存在时自动生成 64 字符随机 hex 密钥并写盘（0600），
        // 此后 config.json 即使外泄也不含有效密钥。
        if (!secretFile.empty()) {
            std::string fs = readPem(secretFile);
            // 密钥文件单行存储，读回后去掉尾部换行/空白，否则与生成值不一致
            while (!fs.empty() && (fs.back() == '\n' || fs.back() == '\r' ||
                                   fs.back() == ' ' || fs.back() == '\t'))
                fs.pop_back();
            size_t b = fs.find_first_not_of(" \t");
            if (b != std::string::npos && b > 0) fs = fs.substr(b);
            if (fs.empty()) {
                // 无感迁移：config.json 内联 secret 有效时直接落盘为密钥文件，
                // 已签发 token 不失效；默认/过短密钥才随机生成
                if (!c.secret.empty() && c.secret.size() >= 16 &&
                    c.secret != "ruoyi-cpp-secret") {
                    fs = c.secret;
                } else {
                    fs = genRandomSecret();
                }
                if (writeSecretFile(secretFile, fs))
                    std::cout << "[JwtUtils] 已生成密钥文件 " << secretFile << std::endl;
            }
            if (!fs.empty()) c.secret = fs;
        }
        if (!privFile.empty()) c.privateKey = readPem(privFile);
        if (!pubFile.empty())  c.publicKey  = readPem(pubFile);
    }

    /**
     * @brief 签发 JWT 令牌
     * 
     * 创建包含用户信息的 JWT 令牌。
     * 如果配置了 RS256 私钥则使用 RS256，否则使用 HS256。
     * 
     * @param uuid 令牌唯一标识（UUID）
     * @param userId 用户 ID
     * @param userName 用户名
     * @param deptId 部门 ID
     * 
     * @return JWT 令牌字符串
     * 
     * @note 
     *   - JWT 过期时间由 config.jwtExpireDays 控制
     *   - 令牌包含 issuer、audience、issued_at、expires_at 等标准声明
     *   - 令牌包含 uuid、userId、userName、deptId 等自定义声明
     */
    static std::string createToken(const std::string &uuid, long userId,
                                   const std::string &userName, long deptId) {
        auto &c = config();
        auto now = std::chrono::system_clock::now();
        auto exp = now + std::chrono::hours(c.jwtExpireDays * 24);

        auto builder = jwt::create<jwt_traits>()
            .set_issuer(c.issuer)
            .set_audience(c.audience)
            .set_issued_at(now)
            .set_expires_at(exp)
            .set_payload_claim(Constants::LOGIN_USER_KEY, jwt::basic_claim<jwt_traits>(uuid))
            .set_payload_claim("userId",   jwt::basic_claim<jwt_traits>(std::to_string(userId)))
            .set_payload_claim("userName", jwt::basic_claim<jwt_traits>(userName))
            .set_payload_claim("deptId",   jwt::basic_claim<jwt_traits>(std::to_string(deptId)));

        if (c.useRS256())
            return builder.sign(jwt::algorithm::rs256{c.publicKey, c.privateKey});
        return builder.sign(jwt::algorithm::hs256{c.secret});
    }

    // 解析 JWT 获取 uuid
    static std::string parseUuid(const std::string &token) {
        auto &c = config();
        auto decoded = jwt::decode<jwt_traits>(token);
        if (c.useRS256()) {
            jwt::verify<jwt_traits>()
                .allow_algorithm(jwt::algorithm::rs256{c.publicKey, c.privateKey})
                .with_issuer(c.issuer)
                .with_audience(c.audience)
                .verify(decoded);
        } else {
            jwt::verify<jwt_traits>()
                .allow_algorithm(jwt::algorithm::hs256{c.secret})
                .with_issuer(c.issuer)
                .with_audience(c.audience)
                .verify(decoded);
        }
        return decoded.get_payload_claim(Constants::LOGIN_USER_KEY).as_string();
    }


};
