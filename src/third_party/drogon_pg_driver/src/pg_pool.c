#include "pg_pool.h"
#include "pg_config.h"
#include "pg_error.h"
#include "pg_mutex.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* 创建连接池 */
pg_pool_t* pg_pool_create(const char *conn_str, int min_size, int max_size, int checkout_timeout_ms, int is_fallback) {
    pg_pool_t *pool = (pg_pool_t*)malloc(sizeof(pg_pool_t));
    if (pool == NULL) {
        return NULL;
    }

    memset(pool, 0, sizeof(pg_pool_t));

    pool->connections = NULL;
    pool->capacity = 0;
    pool->size = 0;
    pool->min_size = min_size;
    pool->max_size = max_size;
    pool->checkout_timeout_ms = checkout_timeout_ms;
    pool->is_fallback = is_fallback;

    pool->connection_string = conn_str ? strdup(conn_str) : NULL;
    if (conn_str && !pool->connection_string) {
        free(pool);
        return NULL;
    }

    /* 创建等待队列 */
    pool->wait_queue = pg_pool_wait_queue_create(max_size * 10);
    if (pool->wait_queue == NULL) {
        free(pool->connection_string);
        free(pool);
        return NULL;
    }

    /* 创建监控对象 */
    pool->monitor = pg_pool_monitor_create(max_size);
    if (pool->monitor == NULL) {
        pg_pool_wait_queue_destroy(pool->wait_queue);
        free(pool->connection_string);
        free(pool);
        return NULL;
    }

    /* 创建租约 ID 数组 */
    pool->connection_lease_ids = (int*)calloc(max_size, sizeof(int));
    if (pool->connection_lease_ids == NULL) {
        pg_pool_monitor_destroy(pool->monitor);
        pg_pool_wait_queue_destroy(pool->wait_queue);
        free(pool->connection_string);
        free(pool);
        return NULL;
    }

    pg_mutex_init(&pool->mutex);

    return pool;
}

/* 销毁连接池 */
void pg_pool_destroy(pg_pool_t *pool) {
    if (pool == NULL) {
        return;
    }

    pg_mutex_destroy(&pool->mutex);

    if (pool->connections != NULL) {
        for (int i = 0; i < pool->capacity; i++) {
            if (pool->connections[i] != NULL) {
                pg_connection_destroy(pool->connections[i]);
                pool->connections[i] = NULL;
            }
        }
        free(pool->connections);
        pool->connections = NULL;
    }

    if (pool->connection_string != NULL) {
        free(pool->connection_string);
        pool->connection_string = NULL;
    }

    if (pool->wait_queue != NULL) {
        pg_pool_wait_queue_destroy(pool->wait_queue);
        pool->wait_queue = NULL;
    }

    if (pool->monitor != NULL) {
        pg_pool_monitor_destroy(pool->monitor);
        pool->monitor = NULL;
    }

    if (pool->connection_lease_ids != NULL) {
        free(pool->connection_lease_ids);
        pool->connection_lease_ids = NULL;
    }

    free(pool);
}

/* 初始化连接池 */
int pg_pool_init(pg_pool_t *pool) {
    if (pool == NULL) {
        return -1;
    }

    /* 分配连接数组 */
    pool->capacity = pool->max_size;
    pool->connections = (pg_connection_t**)calloc(pool->capacity, sizeof(pg_connection_t*));
    if (pool->connections == NULL) {
        return -1;
    }

    /* 创建初始连接（min_size 为 0 时跳过，否则至少要创建 1 条） */
    int created = pg_pool_expand(pool, pool->min_size);
    if (pool->min_size > 0 && created == 0) {
        return -1;
    }
    return 0;
}

/* 扩容连接池 */
int pg_pool_expand(pg_pool_t *pool, int target_size) {
    if (pool == NULL) {
        return -1;
    }

    if (target_size > pool->max_size) {
        target_size = pool->max_size;
    }

    if (target_size <= pool->size) {
        return 0;
    }

    int created = 0;
    for (int i = pool->size; i < target_size; i++) {
        pool->connections[i] = pg_connection_create(pool->connection_string, pool->is_fallback);
        if (pool->connections[i] == NULL) {
            continue;
        }

        if (pg_connection_connect(pool->connections[i]) != 0) {
            pg_connection_destroy(pool->connections[i]);
            pool->connections[i] = NULL;
            continue;
        }

        pool->size++;
        created++;
    }

    return created;
}

/* 缩容连接池 */
int pg_pool_shrink(pg_pool_t *pool, int target_size) {
    if (pool == NULL) {
        return -1;
    }

    if (target_size < pool->min_size) {
        target_size = pool->min_size;
    }

    if (target_size >= pool->size) {
        return 0;
    }

    int removed = 0;
    for (int i = pool->size - 1; i >= target_size; i--) {
        if (pool->connections[i] != NULL) {
            pg_connection_destroy(pool->connections[i]);
            pool->connections[i] = NULL;
            pool->size--;
            removed++;
        }
    }

    return removed;
}

/* 获取连接（阻塞） */
pg_connection_t* pg_pool_checkout(pg_pool_t *pool) {
    if (pool == NULL) {
        return NULL;
    }

    pg_mutex_lock(&pool->mutex);

    /* 检查等待队列超时 */
    pg_pool_wait_queue_check_timeouts(pool->wait_queue);

    /* 查找空闲连接 */
    for (int i = 0; i < pool->size; i++) {
        if (pool->connections[i] != NULL && pool->connection_lease_ids[i] == 0) {
            pg_connection_t *conn = pool->connections[i];
            /* 跳过正在使用或独占的连接 */
            if (pg_connection_is_in_use(conn) || pg_connection_is_exclusive(conn)) {
                continue;
            }
            pg_conn_state_t state = pg_connection_get_state(conn);
            if (state == PG_CONN_READY || state == PG_CONN_CONNECTED) {
                /* 记录 checkout */
                int lease_id = pg_pool_monitor_record_checkout(pool->monitor, i, NULL);
                if (lease_id > 0) {
                    pool->connection_lease_ids[i] = lease_id;
                    pg_connection_set_in_use(conn, 1);
                    pg_connection_set_lease_id(conn, lease_id);
                    pg_mutex_unlock(&pool->mutex);
                    return conn;
                }
            }
        }
    }

    /* 尝试扩容 */
    if (pool->size < pool->max_size) {
        if (pg_pool_expand(pool, pool->size + 1) > 0) {
            int i = pool->size - 1;
            pg_connection_t *conn = pool->connections[i];
            int lease_id = pg_pool_monitor_record_checkout(pool->monitor, i, NULL);
            if (lease_id > 0) {
                pool->connection_lease_ids[i] = lease_id;
                pg_connection_set_in_use(conn, 1);
                pg_connection_set_lease_id(conn, lease_id);
                pg_mutex_unlock(&pool->mutex);
                return conn;
            }
        }
    }

    /* 连接池耗尽，记录超时 */
    pg_pool_monitor_record_timeout(pool->monitor);

    pg_mutex_unlock(&pool->mutex);
    return NULL;
}

/* 获取连接（非阻塞，超时返回 NULL） */
pg_connection_t* pg_pool_checkout_timeout(pg_pool_t *pool, int timeout_ms) {
    if (pool == NULL) {
        return NULL;
    }

    time_t start = time(NULL);
    time_t end = start + timeout_ms / 1000 + 1;

    while (time(NULL) < end) {
        pg_connection_t *conn = pg_pool_checkout(pool);
        if (conn != NULL) {
            return conn;
        }

        /* 短暂休眠避免忙等待 */
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }

    return NULL;
}

/* 归还连接 */
void pg_pool_checkin(pg_pool_t *pool, pg_connection_t *conn) {
    if (pool == NULL || conn == NULL) {
        return;
    }

    pg_mutex_lock(&pool->mutex);

    /* 查找连接索引 */
    int conn_index = -1;
    for (int i = 0; i < pool->size; i++) {
        if (pool->connections[i] == conn) {
            conn_index = i;
            break;
        }
    }

    if (conn_index < 0) {
        pg_mutex_unlock(&pool->mutex);
        return;  /* 连接不在池中 */
    }

    /* 清除连接状态 */
    pg_connection_set_in_use(conn, 0);
    pg_connection_set_lease_id(conn, 0);

    /* 检查是否有等待者 */
    pg_pool_waiter_t *waiter = pg_pool_wait_queue_pop(pool->wait_queue);
    if (waiter != NULL) {
        /* 将连接分配给等待者 */
        int lease_id = pg_pool_monitor_record_checkout(pool->monitor, conn_index, NULL);
        if (lease_id > 0) {
            pool->connection_lease_ids[conn_index] = lease_id;
            pg_connection_set_in_use(conn, 1);
            pg_connection_set_lease_id(conn, lease_id);
            /* TODO: 调用 Drogon promise 的回调 */
        }
        free(waiter);
        pg_mutex_unlock(&pool->mutex);
        return;
    }

    /* 记录 checkin */
    int lease_id = pool->connection_lease_ids[conn_index];
    if (lease_id > 0) {
        pg_pool_monitor_record_checkin(pool->monitor, lease_id);
        pool->connection_lease_ids[conn_index] = 0;
    }

    pg_mutex_unlock(&pool->mutex);
}

/* 获取独占连接（用于事务） */
pg_connection_t* pg_pool_checkout_exclusive(pg_pool_t *pool) {
    if (pool == NULL) {
        return NULL;
    }

    /* 查找空闲连接 */
    for (int i = 0; i < pool->size; i++) {
        if (pool->connections[i] != NULL && pool->connection_lease_ids[i] == 0) {
            pg_connection_t *conn = pool->connections[i];
            /* 跳过正在使用的连接 */
            if (pg_connection_is_in_use(conn)) {
                continue;
            }
            pg_conn_state_t state = pg_connection_get_state(conn);
            if (state == PG_CONN_READY || state == PG_CONN_CONNECTED) {
                /* 记录 checkout */
                int lease_id = pg_pool_monitor_record_checkout(pool->monitor, i, NULL);
                if (lease_id > 0) {
                    pool->connection_lease_ids[i] = lease_id;
                    pg_connection_set_in_use(conn, 1);
                    pg_connection_set_exclusive(conn, 1);  /* 标记为独占 */
                    pg_connection_set_lease_id(conn, lease_id);
                    return conn;
                }
            }
        }
    }

    /* 尝试扩容 */
    if (pool->size < pool->max_size) {
        if (pg_pool_expand(pool, pool->size + 1) > 0) {
            int i = pool->size - 1;
            pg_connection_t *conn = pool->connections[i];
            int lease_id = pg_pool_monitor_record_checkout(pool->monitor, i, NULL);
            if (lease_id > 0) {
                pool->connection_lease_ids[i] = lease_id;
                pg_connection_set_in_use(conn, 1);
                pg_connection_set_exclusive(conn, 1);  /* 标记为独占 */
                pg_connection_set_lease_id(conn, lease_id);
                return conn;
            }
        }
    }

    return NULL;
}

/* 归还独占连接 */
void pg_pool_checkin_exclusive(pg_pool_t *pool, pg_connection_t *conn) {
    if (pool == NULL || conn == NULL) {
        return;
    }

    /* 清除独占状态 */
    pg_connection_set_exclusive(conn, 0);
    
    /* 调用普通 checkin */
    pg_pool_checkin(pool, conn);
}

/* 获取连接池大小 */
int pg_pool_size(const pg_pool_t *pool) {
    if (pool == NULL) {
        return 0;
    }
    return pool->size;
}

/* 获取空闲连接数 */
int pg_pool_idle_count(const pg_pool_t *pool) {
    if (pool == NULL) {
        return 0;
    }

    int idle = 0;
    for (int i = 0; i < pool->size; i++) {
        if (pool->connections[i] != NULL) {
            pg_conn_state_t state = pg_connection_get_state(pool->connections[i]);
            if (state == PG_CONN_READY || state == PG_CONN_CONNECTED) {
                idle++;
            }
        }
    }

    return idle;
}

/* 健康检查所有连接 */
int pg_pool_health_check(pg_pool_t *pool) {
    if (pool == NULL) {
        return -1;
    }

    int healthy = 0;
    for (int i = 0; i < pool->size; i++) {
        if (pool->connections[i] != NULL) {
            if (pg_connection_is_alive(pool->connections[i])) {
                healthy++;
            } else {
                /* 连接失效，尝试重连 */
                pg_connection_disconnect(pool->connections[i]);
                if (pg_connection_connect(pool->connections[i]) != 0) {
                    /* 重连失败，标记为错误状态 */
                    pg_connection_record_query(pool->connections[i], 0, 1);
                }
            }
        }
    }

    return healthy;
}

/* 是否为降级连接池 */
int pg_pool_is_fallback(const pg_pool_t *pool) {
    if (pool == NULL) {
        return 0;
    }
    return pool->is_fallback;
}

/* 获取等待队列大小 */
int pg_pool_get_wait_queue_size(const pg_pool_t *pool) {
    if (pool == NULL || pool->wait_queue == NULL) {
        return 0;
    }
    return pg_pool_wait_queue_size(pool->wait_queue);
}

/* 获取监控统计 */
pg_pool_stats_t pg_pool_get_stats(const pg_pool_t *pool) {
    pg_pool_stats_t empty = {0};
    if (pool == NULL || pool->monitor == NULL) {
        return empty;
    }
    return pg_pool_monitor_get_stats(pool->monitor);
}

/* 检查连接泄漏 */
int pg_pool_check_leaks(pg_pool_t *pool) {
    if (pool == NULL || pool->monitor == NULL) {
        return 0;
    }
    return pg_pool_monitor_check_leaks(pool->monitor);
}

/* 设置泄漏超时 */
void pg_pool_set_leak_timeout(pg_pool_t *pool, int timeout_sec) {
    if (pool == NULL || pool->monitor == NULL) {
        return;
    }
    pg_pool_monitor_set_leak_timeout(pool->monitor, timeout_sec);
}

/* 获取当前使用中的连接数 */
int pg_pool_in_use_count(const pg_pool_t *pool) {
    if (pool == NULL || pool->monitor == NULL) {
        return 0;
    }
    return pg_pool_monitor_get_in_use_count(pool->monitor);
}

/* 获取等待中的请求数 */
int pg_pool_waiting_count(const pg_pool_t *pool) {
    if (pool == NULL || pool->wait_queue == NULL) {
        return 0;
    }
    return pg_pool_wait_queue_size(pool->wait_queue);
}

/* 处理等待队列（归还连接时调用） */
void pg_pool_process_wait_queue(pg_pool_t *pool, pg_connection_t *conn) {
    if (pool == NULL || conn == NULL || pool->wait_queue == NULL) {
        return;
    }

    pg_pool_waiter_t *waiter = pg_pool_wait_queue_pop(pool->wait_queue);
    if (waiter != NULL) {
        /* TODO: 调用 Drogon promise 的回调，将连接分配给等待者 */
        free(waiter);
    }
}

/* 预热连接池（创建 min_size 个连接） */
int pg_pool_warmup(pg_pool_t *pool) {
    if (pool == NULL) {
        return -1;
    }

    /* 如果已经有足够的连接，直接返回 */
    if (pool->size >= pool->min_size) {
        return pool->size;
    }

    /* 扩容到 min_size */
    return pg_pool_expand(pool, pool->min_size);
}

/* 获取可用连接数 */
int pg_pool_available_count(const pg_pool_t *pool) {
    if (pool == NULL) {
        return 0;
    }

    int available = 0;
    for (int i = 0; i < pool->size; i++) {
        if (pool->connections[i] != NULL && pool->connection_lease_ids[i] == 0) {
            pg_connection_t *conn = pool->connections[i];
            /* 跳过正在使用或独占的连接 */
            if (!pg_connection_is_in_use(conn) && !pg_connection_is_exclusive(conn)) {
                pg_conn_state_t state = pg_connection_get_state(conn);
                if (state == PG_CONN_READY || state == PG_CONN_CONNECTED) {
                    available++;
                }
            }
        }
    }

    return available;
}

/* 检查连接池是否已满 */
int pg_pool_is_full(const pg_pool_t *pool) {
    if (pool == NULL) {
        return 0;
    }
    return pool->size >= pool->max_size;
}

/* 检查连接池是否为空 */
int pg_pool_is_empty(const pg_pool_t *pool) {
    if (pool == NULL) {
        return 1;
    }
    return pg_pool_available_count(pool) == 0;
}
