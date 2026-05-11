#ifndef PG_CONNECTION_H
#define PG_CONNECTION_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PostgreSQL libpq 前向声明 */
typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;

/* 连接状态 */
typedef enum {
    PG_CONN_DISCONNECTED,
    PG_CONN_CONNECTING,
    PG_CONN_CONNECTED,
    PG_CONN_AUTHENTICATING,
    PG_CONN_READY,
    PG_CONN_ERROR
} pg_conn_state_t;

/* 连接统计 */
typedef struct pg_conn_stat {
    int query_count;
    int slow_count;
    int error_count;
    time_t last_active_at;
    time_t created_at;
    int64_t total_query_time_ms;
} pg_conn_stat_t;

/* 预编译语句缓存项 */
typedef struct pg_stmt_cache_entry {
    char *name;
    char *sql;
    time_t last_used;
    struct pg_stmt_cache_entry *next;
    struct pg_stmt_cache_entry *prev;
} pg_stmt_cache_entry_t;

/* 预编译语句缓存 */
typedef struct pg_stmt_cache {
    pg_stmt_cache_entry_t *head;
    pg_stmt_cache_entry_t *tail;
    int size;
    int max_size;
} pg_stmt_cache_t;

/* 连接对象 */
typedef struct pg_connection {
    PGconn *pq_conn;
    pg_conn_state_t state;
    pg_conn_stat_t stat;
    pg_stmt_cache_t stmt_cache;
    int is_fallback;  /* 是否为降级连接 */
    char *connection_string;
    time_t last_error_time;
    int consecutive_errors;
    int in_use;       /* 是否正在使用 */
    int is_exclusive; /* 是否为独占连接（事务） */
    time_t checkout_time; /* 借出时间 */
    int lease_id;     /* 租约 ID */
} pg_connection_t;

/* 创建连接 */
pg_connection_t* pg_connection_create(const char *conn_str, int is_fallback);

/* 销毁连接 */
void pg_connection_destroy(pg_connection_t *conn);

/* 连接 PostgreSQL */
int pg_connection_connect(pg_connection_t *conn);

/* 断开连接 */
void pg_connection_disconnect(pg_connection_t *conn);

/* 检查连接是否存活 */
int pg_connection_is_alive(pg_connection_t *conn);

/* 获取连接状态 */
pg_conn_state_t pg_connection_get_state(const pg_connection_t *conn);

/* 获取连接统计 */
void pg_connection_get_stat(const pg_connection_t *conn, pg_conn_stat_t *stat);

/* 重置连接统计 */
void pg_connection_reset_stat(pg_connection_t *conn);

/* 预编译语句缓存 */

/* 添加预编译语句到缓存 */
int pg_stmt_cache_add(pg_connection_t *conn, const char *name, const char *sql);

/* 从缓存获取预编译语句 */
const char* pg_stmt_cache_get(pg_connection_t *conn, const char *sql);

/* 清空缓存 */
void pg_stmt_cache_clear(pg_connection_t *conn);

/* 获取缓存大小 */
int pg_stmt_cache_size(const pg_connection_t *conn);

/* 获取底层 libpq 连接 */
PGconn* pg_connection_get_pq_conn(pg_connection_t *conn);

/* 是否为降级连接 */
int pg_connection_is_fallback(const pg_connection_t *conn);

/* 记录查询 */
void pg_connection_record_query(pg_connection_t *conn, int64_t elapsed_ms, int is_error);

/* 获取连续错误次数 */
int pg_connection_get_consecutive_errors(const pg_connection_t *conn);

/* 重置连续错误次数 */
void pg_connection_reset_consecutive_errors(pg_connection_t *conn);

/* 是否正在使用 */
int pg_connection_is_in_use(const pg_connection_t *conn);

/* 是否为独占连接 */
int pg_connection_is_exclusive(const pg_connection_t *conn);

/* 获取借出时间 */
time_t pg_connection_get_checkout_time(const pg_connection_t *conn);

/* 获取租约 ID */
int pg_connection_get_lease_id(const pg_connection_t *conn);

/* 设置使用状态 */
void pg_connection_set_in_use(pg_connection_t *conn, int in_use);

/* 设置独占状态 */
void pg_connection_set_exclusive(pg_connection_t *conn, int is_exclusive);

/* 设置租约 ID */
void pg_connection_set_lease_id(pg_connection_t *conn, int lease_id);

#ifdef __cplusplus
}
#endif

#endif /* PG_CONNECTION_H */
