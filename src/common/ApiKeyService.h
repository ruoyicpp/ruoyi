#pragma once
#include <string>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include "../services/DatabaseService.h"
#include "LoginUser.h"

// f16 API Key 管理服务（header-only）
//
// 工作流：
//   1) 管理员通过 /system/apikey 接口创建 key，绑定到一个 user_id（继承其权限）
//   2) 服务端生成 48 字符随机 key（如 "ak_xxxxxxxxxxx..."），仅此一次明文返回
//   3) DB 存储 sha256(key) 防止泄漏（同 git/github access token 模式）
//   4) 客户端后续调用：Header `X-API-Key: <key>` 或 query `?apiKey=<key>`
//   5) 中间件查 sha256 命中 sys_apikey → 加载对应 user → 模拟登录态注入
class ApiKeyService {
public:
    // 验签产出的"虚拟登录用户"快照（含权限角色，5 分钟 TTL 缓存）
    struct CachedUser {
        LoginUser     user;
        long long     expireAt = 0;     // unix epoch ms
        long          apiKeyId = 0;     // 对应 sys_apikey.id
    };

    static ApiKeyService& instance() {
        static ApiKeyService inst;
        return inst;
    }

    // 生成 48 字符 key（前缀 "ak_" + 45 字符 hex / base62）
    // 此函数纯计算，不写库
    static std::string generateKey() {
        unsigned char raw[24];
        if (RAND_bytes(raw, sizeof(raw)) != 1) return "";
        // 24 字节 → 48 hex 字符；加 "ak_" 前缀方便识别（识别可以独立提取）
        std::string body;
        body.reserve(48);
        static const char* hex = "0123456789abcdef";
        for (int i = 0; i < (int)sizeof(raw); ++i) {
            body += hex[raw[i] >> 4];
            body += hex[raw[i] & 0xF];
        }
        return "ak_" + body;   // 总长 51 字符，唯一前缀方便日志/审计识别
    }

    // 计算 sha256(key) → 64 hex 字符
    static std::string hashKey(const std::string& key) {
        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(key.data()), key.size(), digest);
        std::string out;
        out.reserve(64);
        static const char* hex = "0123456789abcdef";
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            out += hex[digest[i] >> 4];
            out += hex[digest[i] & 0xF];
        }
        return out;
    }

    // 取 key 的前 8 字符作为 UI 展示前缀（隐藏完整 key）
    static std::string keyPrefix(const std::string& key) {
        return key.substr(0, std::min<size_t>(11, key.size()));   // "ak_" + 前 8 hex
    }

    // 验证 key 并返回虚拟登录用户（含权限）
    // 失败返回 nullopt；errMsg 可选填充失败原因
    std::optional<LoginUser> verifyKey(const std::string& key, std::string* errMsg = nullptr) {
        if (key.empty()) return std::nullopt;
        std::string h = hashKey(key);

        // 缓存命中（5 分钟 TTL）
        long long now = nowMs();
        {
            std::lock_guard<std::mutex> lk(cacheMu_);
            auto it = cache_.find(h);
            if (it != cache_.end() && it->second.expireAt > now) {
                touchLastUsed(it->second.apiKeyId);
                return it->second.user;
            }
        }

        // DB 查询
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT id, user_id, enabled, expire_time FROM sys_apikey WHERE key_hash=$1",
            {h});
        if (!res.ok() || res.rows() == 0) {
            if (errMsg) *errMsg = "API Key 不存在";
            return std::nullopt;
        }
        long  id        = res.longVal(0, 0);
        long  userId    = res.longVal(0, 1);
        int   enabled   = res.intVal(0, 2);
        std::string exp = res.str(0, 3);
        if (enabled == 0) {
            if (errMsg) *errMsg = "API Key 已禁用";
            return std::nullopt;
        }
        if (!exp.empty() && exp <= currentTimestamp()) {
            if (errMsg) *errMsg = "API Key 已过期";
            return std::nullopt;
        }

        // 加载关联用户（含权限 + 角色）
        auto uRes = db.queryParams(
            "SELECT user_name, dept_id FROM sys_user WHERE user_id=$1",
            {std::to_string(userId)});
        if (!uRes.ok() || uRes.rows() == 0) {
            if (errMsg) *errMsg = "关联用户不存在";
            return std::nullopt;
        }
        LoginUser u;
        u.userId   = userId;
        u.userName = uRes.str(0, 0);
        u.deptId   = uRes.longVal(0, 1);
        u.token    = "apikey:" + std::to_string(id);   // 区别 JWT 令牌
        u.loginTime = now;
        u.expireTime = now + 3600 * 1000;

        // 加载权限
        auto permsRes = db.queryParams(
            "SELECT DISTINCT m.perms FROM sys_menu m "
            "INNER JOIN sys_role_menu rm ON rm.menu_id=m.menu_id "
            "INNER JOIN sys_user_role ur ON ur.role_id=rm.role_id "
            "WHERE ur.user_id=$1 AND m.perms IS NOT NULL AND m.perms<>''",
            {std::to_string(userId)});
        if (permsRes.ok())
            for (int i = 0; i < permsRes.rows(); ++i)
                u.permissions.push_back(permsRes.str(i, 0));

        auto rolesRes = db.queryParams(
            "SELECT r.role_key FROM sys_role r "
            "INNER JOIN sys_user_role ur ON ur.role_id=r.role_id "
            "WHERE ur.user_id=$1",
            {std::to_string(userId)});
        if (rolesRes.ok())
            for (int i = 0; i < rolesRes.rows(); ++i)
                u.roles.push_back(rolesRes.str(i, 0));

        // 写缓存
        {
            std::lock_guard<std::mutex> lk(cacheMu_);
            CachedUser c;
            c.user = u;
            c.expireAt = now + 5 * 60 * 1000;   // 5 分钟
            c.apiKeyId = id;
            cache_[h] = std::move(c);
        }
        touchLastUsed(id);
        return u;
    }

    // 失效缓存（如管理员删除 key 后调用）
    void invalidate(const std::string& key) {
        std::lock_guard<std::mutex> lk(cacheMu_);
        cache_.erase(hashKey(key));
    }
    void invalidateAll() {
        std::lock_guard<std::mutex> lk(cacheMu_);
        cache_.clear();
    }

private:
    std::unordered_map<std::string, CachedUser> cache_;   // key_hash → CachedUser
    std::mutex                                  cacheMu_;

    static long long nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    static std::string currentTimestamp() {
        auto t = std::time(nullptr);
        char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        return buf;
    }

    // 异步更新 last_used_at（不阻塞请求）
    void touchLastUsed(long apiKeyId) {
        try {
            DatabaseService::instance().execParams(
                "UPDATE sys_apikey SET last_used_at=CURRENT_TIMESTAMP WHERE id=$1",
                {std::to_string(apiKeyId)});
        } catch (...) {}
    }
};
