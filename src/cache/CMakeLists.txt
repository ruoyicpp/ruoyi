# Cache Strategy Module
# 分布式缓存策略模块

# 源文件
set(CACHE_SOURCES
    CacheStrategy.cc
    RedisCluster.cc
    CacheWarmup.cc
    CacheInvalidation.cc
    CacheCtrl.cc
)

# 头文件
set(CACHE_HEADERS
    CacheStrategy.h
    RedisCluster.h
    CacheWarmup.h
    CacheInvalidation.h
    CacheCtrl.h
)

# 添加到父项目的源文件列表
