#pragma once
#include <string>
#include <chrono>
#include <stdexcept>
// 使用 jsoncpp 替换 jwt-cpp 的 JSON 处理（避免依赖 picojson）
#include <jwt-cpp/traits/open-source-parsers-jsoncpp/defaults.h>
#include <drogon/drogon.h>
#include "Constants.h"

// jwt-cpp 使用 jsoncpp traits
using jwt_traits = jwt::traits::open_source_parsers_jsoncpp;

// 对应 RuoYi.Net JWTEncryption
class JwtUtils {
public:
    struct Config {
        std::string secret;
        std::string issuer;
        std::string audience;
        int expireMinutes = 30;
        int jwtExpireDays = 7;
    };

    static Config &config() {
        static Config cfg;
        return cfg;
    }

    // 从 drogon config.json 加载配置
    static void loadConfig() {
        auto &c = config();
        auto jwtCfg = drogon::app().getCustomConfig()["jwt"];
        c.secret        = jwtCfg.get("secret",          "ruoyi-cpp-secret").asString();
        c.issuer        = jwtCfg.get("issuer",           "ruoyi.cpp.issuer").asString();
        c.audience      = jwtCfg.get("audience",         "ruoyi.cpp.audience").asString();
        c.expireMinutes = jwtCfg.get("expire_minutes",   30).asInt();
        c.jwtExpireDays = jwtCfg.get("jwt_expire_days",  7).asInt();
    }

    // 签发 JWT
    static std::string createToken(const std::string &uuid, long userId,
                                   const std::string &userName, long deptId) {
        auto &c = config();
        auto now = std::chrono::system_clock::now();
        auto exp = now + std::chrono::hours(c.jwtExpireDays * 24);

        return jwt::create<jwt_traits>()
            .set_issuer(c.issuer)
            .set_audience(c.audience)
            .set_issued_at(now)
            .set_expires_at(exp)
            .set_payload_claim(Constants::LOGIN_USER_KEY, jwt::basic_claim<jwt_traits>(uuid))
            .set_payload_claim("userId",   jwt::basic_claim<jwt_traits>(std::to_string(userId)))
            .set_payload_claim("userName", jwt::basic_claim<jwt_traits>(userName))
            .set_payload_claim("deptId",   jwt::basic_claim<jwt_traits>(std::to_string(deptId)))
            .sign(jwt::algorithm::hs256{c.secret});
    }

    // 解析 JWT 获取 uuid
    static std::string parseUuid(const std::string &token) {
        auto &c = config();
        auto decoded = jwt::decode<jwt_traits>(token);
        auto verifier = jwt::verify<jwt_traits>()
            .allow_algorithm(jwt::algorithm::hs256{c.secret})
            .with_issuer(c.issuer)
            .with_audience(c.audience);
        verifier.verify(decoded);
        return decoded.get_payload_claim(Constants::LOGIN_USER_KEY).as_string();
    }


};
