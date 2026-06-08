#include "RedisCluster.h"

#include <functional>
#include <sstream>

namespace Cache {

RedisCluster& RedisCluster::instance() {
    static RedisCluster cluster;
    return cluster;
}

void RedisCluster::init(const ClusterConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    nodes_.clear();

    if (!config_.enabled) {
        connected_ = false;
        return;
    }

    for (const auto& node : config_.nodes) {
        const auto nodeKey = node.host + ":" + std::to_string(node.port);
        nodes_[nodeKey] = node;
        connectNode(node);
    }

    connected_ = !nodes_.empty();
    if (connected_.load()) {
        discoverCluster();
    }
}

void RedisCluster::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
    nodes_.clear();

    std::lock_guard<std::mutex> storeLock(storeMutex_);
    localStore_.clear();
}

std::string RedisCluster::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(storeMutex_);
    auto it = localStore_.find(key);
    return it != localStore_.end() ? it->second : std::string{};
}

bool RedisCluster::set(const std::string& key, const std::string& value, int ttlSeconds) {
    (void)ttlSeconds;
    std::lock_guard<std::mutex> lock(storeMutex_);
    localStore_[key] = value;
    return true;
}

bool RedisCluster::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(storeMutex_);
    return localStore_.erase(key) > 0;
}

bool RedisCluster::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(storeMutex_);
    return localStore_.find(key) != localStore_.end();
}

std::vector<std::string> RedisCluster::mget(const std::vector<std::string>& keys) {
    std::lock_guard<std::mutex> lock(storeMutex_);
    std::vector<std::string> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        auto it = localStore_.find(key);
        results.push_back(it != localStore_.end() ? it->second : std::string{});
    }
    return results;
}

bool RedisCluster::mset(const std::map<std::string, std::string>& items, int ttlSeconds) {
    (void)ttlSeconds;
    std::lock_guard<std::mutex> lock(storeMutex_);
    for (const auto& [key, value] : items) {
        localStore_[key] = value;
    }
    return true;
}

bool RedisCluster::mdel(const std::vector<std::string>& keys) {
    std::lock_guard<std::mutex> lock(storeMutex_);
    size_t removed = 0;
    for (const auto& key : keys) {
        removed += localStore_.erase(key);
    }
    return removed > 0;
}

bool RedisCluster::setExpire(const std::string& key, int ttlSeconds) {
    (void)ttlSeconds;
    return exists(key);
}

long long RedisCluster::ttl(const std::string& key) {
    (void)key;
    return -1;
}

std::vector<std::string> RedisCluster::scan(const std::string& pattern, int count) {
    const int limit = count > 0 ? count : 100;

    std::lock_guard<std::mutex> lock(storeMutex_);
    std::vector<std::string> results;

    const auto wildcardPos = pattern.find('*');
    if (wildcardPos == std::string::npos) {
        if (localStore_.find(pattern) != localStore_.end()) {
            results.push_back(pattern);
        }
        return results;
    }

    const auto prefix = pattern.substr(0, wildcardPos);
    for (const auto& [key, value] : localStore_) {
        (void)value;
        if (key.find(prefix) == 0) {
            results.push_back(key);
            if (static_cast<int>(results.size()) >= limit) {
                break;
            }
        }
    }
    return results;
}

long long RedisCluster::delByPattern(const std::string& pattern) {
    auto keys = scan(pattern);
    if (keys.empty()) {
        return 0;
    }
    mdel(keys);
    return static_cast<long long>(keys.size());
}

std::string RedisCluster::ping() {
    return connected_.load() ? "PONG" : "DISCONNECTED";
}

std::map<std::string, RedisNode> RedisCluster::getNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_;
}

int RedisCluster::nodeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(nodes_.size());
}

std::string RedisCluster::executeRaw(const std::string& command) {
    std::istringstream iss(command);
    std::string op;
    iss >> op;

    if (op == "GET" || op == "get") {
        std::string key;
        iss >> key;
        return get(key);
    }
    if (op == "SET" || op == "set") {
        std::string key;
        std::string value;
        iss >> key >> value;
        return set(key, value) ? "OK" : "ERR";
    }
    if (op == "DEL" || op == "del") {
        std::string key;
        iss >> key;
        return del(key) ? "1" : "0";
    }
    if (op == "EXISTS" || op == "exists") {
        std::string key;
        iss >> key;
        return exists(key) ? "1" : "0";
    }
    if (op == "PING" || op == "ping") {
        return ping();
    }
    return "(unknown command)";
}

std::string RedisCluster::routeToNode(const std::string& key) {
    if (nodes_.empty()) {
        return {};
    }

    size_t hash = std::hash<std::string>{}(key);
    auto it = nodes_.begin();
    std::advance(it, static_cast<long long>(hash % nodes_.size()));
    return it->first;
}

std::string RedisCluster::connectNode(const RedisNode& node) {
    (void)node;
    return "connected";
}

void RedisCluster::discoverCluster() {
}

} // namespace Cache
