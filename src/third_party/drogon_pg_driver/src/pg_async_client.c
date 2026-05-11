#include "pg_async_client.h"
#include "pg_config.h"
#include "pg_logger.h"
#include <stdlib.h>
#include <string.h>

static pg_async_client_t g_client = {0};

/* 获取客户端单例 */
pg_async_client_t* pg_async_client_get_instance(void) {
    return &g_client;
}

/* 初始化客户端 */
int pg_async_client_init(const char *conn_str, int connection_count) {
    if (g_client.initialized) {
        return 0;
    }

    if (conn_str == NULL || connection_count <= 0) {
        return -1;
    }

    g_client.connection_string = strdup(conn_str);
    if (g_client.connection_string == NULL) {
        return -1;
    }

    g_client.capacity = connection_count;
    g_client.connection_count = 0;

    g_client.connections = (pg_async_connection_t**)calloc(connection_count, sizeof(pg_async_connection_t*));
    if (g_client.connections == NULL) {
        free(g_client.connection_string);
        g_client.connection_string = NULL;
        return -1;
    }

    /* 分配回调数组 */
    g_client.callbacks = (pg_pending_callback_t**)calloc(connection_count, sizeof(pg_pending_callback_t*));
    if (g_client.callbacks == NULL) {
        free(g_client.connections);
        free(g_client.connection_string);
        g_client.connection_string = NULL;
        return -1;
    }
    g_client.callback_count = connection_count;

    /* 创建所有连接 */
    pg_config_t *config = pg_config_get_instance();
    int is_fallback = (pg_config_get_driver_mode(config) == PG_DRIVER_MODE_FALLBACK);

    for (int i = 0; i < connection_count; i++) {
        g_client.connections[i] = pg_async_connection_create(conn_str, is_fallback);
        if (g_client.connections[i] == NULL) {
            continue;
        }

        if (pg_async_connection_start(g_client.connections[i]) != 0) {
            pg_async_connection_destroy(g_client.connections[i]);
            g_client.connections[i] = NULL;
            continue;
        }

        g_client.connection_count++;
    }

    if (g_client.connection_count == 0) {
        free(g_client.callbacks);
        free(g_client.connections);
        g_client.connections = NULL;
        free(g_client.connection_string);
        g_client.connection_string = NULL;
        return -1;
    }

    g_client.initialized = 1;
    return 0;
}

/* 清理客户端 */
void pg_async_client_cleanup(void) {
    if (!g_client.initialized) {
        return;
    }

    if (g_client.connections != NULL) {
        for (int i = 0; i < g_client.capacity; i++) {
            if (g_client.connections[i] != NULL) {
                pg_async_connection_destroy(g_client.connections[i]);
                g_client.connections[i] = NULL;
            }
        }
        free(g_client.connections);
        g_client.connections = NULL;
    }

    /* 清理回调 */
    if (g_client.callbacks != NULL) {
        for (int i = 0; i < g_client.callback_count; i++) {
            pg_pending_callback_t *cb = g_client.callbacks[i];
            while (cb != NULL) {
                pg_pending_callback_t *next = cb->next;
                free(cb);
                cb = next;
            }
        }
        free(g_client.callbacks);
        g_client.callbacks = NULL;
    }

    if (g_client.connection_string != NULL) {
        free(g_client.connection_string);
        g_client.connection_string = NULL;
    }

    g_client.initialized = 0;
}

/* 存储回调 */
static int store_callback(int conn_index, pg_async_query_callback_t callback, void *user_data) {
    if (g_client.callbacks == NULL || conn_index < 0 || conn_index >= g_client.callback_count) {
        return -1;
    }

    pg_pending_callback_t *cb = (pg_pending_callback_t*)malloc(sizeof(pg_pending_callback_t));
    if (cb == NULL) {
        return -1;
    }

    cb->callback = callback;
    cb->user_data = user_data;
    cb->connection_index = conn_index;
    cb->next = g_client.callbacks[conn_index];
    g_client.callbacks[conn_index] = cb;

    return 0;
}

/* 获取并移除回调 */
static pg_pending_callback_t* pop_callback(int conn_index) {
    if (g_client.callbacks == NULL || conn_index < 0 || conn_index >= g_client.callback_count) {
        return NULL;
    }

    pg_pending_callback_t *cb = g_client.callbacks[conn_index];
    if (cb != NULL) {
        g_client.callbacks[conn_index] = cb->next;
    }
    return cb;
}

/* 轮询所有连接 */
int pg_async_client_poll(void) {
    if (!g_client.initialized) {
        return -1;
    }

    int ready_count = 0;

    for (int i = 0; i < g_client.connection_count; i++) {
        if (g_client.connections[i] == NULL) {
            continue;
        }

        pg_async_conn_state_t state = pg_async_connection_get_state(g_client.connections[i]);

        /* 连接中，轮询连接状态 */
        if (state == PG_ASYNC_CONN_CONNECTING || state == PG_ASYNC_CONN_AUTHENTICATING) {
            int result = pg_async_connection_poll(g_client.connections[i]);
            if (result > 0) {
                ready_count++;
            }
        }
        /* 查询中，消费输入 */
        else if (state == PG_ASYNC_CONN_QUERYING) {
            if (pg_async_connection_consume_input(g_client.connections[i]) != 0) {
                continue;
            }

            if (!pg_async_connection_is_busy(g_client.connections[i])) {
                ready_count++;
            }
        }
    }

    return ready_count;
}

/* 异步执行查询 */
int pg_async_client_exec_async(const char *sql, const char **params, int param_count,
                              pg_async_query_callback_t callback, void *user_data) {
    if (!g_client.initialized || sql == NULL) {
        return -1;
    }

    /* 查找可用连接 */
    for (int i = 0; i < g_client.connection_count; i++) {
        if (g_client.connections[i] == NULL) {
            continue;
        }

        pg_async_conn_state_t state = pg_async_connection_get_state(g_client.connections[i]);

        if (state == PG_ASYNC_CONN_READY) {
            int result;
            if (params != NULL && param_count > 0) {
                result = pg_async_connection_send_query_params(g_client.connections[i], sql, params, param_count);
            } else {
                result = pg_async_connection_send_query(g_client.connections[i], sql);
            }

            if (result == 0) {
                /* 刷新输出 */
                pg_async_connection_flush(g_client.connections[i]);

                /* 存储回调 */
                if (callback != NULL) {
                    if (store_callback(i, callback, user_data) != 0) {
                        PG_LOG_WARN("[ASYNC_CLIENT] Failed to store callback for connection %d", i);
                    }
                }

                return i;  /* 返回连接索引 */
            }
        }
    }

    return -1; /* 没有可用连接 */
}

/* 获取需要监听的 socket 数量 */
int pg_async_client_get_poll_fds(int *fds, int max_fds) {
    if (!g_client.initialized || fds == NULL) {
        return 0;
    }

    int count = 0;

    for (int i = 0; i < g_client.connection_count && count < max_fds; i++) {
        if (g_client.connections[i] == NULL) {
            continue;
        }

        int fd = pg_async_connection_get_socket(g_client.connections[i]);
        if (fd >= 0) {
            fds[count++] = fd;
        }
    }

    return count;
}

/* 处理 socket 可读事件 */
void pg_async_client_handle_read(int fd) {
    if (!g_client.initialized) {
        return;
    }

    for (int i = 0; i < g_client.connection_count; i++) {
        if (g_client.connections[i] == NULL) {
            continue;
        }

        int conn_fd = pg_async_connection_get_socket(g_client.connections[i]);
        if (conn_fd == fd) {
            /* 消费输入 */
            pg_async_connection_consume_input(g_client.connections[i]);

            /* 轮询连接状态 */
            pg_async_connection_poll(g_client.connections[i]);

            /* 检查结果 */
            if (!pg_async_connection_is_busy(g_client.connections[i])) {
                PGresult *pq_result = pg_async_connection_get_result(g_client.connections[i]);
                if (pq_result != NULL) {
                    /* 创建结果对象 */
                    pg_result_t *result = pg_result_create(pq_result);

                    /* 获取并调用回调 */
                    pg_pending_callback_t *cb = pop_callback(i);
                    if (cb != NULL && cb->callback != NULL) {
                        cb->callback(result, cb->user_data);
                        free(cb);
                    }

                    pg_result_destroy(result);
                    PQclear(pq_result);
                }
            }

            break;
        }
    }
}

/* 处理 socket 可写事件 */
void pg_async_client_handle_write(int fd) {
    if (!g_client.initialized) {
        return;
    }

    for (int i = 0; i < g_client.connection_count; i++) {
        if (g_client.connections[i] == NULL) {
            continue;
        }

        int conn_fd = pg_async_connection_get_socket(g_client.connections[i]);
        if (conn_fd == fd) {
            /* 轮询连接状态 */
            pg_async_connection_poll(g_client.connections[i]);

            /* 刷新输出 */
            pg_async_connection_flush(g_client.connections[i]);

            break;
        }
    }
}

/* 获取连接状态 */
pg_async_conn_state_t pg_async_client_get_connection_state(int index) {
    if (!g_client.initialized || index < 0 || index >= g_client.connection_count) {
        return PG_ASYNC_CONN_DISCONNECTED;
    }

    if (g_client.connections[index] == NULL) {
        return PG_ASYNC_CONN_DISCONNECTED;
    }

    return pg_async_connection_get_state(g_client.connections[index]);
}
