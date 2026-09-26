/**
 * @file ClusterSession.h
 * @brief 集群会话同步 — 跨节点在线用户注册表（Redis SET + 心跳续期）
 *
 * 功能概述：
 *   - 登录态同步：TokenCache 已支持 Redis 后端（redis.enabled=true 时自动共享），
 *     本模块补齐"在线用户"维度：哪个用户在哪个节点在线
 *   - 注册表：cluster:online:<nodeId> = SET(userId...)，EX 120s 心跳续期
 *   - 节点崩溃：key 自动过期，不残留幽灵在线状态
 *   - 跨节点查询：KEYS cluster:online:* → SMEMBERS 合并
 *   - 降级：Redis 不可用时仅本节点内存视图（WsNotifyCtrl::onlineUsers）
 *
 * 使用示例：
 *   // WS 连接建立时（WsNotifyCtrl::handleNewConnection）
 *   ClusterSession::instance().online(userId);
 *   // WS 断开时（handleConnectionClosed）
 *   ClusterSession::instance().offline(userId);
 *   // 定时器每 60s 心跳
 *   ClusterSession::instance().heartbeat();
 *   // 查询集群在线用户
 *   auto users = ClusterSession::instance().clusterOnlineUsers();
 *
 * 配置项（config.json → cluster）：
 *   - node_id: 节点标识（默认自动 = hostname:port）
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <cstdlib>
#include <ctime>
#ifdef _WIN32
#  include <winsock2.h>   // gethostname
#  include <process.h>   // _getpid
#  define cluster_getpid _getpid
#else
#  include <unistd.h>    // gethostname, getpid
#  define cluster_getpid ::getpid
#endif
#include <hiredis/hiredis.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "TokenCache.h"   // RedisConn

/**
 * @class ClusterSession
 * @brief 集群在线用户注册表单例
 */
class ClusterSession {
public:
    static ClusterSession& instance() {
        static ClusterSession inst;
        return inst;
    }

    /// 初始化节点标识（hostname:port 或显式 node_id）
    void init(const std::string& nodeId = "") {
        if (!nodeId.empty()) { nodeId_ = nodeId; }
        else {
            char host[128] = {0};
            gethostname(host, sizeof(host) - 1);
            nodeId_ = std::string(host) + ":" + std::to_string(cluster_getpid());
        }
        LOG_INFO << "[Cluster] node_id=" << nodeId_;
    }

    const std::string& nodeId() const { return nodeId_; }

    /// 用户上线（WS 连接建立时调用）
    void online(long userId) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            local_.insert(userId);
        }
        auto& rc = RedisConn::instance();
        if (!rc.available()) return;
        std::string rk = rc.prefixKey("cluster:online:" + nodeId_);
        auto* r = rc.command("SADD %s %ld", rk.c_str(), userId);
        if (r) freeReplyObject(r); else { rc.markBad(); return; }
        rc.command("EXPIRE %s 120", rk.c_str());   // 心跳兜底 TTL
    }

    /// 用户下线（WS 断开时调用）
    void offline(long userId) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            local_.erase(userId);
        }
        auto& rc = RedisConn::instance();
        if (!rc.available()) return;
        std::string rk = rc.prefixKey("cluster:online:" + nodeId_);
        auto* r = rc.command("SREM %s %ld", rk.c_str(), userId);
        if (r) freeReplyObject(r); else rc.markBad();
    }

    /// 心跳：刷新本节点在线集合 TTL（每 60s 调用一次）
    void heartbeat() {
        auto& rc = RedisConn::instance();
        if (!rc.available()) return;
        std::lock_guard<std::mutex> lk(mu_);
        if (local_.empty()) return;
        std::string rk = rc.prefixKey("cluster:online:" + nodeId_);
        auto* r = rc.command("EXPIRE %s 120", rk.c_str());
        if (r) freeReplyObject(r); else rc.markBad();
    }

    /// 集群在线用户（跨节点合并；Redis 不可用时返回本节点视图）
    std::vector<long> clusterOnlineUsers() {
        auto& rc = RedisConn::instance();
        if (!rc.available()) return localUsers();
        std::string pattern = rc.prefixKey("cluster:online:*");
        auto* kr = rc.command("KEYS %s", pattern.c_str());
        if (!kr) { rc.markBad(); return localUsers(); }
        std::unordered_set<long> all;
        if (kr->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < kr->elements; ++i) {
                auto* sr = rc.command("SMEMBERS %s", kr->element[i]->str);
                if (sr && sr->type == REDIS_REPLY_ARRAY)
                    for (size_t j = 0; j < sr->elements; ++j)
                        all.insert(std::atol(sr->element[j]->str));
                if (sr) freeReplyObject(sr);
            }
        }
        freeReplyObject(kr);
        return std::vector<long>(all.begin(), all.end());
    }

    /// 本节点在线用户
    std::vector<long> localUsers() {
        std::lock_guard<std::mutex> lk(mu_);
        return std::vector<long>(local_.begin(), local_.end());
    }

    /// 集群节点列表（有在线用户的节点）
    std::vector<std::string> clusterNodes() {
        auto& rc = RedisConn::instance();
        std::vector<std::string> out;
        if (!rc.available()) return out;
        std::string pattern = rc.prefixKey("cluster:online:*");
        auto* kr = rc.command("KEYS %s", pattern.c_str());
        if (!kr) { rc.markBad(); return out; }
        if (kr->type == REDIS_REPLY_ARRAY)
            for (size_t i = 0; i < kr->elements; ++i)
                out.emplace_back(kr->element[i]->str);
        freeReplyObject(kr);
        return out;
    }

private:
    ClusterSession() = default;
    std::string nodeId_;
    std::unordered_set<long> local_;
    std::mutex mu_;
};
