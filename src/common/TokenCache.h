#pragma once
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
            // drogon::app().getCustomConfig() ֻ���� custom_config �Σ�
            // ���ǵ� redis ������ config.json ���㣬�������ֱ�Ӷ�ȡ�ļ���
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
        } catch (...) {
            // ignore config errors and fall back to memory cache
        }
        return cfg;
    }

    class RedisConn {
    public:
        static RedisConn &instance() {
            static RedisConn inst;
            return inst;
        }

        bool enabledByConfig() {
            auto cfg = loadRedisConfig();
            return cfg.enabled;
        }

        // True only when config enables redis AND a connection is available.
        // If redis is down, we fall back to memory cache.
        bool available() {
            if (!enabledByConfig()) return false;
            bool ok = ctx() != nullptr;
            int cur = ok ? 1 : 0;
            int prev = lastAvail_.exchange(cur);
            if (prev != -1 && prev != cur) {
                if (ok) {
                    std::cout << "[Cache] Redis is available, switch to redis" << std::endl;
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

        redisContext *ctx() {
            std::lock_guard<std::mutex> lock(mutex_);
            ensureConnectedLocked();
            return ctx_;
        }

        // Best-effort reconnect on error.
        void markBad() {
            std::lock_guard<std::mutex> lock(mutex_);
            closeLocked();
        }

    private:
        RedisConn() = default;
        ~RedisConn() { closeLocked(); }
        RedisConn(const RedisConn&) = delete;
        RedisConn& operator=(const RedisConn&) = delete;

        void closeLocked() {
            if (ctx_) {
                redisFree(ctx_);
                ctx_ = nullptr;
            }
        }

        void ensureConnectedLocked() {
            if (ctx_ && ctx_->err == 0) return;
            closeLocked();

            auto cfg = loadRedisConfig();
            if (!cfg.enabled) return;

            // Redis ������ʱ������ÿ�λ�����ʶ���������
            auto now = std::chrono::steady_clock::now();
            if (now < nextRetryAt_) return;

            // ���ӳ�ʱ����ֹ Redis ֹͣʱ������
            timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 200 * 1000; // 200ms
            ctx_ = redisConnectWithTimeout(cfg.host.c_str(), cfg.port, tv);
            if (!ctx_ || ctx_->err) {
                nextRetryAt_ = now + std::chrono::seconds(2);
                closeLocked();
                return;
            }

            // �����д��ʱ����ֹ redisCommand ������
            redisSetTimeout(ctx_, tv);

            if (!cfg.password.empty()) {
                auto *r = (redisReply *)redisCommand(ctx_, "AUTH %s", cfg.password.c_str());
                if (!r || r->type == REDIS_REPLY_ERROR) {
                    if (r) freeReplyObject(r);
                    nextRetryAt_ = now + std::chrono::seconds(2);
                    closeLocked();
                    return;
                }
                freeReplyObject(r);
            }

            if (cfg.db != 0) {
                auto *r = (redisReply *)redisCommand(ctx_, "SELECT %d", cfg.db);
                if (!r || r->type == REDIS_REPLY_ERROR) {
                    if (r) freeReplyObject(r);
                    nextRetryAt_ = now + std::chrono::seconds(2);
                    closeLocked();
                    return;
                }
                freeReplyObject(r);
            }
        }

        redisContext *ctx_ = nullptr;
        std::mutex mutex_;
        std::atomic<int> lastAvail_{-1};
        std::chrono::steady_clock::time_point nextRetryAt_{};
    };

    inline bool redisSetEx(const std::string &key, const std::string &val, int expireSeconds) {
        auto &rc = RedisConn::instance();
        auto *c = rc.ctx();
        if (!c) return false;
        const auto k = rc.prefixKey(key);

        redisReply *r = nullptr;
        if (expireSeconds > 0) {
            r = (redisReply *)redisCommand(c, "SETEX %s %d %b", k.c_str(), expireSeconds, val.data(), (size_t)val.size());
        } else {
            r = (redisReply *)redisCommand(c, "SET %s %b", k.c_str(), val.data(), (size_t)val.size());
        }
        if (!r) { rc.markBad(); return false; }
        bool ok = (r->type == REDIS_REPLY_STATUS);
        freeReplyObject(r);
        return ok;
    }

    inline std::optional<std::string> redisGet(const std::string &key) {
        auto &rc = RedisConn::instance();
        auto *c = rc.ctx();
        if (!c) return std::nullopt;
        const auto k = rc.prefixKey(key);

        auto *r = (redisReply *)redisCommand(c, "GET %s", k.c_str());
        if (!r) { rc.markBad(); return std::nullopt; }
        if (r->type == REDIS_REPLY_NIL) { freeReplyObject(r); return std::nullopt; }
        if (r->type != REDIS_REPLY_STRING) { freeReplyObject(r); return std::nullopt; }
        std::string out(r->str, (size_t)r->len);
        freeReplyObject(r);
        return out;
    }

    inline void redisDel(const std::string &key) {
        auto &rc = RedisConn::instance();
        auto *c = rc.ctx();
        if (!c) return;
        const auto k = rc.prefixKey(key);
        auto *r = (redisReply *)redisCommand(c, "DEL %s", k.c_str());
        if (!r) { rc.markBad(); return; }
        freeReplyObject(r);
    }

    inline std::vector<std::string> redisKeysByPrefix(const std::string &prefix) {
        std::vector<std::string> out;
        auto &rc = RedisConn::instance();
        auto *c = rc.ctx();
        if (!c) return out;

        // Note: KEYS is fine for small deployments; for large scale use SCAN.
        const auto pfx = rc.prefixKey(prefix);
        std::string pattern = pfx + "*";
        auto *r = (redisReply *)redisCommand(c, "KEYS %s", pattern.c_str());
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

// �ڴ� Token ���棨��� Redis��
// �洢 uuid -> LoginUser JSON��������ʱ��
class TokenCache {
public:
    static TokenCache &instance() {
        static TokenCache inst;
        return inst;
    }

    static std::string backendInfo() {
        auto &rc = RedisConn::instance();
        if (!rc.enabledByConfig()) return "memory";
        if (rc.available()) {
            auto cfg = loadRedisConfig();
            return "redis(" + cfg.host + ":" + std::to_string(cfg.port) + "/" + std::to_string(cfg.db) + ")";
        }
        return "memory(fallback)";
    }

    void set(const std::string &key, const LoginUser &user, int expireMinutes = 30) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto expireAt = std::chrono::steady_clock::now() + std::chrono::minutes(expireMinutes);
        store_[key] = {user.toJson(), expireAt};

        if (RedisConn::instance().available()) {
            Json::FastWriter w;
            redisSetEx(key, w.write(user.toJson()), expireMinutes * 60);
        }
    }

    std::optional<LoginUser> get(const std::string &key) {
        if (RedisConn::instance().available()) {
            auto s = redisGet(key);
            if (s) {
                Json::Value v;
                Json::Reader r;
                if (!r.parse(*s, v)) return std::nullopt;
                return LoginUser::fromJson(v);
            }
            // Redis ����ʧ��ʱ���������ڴ棨֧�� Redis ��ʱ������/�����
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it == store_.end()) return std::nullopt;
        if (std::chrono::steady_clock::now() > it->second.expireAt) {
            store_.erase(it);
            return std::nullopt;
        }
        // ��� Redis ������ Redis δ���У����ڴ���� Redis������ʣ�� TTL��
        if (RedisConn::instance().available()) {
            auto now = std::chrono::steady_clock::now();
            auto remain = std::chrono::duration_cast<std::chrono::seconds>(it->second.expireAt - now).count();
            if (remain > 0) {
                Json::FastWriter w;
                redisSetEx(key, w.write(it->second.json), (int)remain);
            }
        }
        return LoginUser::fromJson(it->second.json);
    }

    void remove(const std::string &key) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            store_.erase(key);
        }
        if (RedisConn::instance().available()) redisDel(key);
    }

    void refresh(const std::string &key, int expireMinutes = 30) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it != store_.end()) {
            it->second.expireAt = std::chrono::steady_clock::now() + std::chrono::minutes(expireMinutes);
        }

        if (RedisConn::instance().available() && it != store_.end()) {
            Json::FastWriter w;
            redisSetEx(key, w.write(it->second.json), expireMinutes * 60);
        }
    }

    // ���»����е� LoginUser�������޸�Ȩ�޺�ˢ�£�
    void update(const std::string &key, const LoginUser &user) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it != store_.end()) {
            it->second.json = user.toJson();
        }

        if (RedisConn::instance().available()) {
            // Redis û������ʣ�� TTL�����ﰴ���� expire_minutes ����д�롣
            int expireMinutes = drogon::app().getCustomConfig()["jwt"].get("expire_minutes", 30).asInt();
            Json::FastWriter w;
            redisSetEx(key, w.write(user.toJson()), expireMinutes * 60);
        }
    }

    // ö������δ���ڵ������û�
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
    struct Entry {
        Json::Value json;
        std::chrono::steady_clock::time_point expireAt;
    };
    std::unordered_map<std::string, Entry> store_;
    std::mutex mutex_;
};

// ͨ�� KV �ڴ滺�棨������֤�롢�ֵ䡢���õȣ�
class MemCache {
public:
    static MemCache &instance() {
        static MemCache inst;
        return inst;
    }

    static std::string backendInfo() {
        return TokenCache::backendInfo();
    }

    void setString(const std::string &key, const std::string &val, int expireSeconds = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::chrono::steady_clock::time_point expireAt;
        bool hasExpire = expireSeconds > 0;
        if (hasExpire)
            expireAt = std::chrono::steady_clock::now() + std::chrono::seconds(expireSeconds);
        store_[key] = {val, expireAt, hasExpire};

        if (RedisConn::instance().available()) {
            redisSetEx(key, val, expireSeconds);
        }
    }

    std::optional<std::string> getString(const std::string &key) {
        if (RedisConn::instance().available()) {
            auto s = redisGet(key);
            if (s) return s;
            // Redis δ����ʱ�������ڴ�
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(key);
        if (it == store_.end()) return std::nullopt;
        if (it->second.hasExpire && std::chrono::steady_clock::now() > it->second.expireAt) {
            store_.erase(it);
            return std::nullopt;
        }
        // Redis ���ܸջָ�������ڴ������� Redis ���ぁ��� Redis
        if (RedisConn::instance().available()) {
            int remain = 0;
            if (it->second.hasExpire) {
                auto now = std::chrono::steady_clock::now();
                auto sec = std::chrono::duration_cast<std::chrono::seconds>(it->second.expireAt - now).count();
                if (sec > 0) remain = (int)sec;
            }
            redisSetEx(key, it->second.val, remain);
        }
        return it->second.val;
    }

    void remove(const std::string &key) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            store_.erase(key);
        }
        if (RedisConn::instance().available()) redisDel(key);
    }

    // ��ǰ׺ɾ��������ˢ������/�ֵ仺�棩
    void removeByPrefix(const std::string &prefix) {
        if (RedisConn::instance().available()) {
            auto keys = redisKeysByPrefix(prefix);
            for (auto &k : keys) redisDel(k);
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = store_.begin(); it != store_.end(); ) {
            if (it->first.rfind(prefix, 0) == 0)
                it = store_.erase(it);
            else
                ++it;
        }
    }

    // ��ǰ׺��ȡ���� key
    std::vector<std::string> getKeysByPrefix(const std::string &prefix) {
        if (RedisConn::instance().available()) {
            return redisKeysByPrefix(prefix);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (auto &kv : store_) {
            if (kv.first.rfind(prefix, 0) == 0)
                result.push_back(kv.first);
        }
        return result;
    }

    size_t size() {
        if (RedisConn::instance().available()) {
            return redisKeysByPrefix("").size();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        return store_.size();
    }

    void clear() {
        if (RedisConn::instance().available()) {
            auto keys = redisKeysByPrefix("");
            for (auto &k : keys) redisDel(k);
            return;
        }
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
    struct Entry {
        std::string val;
        std::chrono::steady_clock::time_point expireAt;
        bool hasExpire = false;
    };
    std::unordered_map<std::string, Entry> store_;
    std::mutex mutex_;
};
