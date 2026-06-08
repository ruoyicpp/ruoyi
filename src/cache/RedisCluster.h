#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Cache {

struct RedisNode {
    std::string host;
    int port = 6379;
    std::string password;
    int db = 0;
    bool master = true;
    std::string nodeId;
};

struct ClusterConfig {
    bool enabled = false;
    std::vector<RedisNode> nodes;
    int connectionTimeoutMs = 5000;
    int commandTimeoutMs = 3000;
    int maxConnectionsPerNode = 8;
    int minIdleConnections = 2;
    int maxQueueSize = 1024;
    bool autoReconnect = true;
    int reconnectIntervalSeconds = 3;
};

class RedisCluster {
public:
    static RedisCluster& instance();

    void init(const ClusterConfig& config);
    void shutdown();

    bool isConnected() const { return connected_.load(); }
    int nodeCount() const;

    std::string get(const std::string& key);
    bool set(const std::string& key, const std::string& value, int ttlSeconds = 0);
    bool del(const std::string& key);
    bool exists(const std::string& key);

    std::vector<std::string> mget(const std::vector<std::string>& keys);
    bool mset(const std::map<std::string, std::string>& items, int ttlSeconds = 0);
    bool mdel(const std::vector<std::string>& keys);

    bool setExpire(const std::string& key, int ttlSeconds);
    long long ttl(const std::string& key);

    std::vector<std::string> scan(const std::string& pattern, int count = 100);
    long long delByPattern(const std::string& pattern);

    std::string ping();

    std::map<std::string, RedisNode> getNodes() const;

    std::string executeRaw(const std::string& command);

private:
    RedisCluster() = default;
    ~RedisCluster() = default;
    RedisCluster(const RedisCluster&) = delete;
    RedisCluster& operator=(const RedisCluster&) = delete;

    std::string routeToNode(const std::string& key);
    std::string connectNode(const RedisNode& node);
    void discoverCluster();

    mutable std::mutex mutex_;
    ClusterConfig config_;
    std::map<std::string, RedisNode> nodes_;
    std::atomic<bool> connected_{false};
    std::map<std::string, std::string> localStore_;
    mutable std::mutex storeMutex_;
};

} // namespace Cache
