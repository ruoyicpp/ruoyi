#ifndef PG_ASYNC_CLIENT_H
#define PG_ASYNC_CLIENT_H

#include "pg_async_connection.h"
#include "pg_result.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 查询回调类型 */
typedef void (*pg_async_query_callback_t)(pg_result_t *result, void *user_data);

/* 待处理的回调 */
typedef struct pg_pending_callback {
    pg_async_query_callback_t callback;
    void *user_data;
    int connection_index;
    struct pg_pending_callback *next;
} pg_pending_callback_t;

/* 非阻塞客户端对象 */
typedef struct pg_async_client {
    pg_async_connection_t **connections;
    int connection_count;
    int capacity;
    char *connection_string;
    int initialized;
    pg_pending_callback_t **callbacks;  /* 每个连接的回调 */
    int callback_count;
} pg_async_client_t;

/* 获取客户端单例 */
pg_async_client_t* pg_async_client_get_instance(void);

/* 初始化客户端 */
int pg_async_client_init(const char *conn_str, int connection_count);

/* 清理客户端 */
void pg_async_client_cleanup(void);

/* 轮询所有连接（需要在 EventLoop 中定期调用） */
int pg_async_client_poll(void);

/* 异步执行查询 */
int pg_async_client_exec_async(const char *sql, const char **params, int param_count,
                              pg_async_query_callback_t callback, void *user_data);

/* 获取需要监听的 socket 数量 */
int pg_async_client_get_poll_fds(int *fds, int max_fds);

/* 处理 socket 可读事件 */
void pg_async_client_handle_read(int fd);

/* 处理 socket 可写事件 */
void pg_async_client_handle_write(int fd);

/* 获取连接状态 */
pg_async_conn_state_t pg_async_client_get_connection_state(int index);

#ifdef __cplusplus
}
#endif

#endif /* PG_ASYNC_CLIENT_H */
