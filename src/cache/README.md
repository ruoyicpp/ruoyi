# 分布式缓存策略 (Distributed Cache Strategy)

## 📋 模块概述

分布式缓存策略模块提供企业级的缓存解决方案，支持 Redis Cluster、多层缓存、缓存预热、缓存失效策略等，可以有效提升系统性能和可靠性。

## 🎯 核心功能

### 1. Redis Cluster 支持
- Redis Cluster 连接管理
- 自动故障转移
- 集群拓扑发现
- 集群扩容和缩容

### 2. 多层缓存架构
- L1：本地内存缓存
- L2：Redis Cluster 抽象层
- 自动回填和降级

### 3. 缓存穿透防护
- 空值缓存
- 布隆过滤器
- 缓存预热

### 4. 缓存击穿防护
- 互斥锁机制
- 热点数据预热
- 缓存更新策略

### 5. 缓存雪崩防护
- 随机过期时间
- 多层缓存保护
- 自动降级机制

### 6. 缓存预热
- 启动时预热
- 定时预热
- 智能预热

### 7. 缓存失效策略
- 主动失效
- 被动失效
- 延迟失效

## 📁 文件结构

```
src/cache/
├── CacheStrategy.h            - 缓存策略接口
├── CacheStrategy.cc           - 实现代码
├── RedisCluster.h             - Redis Cluster 支持
├── RedisCluster.cc            - 实现代码
├── CacheWarmup.h              - 缓存预热
├── CacheWarmup.cc             - 实现代码
├── CacheInvalidation.h        - 缓存失效策略
├── CacheInvalidation.cc       - 实现代码
├── CacheCtrl.h                - 缓存管理 API
├── CMakeLists.txt             - 编译配置
└── README.md                  - 本文件
```

## 🚀 快速开始

### 1. 配置缓存系统

```json
{
  "cache": {
    "enabled": true,
    "redis_cluster": {
      "enabled": true,
      "nodes": [
        {"host": "localhost", "port": 7000},
        {"host": "localhost", "port": 7001},
        {"host": "localhost", "port": 7002}
      ],
      "password": "",
      "db": 0
    },
    "warmup": {
      "enabled": true,
      "on_startup": true,
      "interval": 3600
    },
    "invalidation": {
      "strategy": "active",
      "ttl": 3600
    }
  }
}
```

### 2. 初始化缓存系统

```cpp
#include "cache/CacheStrategy.h"

Cache::CacheConfig cacheConfig;
cacheConfig.enabled = true;
cacheConfig.redisTtlSeconds = 3600;

// 初始化
Cache::CacheStrategy::instance().init(cacheConfig);

// 启动缓存预热
Cache::CacheStrategy::instance().warmup();
```

### 3. 使用缓存

```cpp
// 获取缓存
auto value = Cache::CacheStrategy::instance().get("key");

// 设置缓存
Cache::CacheStrategy::instance().set("key", std::string("value"), 3600);

// 删除缓存
Cache::CacheStrategy::instance().remove("key");

// 批量删除
Cache::CacheStrategy::instance().removeByPattern("prefix:*");
```

## 📊 API 端点

```
GET  /monitor/cache/stats              - 获取缓存统计
GET  /monitor/cache/keys               - 获取缓存键列表
GET  /monitor/cache/key/{key}          - 获取缓存值

POST /monitor/cache/warmup             - 执行缓存预热
POST /monitor/cache/clear              - 清空缓存
DELETE /monitor/cache/key/{key}        - 删除缓存键

GET  /monitor/cache/cluster/nodes      - 获取集群节点
GET  /monitor/cache/cluster/slots      - 获取集群槽位
```

## 🔧 缓存穿透防护

### 空值缓存

```cpp
// 缓存空值，防止频繁查询不存在的数据
auto entry = Cache::CacheStrategy::instance().getOrSet("user:999", [&]() -> Cache::CacheValue {
    auto user = db.queryUser(999);
    if (!user) {
        return std::monostate{};
    }
    Json::Value jsonUser;
    jsonUser["id"] = user->id;
    return jsonUser;
}, 3600);
```

### 布隆过滤器

```cpp
// 当前版本未内建 Bloom Filter，建议先按业务键前缀或主存储存在性做前置判断
if (!db.mayExist("user:999")) {
    return nullptr;
}
```

## 🔧 缓存击穿防护

### 互斥锁机制

```cpp
// 使用互斥锁防止热点数据缓存失效时的并发查询
auto entry = Cache::CacheStrategy::instance().getWithLock("hot_key", [&]() -> Cache::CacheValue {
    return std::string("hot-data");
}, 3600);
```

### 热点数据预热

```cpp
// 预热热点数据
Cache::CacheStrategy::instance().warmupHotKeys({
    "user:1", "user:2", "user:3",
    "product:100", "product:101"
});
```

## 🔧 缓存雪崩防护

### 随机过期时间

```cpp
// 设置随机过期时间，避免大量缓存同时失效
int ttl = 3600 + rand() % 600;  // 3600-4200 秒
Cache::CacheStrategy::instance().set("key", std::string("value"), ttl);
```

### 多层缓存

```cpp
// 当前实现使用两层缓存
// L1: 本地内存
// L2: Redis Cluster 抽象层
auto value = Cache::CacheStrategy::instance().get("key");
```

## 💡 最佳实践

1. **缓存键设计**
   - 使用有意义的前缀
   - 避免键冲突
   - 便于监控和管理

2. **缓存值设计**
   - 序列化格式统一
   - 避免过大的值
   - 考虑压缩

3. **缓存过期策略**
   - 合理设置 TTL
   - 避免热点数据过期
   - 定期更新缓存

4. **缓存监控**
   - 监控缓存命中率
   - 监控缓存大小
   - 监控缓存延迟

5. **缓存预热**
   - 启动时预热热点数据
   - 定期更新预热数据
   - 智能预热策略

## 🔗 相关模块

- [TaskQueue](../taskqueue/) - 异步任务队列（缓存预热任务）
- [Alert](../alert/) - 性能告警系统（缓存告警）
- [Log](../log/) - 日志聚合分析（缓存日志）

## 📚 参考资源

- [Redis 官方文档](https://redis.io/documentation)
- [Redis Cluster 指南](https://redis.io/topics/cluster-tutorial)
- [缓存设计模式](docs/cache-patterns.md)
- [性能优化指南](docs/performance-tuning.md)

