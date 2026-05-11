#include "pg_async_connection.h"
#include <stdlib.h>
#include <string.h>

/* 创建非阻塞连接 */
pg_async_connection_t* pg_async_connection_create(const char *conn_str, int is_fallback) {
    pg_async_connection_t *conn = (pg_async_connection_t*)malloc(sizeof(pg_async_connection_t));
    if (conn == NULL) {
        return NULL;
    }

    memset(conn, 0, sizeof(pg_async_connection_t));

    conn->pq_conn = NULL;
    conn->state = PG_ASYNC_CONN_DISCONNECTED;
    conn->socket_fd = -1;
    conn->is_fallback = is_fallback;
    conn->connection_string = conn_str ? strdup(conn_str) : NULL;
    conn->want_read = 0;
    conn->want_write = 0;

    if (conn_str && !conn->connection_string) {
        free(conn);
        return NULL;
    }

    return conn;
}

/* 销毁非阻塞连接 */
void pg_async_connection_destroy(pg_async_connection_t *conn) {
    if (conn == NULL) {
        return;
    }

    if (conn->pq_conn != NULL) {
        PQfinish(conn->pq_conn);
        conn->pq_conn = NULL;
    }

    if (conn->connection_string != NULL) {
        free(conn->connection_string);
        conn->connection_string = NULL;
    }

    free(conn);
}

/* 开始连接（非阻塞） */
int pg_async_connection_start(pg_async_connection_t *conn) {
    if (conn == NULL || conn->connection_string == NULL) {
        return -1;
    }

    conn->pq_conn = PQconnectStart(conn->connection_string);
    if (conn->pq_conn == NULL) {
        conn->state = PG_ASYNC_CONN_ERROR;
        return -1;
    }

    /* 设置非阻塞模式 */
    if (PQsetnonblocking(conn->pq_conn, 1) != 0) {
        conn->state = PG_ASYNC_CONN_ERROR;
        PQfinish(conn->pq_conn);
        conn->pq_conn = NULL;
        return -1;
    }

    conn->state = PG_ASYNC_CONN_CONNECTING;
    return 0;
}

/* 轮询连接状态 */
int pg_async_connection_poll(pg_async_connection_t *conn) {
    if (conn == NULL || conn->pq_conn == NULL) {
        return -1;
    }

    PostgresPollingStatusType status = PQconnectPoll(conn->pq_conn);
    conn->socket_fd = PQsocket(conn->pq_conn);

    switch (status) {
        case PGRES_POLLING_READING:
            conn->want_read = 1;
            conn->want_write = 0;
            break;
        case PGRES_POLLING_WRITING:
            conn->want_read = 0;
            conn->want_write = 1;
            break;
        case PGRES_POLLING_OK:
            conn->state = PG_ASYNC_CONN_READY;
            conn->want_read = 0;
            conn->want_write = 0;
            return 1;
        case PGRES_POLLING_FAILED:
            conn->state = PG_ASYNC_CONN_ERROR;
            conn->want_read = 0;
            conn->want_write = 0;
            return -1;
        default:
            break;
    }

    return 0;
}

/* 获取连接状态 */
pg_async_conn_state_t pg_async_connection_get_state(const pg_async_connection_t *conn) {
    if (conn == NULL) {
        return PG_ASYNC_CONN_DISCONNECTED;
    }
    return conn->state;
}

/* 获取 socket fd */
int pg_async_connection_get_socket(const pg_async_connection_t *conn) {
    if (conn == NULL || conn->pq_conn == NULL) {
        return -1;
    }
    return PQsocket(conn->pq_conn);
}

/* 是否需要读取 */
int pg_async_connection_want_read(const pg_async_connection_t *conn) {
    if (conn == NULL) {
        return 0;
    }
    return conn->want_read;
}

/* 是否需要写入 */
int pg_async_connection_want_write(const pg_async_connection_t *conn) {
    if (conn == NULL) {
        return 0;
    }
    return conn->want_write;
}

/* 发送查询（非阻塞） */
int pg_async_connection_send_query(pg_async_connection_t *conn, const char *sql) {
    if (conn == NULL || conn->pq_conn == NULL || sql == NULL) {
        return -1;
    }

    if (conn->state != PG_ASYNC_CONN_READY) {
        return -1;
    }

    int result = PQsendQuery(conn->pq_conn, sql);
    if (result == 1) {
        conn->state = PG_ASYNC_CONN_QUERYING;
        return 0;
    }

    return -1;
}

/* 发送参数化查询（非阻塞） */
int pg_async_connection_send_query_params(pg_async_connection_t *conn, const char *sql, const char **params, int param_count) {
    if (conn == NULL || conn->pq_conn == NULL || sql == NULL) {
        return -1;
    }

    if (conn->state != PG_ASYNC_CONN_READY) {
        return -1;
    }

    int result = PQsendQueryParams(conn->pq_conn, sql, param_count, NULL,
                                   (const char * const *)params, NULL, NULL, 0);
    if (result == 1) {
        conn->state = PG_ASYNC_CONN_QUERYING;
        return 0;
    }

    return -1;
}

/* 消费输入 */
int pg_async_connection_consume_input(pg_async_connection_t *conn) {
    if (conn == NULL || conn->pq_conn == NULL) {
        return -1;
    }

    return PQconsumeInput(conn->pq_conn);
}

/* 是否忙碌 */
int pg_async_connection_is_busy(pg_async_connection_t *conn) {
    if (conn == NULL || conn->pq_conn == NULL) {
        return 1;
    }

    int is_busy = PQisBusy(conn->pq_conn);
    if (!is_busy) {
        conn->state = PG_ASYNC_CONN_READY;
    }

    return is_busy;
}

/* 获取结果 */
PGresult* pg_async_connection_get_result(pg_async_connection_t *conn) {
    if (conn == NULL || conn->pq_conn == NULL) {
        return NULL;
    }

    return PQgetResult(conn->pq_conn);
}

/* 刷新输出 */
int pg_async_connection_flush(pg_async_connection_t *conn) {
    if (conn == NULL || conn->pq_conn == NULL) {
        return -1;
    }

    return PQflush(conn->pq_conn);
}

/* 获取底层 libpq 连接 */
PGconn* pg_async_connection_get_pq_conn(pg_async_connection_t *conn) {
    if (conn == NULL) {
        return NULL;
    }
    return conn->pq_conn;
}
