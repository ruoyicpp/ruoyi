/**
 * @file LoginUser.h
 * @brief 登录用户信息结构体
 * 
 * 功能概述：
 *   - 用户信息存储：存储登录用户的基本信息、权限和角色
 *   - 会话管理：包含令牌、登录时间、过期时间等会话信息
 *   - 设备信息：记录登录设备的浏览器和操作系统
 *   - 序列化：支持 JSON 序列化和反序列化，用于缓存存储
 * 
 * 使用场景：
 *   - 登录后存储在 TokenCache 中
 *   - 从 HTTP 请求的 Attributes 中提取
 *   - 序列化后存入 Redis 缓存
 * 
 * 字段说明：
 *   - userId: 用户 ID（唯一标识）
 *   - deptId: 部门 ID
 *   - userName: 用户名
 *   - token: JWT 令牌（UUID 格式）
 *   - loginTime: 登录时间戳（毫秒）
 *   - expireTime: 令牌过期时间戳（毫秒）
 *   - ipAddr: 登录 IP 地址
 *   - loginLocation: IP 地理位置
 *   - browser: 浏览器名称
 *   - os: 操作系统名称
 *   - deptName: 部门名称
 *   - permissions: 权限字符串列表（如 "system:user:list"）
 *   - roles: 角色 key 列表（如 "admin"）
 */

#pragma once
#include <string>
#include <vector>
#include <json/json.h>

/**
 * @struct LoginUser
 * @brief 登录用户信息
 * 
 * 对应 RuoYi 中的 LoginUser，存储登录用户的完整信息。
 * 支持 JSON 序列化和反序列化，用于会话管理和缓存存储。
 */
struct LoginUser {
    long        userId      = 0;
    long        deptId      = 0;
    std::string userName;
    std::string password;   // 登录密码（仅在登录时传输，缓存中不持久化存储）
    std::string token;      // uuid 作为 Redis cache key
    long long   loginTime   = 0;
    long long   expireTime  = 0;
    std::string ipAddr;
    std::string loginLocation;
    std::string browser;
    std::string os;
    std::string deptName;
    std::vector<std::string> permissions;  // 权限标识列表，如 "system:user:list"
    std::vector<std::string> roles;        // 角色 key 列表，如 "admin"

    // 序列化当前对象为 JSON（用于存入 Redis 缓存）
    Json::Value toJson() const {
        Json::Value j;
        j["userId"]        = (Json::Int64)userId;
        j["deptId"]        = (Json::Int64)deptId;
        j["userName"]      = userName;
        j["token"]         = token;
        j["loginTime"]     = (Json::Int64)loginTime;
        j["expireTime"]    = (Json::Int64)expireTime;
        j["ipAddr"]        = ipAddr;
        j["loginLocation"] = loginLocation;
        j["browser"]       = browser;
        j["os"]            = os;
        j["deptName"]      = deptName;
        for (auto &p : permissions) j["permissions"].append(p);
        for (auto &r : roles)       j["roles"].append(r);
        return j;
    }

    /**
     * @brief 从 JSON 对象反序列化
     *
     * 用于从缓存或数据库恢复用户信息。
     *
     * @param j JSON 对象
     * @return 反序列化后的 LoginUser 对象
     */
    static LoginUser fromJson(const Json::Value &j) {
        LoginUser u;
        u.userId        = j["userId"].asInt64();
        u.deptId        = j["deptId"].asInt64();
        u.userName      = j["userName"].asString();
        u.token         = j["token"].asString();
        u.loginTime     = j["loginTime"].asInt64();
        u.expireTime    = j["expireTime"].asInt64();
        u.ipAddr        = j["ipAddr"].asString();
        u.loginLocation = j["loginLocation"].asString();
        u.browser       = j["browser"].asString();
        u.os            = j["os"].asString();
        u.deptName      = j.get("deptName", "").asString();
        for (auto &p : j["permissions"]) u.permissions.push_back(p.asString());
        for (auto &r : j["roles"])       u.roles.push_back(r.asString());
        return u;
    }
};
