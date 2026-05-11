#include "pg_connection.h"
#include "pg_config.h"
#include "pg_error.h"
#include <libpq-fe.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* SQL Hash (FNV-64) */
static uint64_t fnv64_hash(const char *str, size_t len) {
    const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    const uint64_t FNV_PRIME = 1099511628211ULL;

    uint64_t hash = FNV_OFFSET_BASIS;

    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)(unsigned char)str[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

/* 预编译语句缓存 */
static pg_stmt_cache_entry_t* stmt_cache_entry_create(const char *name, const char *sql) {
    pg_stmt_cache_entry_t *entry = (pg_stmt_cache_entry_t*)malloc(sizeof(pg_stmt_cache_entry_t));
    if (entry == NULL) {
        return NULL;
    }

    entry->name = name ? strdup(name) : NULL;
    entry->sql = sql ? strdup(sql) : NULL;
    entry->last_used = time(NULL);
    entry->next = NULL;
    entry->prev = NULL;

    if ((name && !entry->name) || (sql && !entry->sql)) {
        if (entry->name) free(entry->name);
        if (entry->sql) free(entry->sql);
        free(entry);
        return NULL;
    }

    return entry;
}

static void stmt_cache_entry_destroy(pg_stmt_cache_entry_t *entry) {
    if (entry == NULL) {
        return;
    }

    if (entry->name) {
        free(entry->name);
        entry->name = NULL;
    }

    if (entry->sql) {
        free(entry->sql);
        entry->sql = NULL;
    }

    free(entry);
}

static void stmt_cache_remove(pg_stmt_cache_t *cache, pg_stmt_cache_entry_t *entry) {
    if (cache == NULL || entry == NULL) {
        return;
    }

    if (entry->prev != NULL) {
        entry->prev->next = entry->next;
    } else {
        cache->head = entry->next;
    }

    if (entry->next != NULL) {
        entry->next->prev = entry->prev;
    } else {
        cache->tail = entry->prev;
    }

    cache->size--;
    stmt_cache_entry_destroy(entry);
}

static void stmt_cache_clear(pg_stmt_cache_t *cache) {
    if (cache == NULL) {
        return;
    }

    pg_stmt_cache_entry_t *entry = cache->head;
    while (entry != NULL) {
        pg_stmt_cache_entry_t *next = entry->next;
        stmt_cache_entry_destroy(entry);
        entry = next;
    }

    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;
}

/* 创建连接 */
pg_connection_t* pg_connection_create(const char *conn_str, int is_fallback) {
    pg_connection_t *conn = (pg_connection_t*)malloc(sizeof(pg_connection_t));
    if (conn == NULL) {
        return NULL;
    }

    memset(conn, 0, sizeof(pg_connection_t));

    conn->pq_conn = NULL;
    conn->state = PG_CONN_DISCONNECTED;
    conn->is_fallback = is_fallback;
    conn->connection_string = conn_str ? strdup(conn_str) : NULL;
    conn->last_error_time = 0;
    conn->consecutive_errors = 0;

    /* 初始化统计 */
    conn->stat.query_count = 0;
    conn->stat.slow_count = 0;
    conn->stat.error_count = 0;
    conn->stat.last_active_at = time(NULL);
    conn->stat.created_at = time(NULL);
    conn->stat.total_query_time_ms = 0;

    /* 初始化语句缓存 */
    conn->stmt_cache.head = NULL;
    conn->stmt_cache.tail = NULL;
    conn->stmt_cache.size = 0;

    pg_config_t *config = pg_config_get_instance();
    conn->stmt_cache.max_size = pg_config_get_stmt_cache_size(config);

    if (conn_str && !conn->connection_string) {
        free(conn);
        return NULL;
    }

    return conn;
}

/* 销毁连接 */
void pg_connection_destroy(pg_connection_t *conn) {
    if (conn == NULL) {
        return;
    }

    pg_connection_disconnect(conn);
    stmt_cache_clear(&conn->stmt_cache);

    if (conn->connection_string != NULL) {
        free(conn->connection_string);
        conn->connection_string = NULL;
    }

    free(conn);
}

/* 连接 PostgreSQL */
int pg_connection_connect(pg_connection_t *conn) {
    if (conn == NULL) {
        return -1;
    }

    if (conn->state == PG_CONN_CONNECTED || conn->state == PG_CONN_READY) {
        return 0;
    }

    conn->state = PG_CONN_CONNECTING;

    conn->pq_conn = PQconnectdb(conn->connection_string);
    if (conn->pq_conn == NULL) {
        conn->state = PG_CONN_ERROR;
        return -1;
    }

    if (PQstatus(conn->pq_conn) != CONNECTION_OK) {
        PQfinish(conn->pq_conn);
        conn->pq_conn = NULL;
        conn->state = PG_CONN_ERROR;
        return -1;
    }

    conn->state = PG_CONN_READY;
    conn->stat.last_active_at = time(NULL);
    return 0;
}

/* 断开连接 */
void pg_connection_disconnect(pg_connection_t *conn) {
    if (conn == NULL) {
        return;
    }

    if (conn->pq_conn != NULL) {
        PQfinish(conn->pq_conn);
        conn->pq_conn = NULL;
    }

    conn->state = PG_CONN_DISCONNECTED;
    stmt_cache_clear(&conn->stmt_cache);
}

/* 检查连接是否存活 */
int pg_connection_is_alive(pg_connection_t *conn) {
    if (conn == NULL || conn->pq_conn == NULL) {
        return 0;
    }

    PGresult *res = PQexec(conn->pq_conn, "SELECT 1");
    if (res == NULL) {
        return 0;
    }

    int is_ok = (PQresultStatus(res) == PGRES_TUPLES_OK);
    PQclear(res);

    return is_ok;
}

/* 获取连接状态 */
pg_conn_state_t pg_connection_get_state(const pg_connection_t *conn) {
    if (conn == NULL) {
        return PG_CONN_DISCONNECTED;
    }
    return conn->state;
}

/* 获取连接统计 */
void pg_connection_get_stat(const pg_connection_t *conn, pg_conn_stat_t *stat) {
    if (conn == NULL || stat == NULL) {
        return;
    }

    memcpy(stat, &conn->stat, sizeof(pg_conn_stat_t));
}

/* 重置连接统计 */
void pg_connection_reset_stat(pg_connection_t *conn) {
    if (conn == NULL) {
        return;
    }

    conn->stat.query_count = 0;
    conn->stat.slow_count = 0;
    conn->stat.error_count = 0;
    conn->stat.total_query_time_ms = 0;
}

/* 添加预编译语句到缓存 */
int pg_stmt_cache_add(pg_connection_t *conn, const char *name, const char *sql) {
    if (conn == NULL || name == NULL || sql == NULL) {
        return -1;
    }

    /* 降级模式不使用预编译缓存 */
    if (conn->is_fallback) {
        return -1;
    }

    pg_stmt_cache_t *cache = &conn->stmt_cache;

    /* 检查是否已存在 */
    const char *existing = pg_stmt_cache_get(conn, sql);
    if (existing != NULL) {
        return 0;
    }

    /* 缓存已满，移除最久未使用的 */
    if (cache->size >= cache->max_size) {
        if (cache->tail != NULL) {
            stmt_cache_remove(cache, cache->tail);
        }
    }

    /* 创建新条目 */
    pg_stmt_cache_entry_t *entry = stmt_cache_entry_create(name, sql);
    if (entry == NULL) {
        return -1;
    }

    /* 添加到链表头部 */
    entry->next = cache->head;
    if (cache->head != NULL) {
        cache->head->prev = entry;
    }
    cache->head = entry;

    if (cache->tail == NULL) {
        cache->tail = entry;
    }

    cache->size++;
    return 0;
}

/* 从缓存获取预编译语句 */
const char* pg_stmt_cache_get(pg_connection_t *conn, const char *sql) {
    if (conn == NULL || sql == NULL) {
        return NULL;
    }

    /* 降级模式不使用预编译缓存 */
    if (conn->is_fallback) {
        return NULL;
    }

    pg_stmt_cache_t *cache = &conn->stmt_cache;
    pg_stmt_cache_entry_t *entry = cache->head;

    while (entry != NULL) {
        if (strcmp(entry->sql, sql) == 0) {
            /* 更新访问时间 */
            entry->last_used = time(NULL);

            /* 移动到链表头部 */
            if (entry != cache->head) {
                if (entry->prev != NULL) {
                    entry->prev->next = entry->next;
                }
                if (entry->next != NULL) {
                    entry->next->prev = entry->prev;
                }
                if (entry == cache->tail) {
                    cache->tail = entry->prev;
                }
                entry->prev = NULL;
                entry->next = cache->head;
                if (cache->head != NULL) {
                    cache->head->prev = entry;
                }
                cache->head = entry;
            }

            return entry->name;
        }
        entry = entry->next;
    }

    return NULL;
}

/* 清空缓存 */
void pg_stmt_cache_clear(pg_connection_t *conn) {
    if (conn == NULL) {
        return;
    }
    stmt_cache_clear(&conn->stmt_cache);
}

/* 获取缓存大小 */
int pg_stmt_cache_size(const pg_connection_t *conn) {
    if (conn == NULL) {
        return 0;
    }
    return conn->stmt_cache.size;
}

/* 获取底层 libpq 连接 */
PGconn* pg_connection_get_pq_conn(pg_connection_t *conn) {
    if (conn == NULL) {
        return NULL;
    }
    return conn->pq_conn;
}

/* 是否为降级连接 */
int pg_connection_is_fallback(const pg_connection_t *conn) {
    if (conn == NULL) {
        return 0;
    }
    return conn->is_fallback;
}

/* 记录查询 */
void pg_connection_record_query(pg_connection_t *conn, int64_t elapsed_ms, int is_error) {
    if (conn == NULL) {
        return;
    }

    conn->stat.query_count++;
    conn->stat.total_query_time_ms += elapsed_ms;
    conn->stat.last_active_at = time(NULL);

    pg_config_t *config = pg_config_get_instance();
    int slow_threshold = pg_config_get_slow_query_threshold_ms(config);

    if (elapsed_ms > slow_threshold) {
        conn->stat.slow_count++;
    }

    if (is_error) {
        conn->stat.error_count++;
        conn->consecutive_errors++;
        conn->last_error_time = time(NULL);
    } else {
        conn->consecutive_errors = 0;
    }
}

/* 获取连续错误次数 */
int pg_connection_get_consecutive_errors(const pg_connection_t *conn) {
    if (conn == NULL) {
        return 0;
    }
    return conn->consecutive_errors;
}

/* 重置连续错误次数 */
void pg_connection_reset_consecutive_errors(pg_connection_t *conn) {
    if (conn == NULL) {
        return;
    }
    conn->consecutive_errors = 0;
}

/* 是否正在使用 */
int pg_connection_is_in_use(const pg_connection_t *conn) {
    if (conn == NULL) {
        return 0;
    }
    return conn->in_use;
}

/* 是否为独占连接 */
int pg_connection_is_exclusive(const pg_connection_t *conn) {
    if (conn == NULL) {
        return 0;
    }
    return conn->is_exclusive;
}

/* 获取借出时间 */
time_t pg_connection_get_checkout_time(const pg_connection_t *conn) {
    if (conn == NULL) {
        return 0;
    }
    return conn->checkout_time;
}

/* 获取租约 ID */
int pg_connection_get_lease_id(const pg_connection_t *conn) {
    if (conn == NULL) {
        return 0;
    }
    return conn->lease_id;
}

/* 设置使用状态 */
void pg_connection_set_in_use(pg_connection_t *conn, int in_use) {
    if (conn == NULL) {
        return;
    }
    conn->in_use = in_use;
    if (in_use) {
        conn->checkout_time = time(NULL);
    }
}

/* 设置独占状态 */
void pg_connection_set_exclusive(pg_connection_t *conn, int is_exclusive) {
    if (conn == NULL) {
        return;
    }
    conn->is_exclusive = is_exclusive;
}

/* 设置租约 ID */
void pg_connection_set_lease_id(pg_connection_t *conn, int lease_id) {
    if (conn == NULL) {
        return;
    }
    conn->lease_id = lease_id;
}
