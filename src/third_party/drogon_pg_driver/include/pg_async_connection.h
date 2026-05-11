#ifndef PG_ASYNC_CONNECTION_H
#define PG_ASYNC_CONNECTION_H

#include <libpq-fe.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 非阻塞连接状态 */
typedef enum {
    PG_ASYNC_CONN_DISCONNECTED,
    PG_ASYNC_CONN_CONNECTING,
    PG_ASYNC_CONN_AUTHENTICATING,
    PG_ASYNC_CONN_READY,
    PG_ASYNC_CONN_QUERYING,
    PG_ASYNC_CONN_ERROR
} pg_async_conn_state_t;

/* 非阻塞连接对象 */
typedef struct pg_async_connection {
    PGconn *pq_conn;
    pg_async_conn_state_t state;
    int socket_fd;
    int is_fallback;
    char *connection_string;
    int want_read;
    int want_write;
} pg_async_connection_t;

/* 创建非阻塞连接 */
pg_async_connection_t* pg_async_connection_create(const char *conn_str, int is_fallback);

/* 销毁非阻塞连接 */
void pg_async_connection_destroy(pg_async_connection_t *conn);

/* 开始连接（非阻塞） */
int pg_async_connection_start(pg_async_connection_t *conn);

/* 轮询连接状态 */
int pg_async_connection_poll(pg_async_connection_t *conn);

/* 获取连接状态 */
pg_async_conn_state_t pg_async_connection_get_state(const pg_async_connection_t *conn);

/* 获取 socket fd */
int pg_async_connection_get_socket(const pg_async_connection_t *conn);

/* 是否需要读取 */
int pg_async_connection_want_read(const pg_async_connection_t *conn);

/* 是否需要写入 */
int pg_async_connection_want_write(const pg_async_connection_t *conn);

/* 发送查询（非阻塞） */
int pg_async_connection_send_query(pg_async_connection_t *conn, const char *sql);

/* 发送参数化查询（非阻塞） */
int pg_async_connection_send_query_params(pg_async_connection_t *conn, const char *sql, const char **params, int param_count);

/* 消费输入 */
int pg_async_connection_consume_input(pg_async_connection_t *conn);

/* 是否忙碌 */
int pg_async_connection_is_busy(pg_async_connection_t *conn);

/* 获取结果 */
PGresult* pg_async_connection_get_result(pg_async_connection_t *conn);

/* 刷新输出 */
int pg_async_connection_flush(pg_async_connection_t *conn);

/* 获取底层 libpq 连接 */
PGconn* pg_async_connection_get_pq_conn(pg_async_connection_t *conn);

#ifdef __cplusplus
}
#endif

#endif /* PG_ASYNC_CONNECTION_H */
