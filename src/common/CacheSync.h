/**
 * @file CacheSync.h
 * @brief 缓存一致性 — Redis pub/sub 失效广播（多节点本地缓存同步）
 *
 * 功能概述：
 *   - 问题：MemCache 本地 store_ 是进程内缓存，A 节点 remove 后
 *     B 节点本地旧值要等 TTL 才过期 → "改了参数 A 生效、B 不生效"
 *   - 方案：MemCache::remove/removeByPrefix 触发 CacheSyncHook →
 *     PUBLISH ruoyi:cache:inv；各节点订阅线程收到后只清本地缓存
 *   - 防回声：订阅端调 removeLocal/removeLocalByPrefix（不再发广播）
 *   - 连接：SUBSCRIBE 阻塞，用独立 redisContext（不走 RedisConn 池）
 *   - 降级：Redis 不可用时自动重连（5s 退避），期间各节点退化为 TTL 过期
 *
 * 消息格式：
 *   - "K <prefixedKey>"  单键失效
 *   - "P <prefix>"       前缀失效
 *
 * 接线（main.cc，Redis 初始化后）：
 *   CacheSync::instance().start();
 */

#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <hiredis/hiredis.h>
#include <trantor/utils/Logger.h>
#include "TokenCache.h"   // MemCache / RedisConn / CacheSyncHook / loadRedisConfig

/**
 * @class CacheSync
 * @brief 缓存失效广播单例
 */
class CacheSync {
public:
    static constexpr const char* CHANNEL = "ruoyi:cache:inv";

    static CacheSync& instance() {
        static CacheSync inst;
        return inst;
    }

    /// 启动：绑定失效钩子 + 订阅线程（仅 Redis 启用时）
    void start() {
        auto cfg = loadRedisConfig();
        if (!cfg.enabled) {
            LOG_INFO << "[CacheSync] redis disabled, skip invalidation broadcast";
            return;
        }
        // 防重复启动：覆盖 joinable 线程会 std::terminate
        if (running_.exchange(true)) return;
        // 绑定 MemCache 失效钩子 → PUBLISH
        CacheSyncHook::fn = [](const std::string& pk, bool isPrefix) {
            publish(pk, isPrefix);
        };
        subThread_ = std::thread(&CacheSync::subscribeLoop, this, cfg);
        LOG_INFO << "[CacheSync] invalidation broadcast started, channel=" << CHANNEL;
    }

    void stop() {
        running_ = false;
        // 只 shutdown fd 解除订阅线程 redisGetReply 阻塞；
        // 不能直接 redisFree——订阅线程正在用，会 UAF。由线程内 closeSub 释放。
        // ctxMu_ 保护：防止 load 后线程已 free 导致读野指针
        {
            std::lock_guard<std::mutex> lk(ctxMu_);
            if (subCtx_) {
#ifdef _WIN32
                shutdown(subCtx_->fd, SD_BOTH);
#else
                shutdown(subCtx_->fd, SHUT_RDWR);
#endif
            }
        }
        if (subThread_.joinable()) subThread_.join();
    }

private:
    CacheSync() = default;
    ~CacheSync() { stop(); }

    /// 发布失效消息（走 RedisConn 池，线程安全）
    static void publish(const std::string& pk, bool isPrefix) {
        auto& rc = RedisConn::instance();
        if (!rc.available()) return;
        std::string msg = (isPrefix ? "P " : "K ") + pk;
        auto* r = rc.command("PUBLISH %s %s", CHANNEL, msg.c_str());
        if (r) freeReplyObject(r);
    }

    /// 订阅循环：SUBSCRIBE → redisGetReply → 清本地缓存
    void subscribeLoop(RedisConfig cfg) {
        while (running_) {
            auto* ctx = redisConnect(cfg.host.c_str(), cfg.port);
            {
                std::lock_guard<std::mutex> lk(ctxMu_);
                subCtx_ = ctx;
            }
            if (!ctx || ctx->err) {
                LOG_WARN << "[CacheSync] connect failed: "
                         << (ctx ? ctx->errstr : "oom");
                closeSub();
                sleep5();
                continue;
            }
            // AUTH + SELECT
            if (!cfg.password.empty()) {
                auto* r = (redisReply*)redisCommand(ctx, "AUTH %s", cfg.password.c_str());
                if (r) freeReplyObject(r);
            }
            if (cfg.db != 0) {
                auto* r = (redisReply*)redisCommand(ctx, "SELECT %d", cfg.db);
                if (r) freeReplyObject(r);
            }
            auto* sub = (redisReply*)redisCommand(ctx, "SUBSCRIBE %s", CHANNEL);
            if (sub) freeReplyObject(sub);
            LOG_INFO << "[CacheSync] subscribed " << CHANNEL;

            // 阻塞读消息
            redisReply* msg = nullptr;
            while (running_ && redisGetReply(ctx, (void**)&msg) == REDIS_OK) {
                if (msg && msg->type == REDIS_REPLY_ARRAY && msg->elements >= 3) {
                    auto* payload = msg->element[2];
                    if (payload && payload->type == REDIS_REPLY_STRING)
                        onInvalidate(std::string(payload->str, payload->len));
                }
                if (msg) { freeReplyObject(msg); msg = nullptr; }
            }
            if (msg) freeReplyObject(msg);   // 错误路径也可能已分配
            closeSub();
            if (running_) {
                LOG_WARN << "[CacheSync] subscription lost, reconnect in 5s";
                sleep5();
            }
        }
    }

    /// 收到失效消息 → 只清本地（不再广播，防回声）
    static void onInvalidate(const std::string& msg) {
        if (msg.size() < 3) return;
        bool isPrefix = (msg[0] == 'P');
        std::string pk = msg.substr(2);
        if (isPrefix) MemCache::instance().removeLocalByPrefix(pk);
        else          MemCache::instance().removeLocal(pk);
    }

    void closeSub() {
        redisContext* ctx;
        {
            std::lock_guard<std::mutex> lk(ctxMu_);
            ctx = subCtx_;
            subCtx_ = nullptr;
        }
        if (ctx) redisFree(ctx);
    }
    void sleep5() {
        for (int i = 0; i < 50 && running_; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::atomic<bool> running_{false};
    std::thread subThread_;
    std::mutex ctxMu_;                  ///< 保护 subCtx_（stop 读 fd / 线程 free）
    redisContext* subCtx_ = nullptr;
};
