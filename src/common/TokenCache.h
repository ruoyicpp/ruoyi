#pragma once
#include "ErrorLogger.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>
#include <memory>
#include <fstream>
#include <iostream>
#include <atomic>
#include <json/json.h>
#include <drogon/drogon.h>
#include <hiredis/hiredis.h>
#include "LoginUser.h"
#include "Constants.h"
#include "VramCache.h"

namespace {
    struct RedisConfig {
        bool enabled = false;
        std::string host = "127.0.0.1";
        int port = 6379;
        std::string password;
        int db = 0;
        std::string keyPrefix;
    };

    inline RedisConfig loadRedisConfig() {
        RedisConfig cfg;
        try {
            std::ifstream cfgFile("config.json");
            if (!cfgFile.is_open()) return cfg;
            Json::Value root;
            Json::CharReaderBuilder rb;
            std::string errs;
            if (!Json::parseFromStream(rb, cfgFile, &root, &errs)) return cfg;
            if (!root.isMember("redis")) return cfg;
            auto rc = root["redis"];
            cfg.enabled = rc.get("enabled", false).asBool();
            cfg.host = rc.get("host", "127.0.0.1").asString();
            cfg.port = rc.get("port", 6379).asInt();
            cfg.password = rc.get("password", "").asString();
            cfg.db = rc.get("db", 0).asInt();
            cfg.keyPrefix = rc.get("key_prefix", "").asString();
        } catch (...) {}

        // 环境变量覆盖敏感字段（优先级高于 config.json）
        auto envOr = [](const char* name, std::string& out) {
            const char* v = std::getenv(name);
            if (v && *v) out = v;
        };
        envOr("RUOYI_REDIS_PASSWORD", cfg.password);
        envOr("RUOYI_REDIS_HOST",     cfg.host);
        const char* ep = std::getenv("RUOYI_REDIS_PORT");
        if (ep && *ep) try { cfg.port = std::stoi(ep); } catch(...) {}
        return cfg;
    }

    class RedisConn {
        static constexpr int POOL_SIZE = 4;  // 连接池大小

        struct Slot {
            std::mutex           mu;
            redisContext*        ctx = nullptr;
            std::chrono::steady_clock::time_point nextRetryAt{};
        };

    public:
        static RedisConn &instance() {
            static RedisConn inst;
            return inst;
        }

        bool enabledByConfig() {
            auto cfg = loadRedisConfig();
            return cfg.enabled;
        }

        bool available() {
            if (!enabledByConfig()) return false;
            // 尝试任意一个 slot 能连上即可
            auto& slot = pickSlot();
            std::lock_guard<std::mutex> lk(slot.mu);
            ensureConnected(slot);
            bool ok = (slot.ctx != nullptr);
            int cur = ok ? 1 : 0;
            int prev = lastAvail_.exchange(cur);
            if (prev != -1 && prev != cur) {
                if (ok) {
                    std::cout << "[Cache] Redis is available, switch to redis" << std::endl;
                    needSync_.store(true);
                } else {
                    std::cout << "[Cache] Redis is unavailable, fallback to memory" << std::endl;
                }
            }
            return ok;
        }

        std::string prefixKey(const std::string &key) {
            auto cfg = loadRedisConfig();
            if (!cfg.enabled || cfg.keyPrefix.empty()) return key;
            return cfg.keyPrefix + key;
        }

        // 线程安全：round-robin 选连接，锁内执行命令
        template<typename... Args>
        redisReply* command(const char* fmt, Args&&... args) {
            auto& slot = pickSlot();
            std::lock_guard<std::mutex> lk(slot.mu);
            ensureConnected(slot);
            if (!slot.ctx) return nullptr;
            return (redisReply*)redisCommand(slot.ctx, fmt, std::forward<Args>(args)...);
        }

        // 旧接口兼容（尽量不用）
        redisContext *ctx() {
            auto& slot = pickSlot();
            std::lock_guard<std::mutex> lk(slot.mu);
            ensureConnected(slot);
            return slot.ctx;
        }

        void markBad() {
            // 关闭所有连接，触发重连
            for (auto& slot : pool_) {
                std::lock_guard<std::mutex> lk(slot.mu);
                closeSlot(slot);
            }
        }

    private:
        RedisConn() = default;
        ~RedisConn() {
            for (auto& slot : pool_) closeSlot(slot);
        }
        RedisConn(const RedisConn&) = delete;
        RedisConn& operator=(const RedisConn&) = delete;

        Slot& pickSlot() {
            auto idx = robin_.fetch_add(1, std::memory_order_relaxed) % POOL_SIZE;
            return pool_[idx];
        }

        static void closeSlot(Slot& s) {
            if (s.ctx) { redisFree(s.ctx); s.ctx = nullptr; }
        }

        void ensureConnected(Slot& slot) {
            if (slot.ctx && slot.ctx->err == 0) return;
            closeSlot(slot);

            auto cfg = loadRedisConfig();
            if (!cfg.enabled) return;

            auto now = std::chrono::steady_clock::now();
            if (now < slot.nextRetryAt) return;

            timeval tv; tv.tv_sec = 0; tv.tv_usec = 200 * 1000;
            slot.ctx = redisConnectWithTimeout(cfg.host.c_str(), cfg.port, tv);
            if (!slot.ctx || slot.ctx->err) {
                slot.nextRetryAt = now + std::chrono::seconds(2);
                closeSlot(slot);
                return;
            }
            redisSetTimeout(slot.ctx, tv);

            if (!cfg.password.empty()) {
                auto *r = (redisReply *)redisCommand(slot.ctx, "AUTH %s", cfg.password.c_str());
                if (!r || r->type == REDIS_REPLY_ERROR) {
                    if (r) freeReplyObject(r);
                    slot.nextRetryAt = now + std::chrono::seconds(2);
                    closeSlot(slot); return;
                }
                freeReplyObject(r);
            }
            if (cfg.db != 0) {
                auto *r = (redisReply *)redisCommand(slot.ctx, "SELECT %d", cfg.db);
                if (!r || r->type == REDIS_REPLY_ERROR) {
                    if (r) freeReplyObject(r);
                    slot.nextRetryAt = now + std::chrono::seconds(2);
                    closeSlot(slot); return;
                }
                freeReplyObject(r);
            }
        }

        Slot pool_[POOL_SIZE];
        std::atomic<unsigned> robin_{0};
        std::atomic<int> lastAvail_{-1};
        std::atomic<bool> needSync_{false};

    public:
        // Redis 恢复后是否需要回写内存数据
        bool consumeNeedSync() { return needSync_.exchange(false); }
    };

    inline bool redisSetEx(const std::string &key, const std::string &val, int expireSeconds) {
        auto &rc = RedisConn::instance();
        const auto k = rc.prefixKey(key);
        redisReply *r = nullptr;
        if (expireSeconds > 0)
            r = rc.command("SETEX %s %d %b", k.c_str(), expireSeconds, val.data(), (size_t)val.size());
        else
            r = rc.command("SET %s %b", k.c_str(), val.data(), (size_t)val.size());
        if (!r) {
            rc.markBad();
            ELOG_ERROR("Redis", "SETEX failed (null reply), key=" + key);
            return false;
        }
        bool ok = (r->type != REDIS_REPLY_ERROR);
        if (!ok) ELOG_ERROR("Redis", std::string("SETEX error: ")
                            + (r->str ? r->str : "unknown") + " key=" + key);
        freeReplyObject(r);
        return ok;
    }

    inline std::optional<std::string> redisGet(const std::string &key) {
        auto &rc = RedisConn::instance();
        const auto k = rc.prefixKey(key);

        auto *r = rc.command("GET %s", k.c_str());
        if (!r) { rc.markBad(); return std::nullopt; }
        if (r->type == REDIS_REPLY_NIL) { freeReplyObject(r); return std::nullopt; }
        if (r->type != REDIS_REPLY_STRING) { freeReplyObject(r); return std::nullopt; }
        std::string out(r->str, (size_t)r->len);
        freeReplyObject(r);
        return out;
    }

    inline void redisDel(const std::string &key) {
        auto &rc = RedisConn::instance();
        const auto k = rc.prefixKey(key);
        auto *r = rc.command("DEL %s", k.c_str());
        if (!r) { rc.markBad(); return; }
        freeReplyObject(r);
    }

    inline std::vector<std::string> redisKeysByPrefix(const std::string &prefix) {
        std::vector<std::string> out;
        auto &rc = RedisConn::instance();

        // Note: KEYS is fine for small deployments; for large scale use SCAN.
        const auto pfx = rc.prefixKey(prefix);
        std::string pattern = pfx + "*";
        auto *r = rc.command("KEYS %s", pattern.c_str());
        if (!r) { rc.markBad(); return out; }
        if (r->type != REDIS_REPLY_ARRAY) { freeReplyObject(r); return out; }
        auto cfg = loadRedisConfig();
        for (size_t i = 0; i < r->elements; ++i) {
            auto *e = r->element[i];
            if (!e || e->type != REDIS_REPLY_STRING) continue;
            std::string k(e->str, (size_t)e->len);
            if (!cfg.keyPrefix.empty() && k.rfind(cfg.keyPrefix, 0) == 0)
                k = k.substr(cfg.keyPrefix.size());
            out.push_back(k);
        }
        freeReplyObject(r);
        return out;
    }
}

    // Token 缓存（内存）
    // Token 缓存（内存）
class TokenCache {
public:
    static TokenCache &instance() {
        static TokenCache inst;
        return inst;
    }

    static std::string backendInfo() {
        auto &rc = RedisConn::instance();
        if (!rc.enabledByConfig()) {
            if (VramCache::instance().available())
                return "vram+memory";
            return "memory";
        }
        if (rc.available()) {
            auto cfg = loadRedisConfig();
            return "redis(" + cfg.host + ":" + std::to_string(cfg.port) + "/" + std::to_string(cfg.db) + ")";
        }
        if (VramCache::instance().available())
            return "vram(fallback)";
        return "memory(fallback)";
    }

    void set(const std::string &key, const LoginUser &user, int expireMinutes = 30) {
        auto expireAt = std::chrono::steady_clock::now() + std::chrono::minutes(expireMinutes);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            store_[key] = {user.toJson(), expireAt};
        }
        Json::FastWriter w;
        std::string serialized = w.write(user.toJson());
        if (RedisConn::instance().available()) {
            redisSetEx(key, serialized, expireMinutes * 60);
        } else if (VramCache::instance().available()) {
            VramCache::instance().setString(key, serialized, expireMinutes * 60);
        }
    }

    std::optional<LoginUser> get(const std::string &key) {
        auto parseJson = [](const std::string &s) -> std::optional<LoginUser> {
            Json::Value v; Json::Reader r;
            if (!r.parse(s, v)) return std::nullopt;
            return LoginUser::fromJson(v);
        };
        if (RedisConn::instance().available()) {
            // Redis 恢复时回写内存中的 token
            if (RedisConn::instance().consumeNeedSync()) syncToRedis();
            auto s = redisGet(key);
            if (s) return parseJson(*s);
        } else if (VramCache::instance().available()) {
            auto s = VramCache::instance().getString(key);
            if (s) return parseJson(*s);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it == store_.end()) return std::nullopt;
        if (std::chrono::steady_clock::now() > it->second.expireAt) {
            store_.erase(it);
            return std::nullopt;
        }
        return LoginUser::fromJson(it->second.json);
    }

    void remove(const std::string &key) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            store_.erase(key);
        }
        if (RedisConn::instance().available()) redisDel(key);
        VramCache::instance().remove(key);
    }

    void refresh(const std::string &key, int expireMinutes = 30) {
        Json::Value snap;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = store_.find(key);
            if (it != store_.end()) {
                it->second.expireAt = std::chrono::steady_clock::now() + std::chrono::minutes(expireMinutes);
                snap = it->second.json;
            }
        }
        if (!snap.isNull()) {
            Json::FastWriter w;
            std::string s = w.write(snap);
            if (RedisConn::instance().available())
                redisSetEx(key, s, expireMinutes * 60);
            else if (VramCache::instance().available())
                VramCache::instance().setString(key, s, expireMinutes * 60);
        }
    }

    void update(const std::string &key, const LoginUser &user) {
        int expireMinutes = drogon::app().getCustomConfig()["jwt"].get("expire_minutes", 30).asInt();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = store_.find(key);
            if (it != store_.end()) {
                it->second.json     = user.toJson();
                it->second.expireAt = std::chrono::steady_clock::now()
                                      + std::chrono::minutes(expireMinutes);
            }
        }
        Json::FastWriter w;
        std::string s = w.write(user.toJson());
        if (RedisConn::instance().available())
            redisSetEx(key, s, expireMinutes * 60);
        else if (VramCache::instance().available())
            VramCache::instance().setString(key, s, expireMinutes * 60);
    }

    // Token 缓存（内存）
    std::vector<LoginUser> getAll() {
        if (RedisConn::instance().available()) {
            std::vector<LoginUser> result;
            auto keys = redisKeysByPrefix(Constants::LOGIN_TOKEN_KEY);
            for (auto &k : keys) {
                auto u = get(k);
                if (u) result.push_back(*u);
            }
            return result;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<LoginUser> result;
        auto now = std::chrono::steady_clock::now();
        for (auto it = store_.begin(); it != store_.end(); ) {
            if (now > it->second.expireAt) {
                it = store_.erase(it);
            } else {
                result.push_back(LoginUser::fromJson(it->second.json));
                ++it;
            }
        }
        return result;
    }

    size_t size() {
        if (RedisConn::instance().available()) {
            return redisKeysByPrefix(Constants::LOGIN_TOKEN_KEY).size();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        return store_.size();
    }

private:
    void syncToRedis() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        Json::FastWriter w;
        int synced = 0;
        for (auto it = store_.begin(); it != store_.end(); ) {
            if (now > it->second.expireAt) {
                it = store_.erase(it);
                continue;
            }
            auto remainSec = (int)std::chrono::duration_cast<std::chrono::seconds>(
                it->second.expireAt - now).count();
            if (remainSec > 0) {
                redisSetEx(it->first, w.write(it->second.json), remainSec);
                ++synced;
            }
            ++it;
        }
        if (synced > 0)
            std::cout << "[TokenCache] synced " << synced << " tokens to Redis" << std::endl;
    }

    struct Entry {
        Json::Value json;
        std::chrono::steady_clock::time_point expireAt;
    };
    std::unordered_map<std::string, Entry> store_;
    std::mutex mutex_;
};

    // Token 缓存（内存）
class MemCache {
public:
    static MemCache &instance() {
        static MemCache inst;
        return inst;
    }

    static std::string backendInfo() {
        return TokenCache::backendInfo();
    }

    void setKeyPrefix(const std::string &prefix) {
        keyPrefix_ = prefix.empty() ? "" : prefix + ":";
    }
    const std::string &keyPrefix() const { return keyPrefix_; }

    void setString(const std::string &key, const std::string &val, int expireSeconds = 0) {
        auto pk = k(key);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::chrono::steady_clock::time_point expireAt;
            bool hasExpire = expireSeconds > 0;
            if (hasExpire)
                expireAt = std::chrono::steady_clock::now() + std::chrono::seconds(expireSeconds);
            store_[pk] = {val, expireAt, hasExpire};
        }
        if (RedisConn::instance().available()) {
            redisSetEx(pk, val, expireSeconds);
        } else if (VramCache::instance().available()) {
            VramCache::instance().setString(pk, val, expireSeconds);
        }
    }

    std::optional<std::string> getString(const std::string &key) {
        auto pk = k(key);
        if (RedisConn::instance().available()) {
            auto s = redisGet(pk);
            if (s) return s;
            // Redis 可用但 key 不存在（半死状态写失败），fallback 到内存
        } else if (VramCache::instance().available()) {
            auto s = VramCache::instance().getString(pk);
            if (s) return s;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(pk);
        if (it == store_.end()) return std::nullopt;
        if (it->second.hasExpire && std::chrono::steady_clock::now() > it->second.expireAt) {
            store_.erase(it);
            return std::nullopt;
        }
        return it->second.val;
    }

    void remove(const std::string &key) {
        auto pk = k(key);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            store_.erase(pk);
        }
        if (RedisConn::instance().available()) redisDel(pk);
        VramCache::instance().remove(pk);
    }

    void removeByPrefix(const std::string &prefix) {
        auto pp = k(prefix);
        if (RedisConn::instance().available()) {
            auto keys = redisKeysByPrefix(pp);
            for (auto &rk : keys) redisDel(rk);
        } else if (VramCache::instance().available()) {
            VramCache::instance().removeByPrefix(pp);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = store_.begin(); it != store_.end(); ) {
            if (it->first.rfind(pp, 0) == 0)
                it = store_.erase(it);
            else
                ++it;
        }
    }

    std::vector<std::string> getKeysByPrefix(const std::string &prefix) {
        auto pp = k(prefix);
        if (RedisConn::instance().available())
            return redisKeysByPrefix(pp);
        if (VramCache::instance().available())
            return VramCache::instance().getKeysByPrefix(pp);
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (auto &kv : store_) {
            if (kv.first.rfind(pp, 0) == 0)
                result.push_back(kv.first);
        }
        return result;
    }

    size_t size() {
        if (RedisConn::instance().available())
            return redisKeysByPrefix(keyPrefix_).size();
        if (VramCache::instance().available())
            return VramCache::instance().size();
        std::lock_guard<std::mutex> lock(mutex_);
        return store_.size();
    }

    void clear() {
        if (RedisConn::instance().available()) {
            auto keys = redisKeysByPrefix(keyPrefix_);
            for (auto &k : keys) redisDel(k);
        }
        VramCache::instance().clear();
        std::lock_guard<std::mutex> lock(mutex_);
        store_.clear();
    }

    void setJson(const std::string &key, const Json::Value &val, int expireSeconds = 0) {
        Json::FastWriter w;
        setString(key, w.write(val), expireSeconds);
    }

    std::optional<Json::Value> getJson(const std::string &key) {
        auto s = getString(key);
        if (!s) return std::nullopt;
        Json::Value v;
        Json::Reader r;
        if (!r.parse(*s, v)) return std::nullopt;
        return v;
    }

private:
    MemCache() {
        auto cfg = loadRedisConfig();
        if (!cfg.keyPrefix.empty())
            keyPrefix_ = cfg.keyPrefix + ":";
    }

    std::string k(const std::string &key) const { return keyPrefix_ + key; }

    struct Entry {
        std::string val;
        std::chrono::steady_clock::time_point expireAt;
        bool hasExpire = false;
    };
    std::unordered_map<std::string, Entry> store_;
    std::mutex mutex_;
    std::string keyPrefix_;  // 如 "ruoyicpp:"
};
