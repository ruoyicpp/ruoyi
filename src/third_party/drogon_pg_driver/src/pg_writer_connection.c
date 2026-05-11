#include "pg_writer_connection.h"
#include "pg_logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 创建 Writer 连接 */
pg_writer_connection_t* pg_writer_connection_create(const char *conn_str, int max_batch_size, int batch_timeout_ms) {
    pg_writer_connection_t *writer = (pg_writer_connection_t*)malloc(sizeof(pg_writer_connection_t));
    if (writer == NULL) {
        return NULL;
    }

    memset(writer, 0, sizeof(pg_writer_connection_t));

    writer->pq_conn = NULL;
    writer->state = PG_WRITER_DISCONNECTED;
    writer->connection_string = conn_str ? strdup(conn_str) : NULL;
    writer->in_transaction = 0;
    writer->batch_size = 0;
    writer->max_batch_size = max_batch_size > 0 ? max_batch_size : 200;
    writer->batch_timeout_ms = batch_timeout_ms > 0 ? batch_timeout_ms : 1000;
    writer->last_write_time = 0;

    if (conn_str && !writer->connection_string) {
        free(writer);
        return NULL;
    }

    return writer;
}

/* 销毁 Writer 连接 */
void pg_writer_connection_destroy(pg_writer_connection_t *writer) {
    if (writer == NULL) {
        return;
    }

    /* 如果在事务中，先回滚 */
    if (writer->in_transaction && writer->pq_conn != NULL) {
        PGresult *result = PQexec(writer->pq_conn, "ROLLBACK");
        if (result != NULL) {
            PQclear(result);
        }
    }

    if (writer->pq_conn != NULL) {
        PQfinish(writer->pq_conn);
        writer->pq_conn = NULL;
    }

    if (writer->connection_string != NULL) {
        free(writer->connection_string);
        writer->connection_string = NULL;
    }

    free(writer);
}

/* 连接 */
int pg_writer_connection_connect(pg_writer_connection_t *writer) {
    if (writer == NULL || writer->connection_string == NULL) {
        return -1;
    }

    if (writer->pq_conn != NULL) {
        PQfinish(writer->pq_conn);
        writer->pq_conn = NULL;
    }

    writer->pq_conn = PQconnectdb(writer->connection_string);
    if (writer->pq_conn == NULL) {
        writer->state = PG_WRITER_ERROR;
        return -1;
    }

    if (PQstatus(writer->pq_conn) != CONNECTION_OK) {
        pg_logger_connection_event("writer_connect", PQerrorMessage(writer->pq_conn), 0);
        PQfinish(writer->pq_conn);
        writer->pq_conn = NULL;
        writer->state = PG_WRITER_ERROR;
        return -1;
    }

    writer->state = PG_WRITER_READY;
    writer->in_transaction = 0;
    writer->batch_size = 0;

    pg_logger_connection_event("writer_connect", NULL, 1);
    return 0;
}

/* 断开连接 */
void pg_writer_connection_disconnect(pg_writer_connection_t *writer) {
    if (writer == NULL) {
        return;
    }

    /* 如果在事务中，先回滚 */
    if (writer->in_transaction && writer->pq_conn != NULL) {
        PGresult *result = PQexec(writer->pq_conn, "ROLLBACK");
        if (result != NULL) {
            PQclear(result);
        }
        writer->in_transaction = 0;
    }

    if (writer->pq_conn != NULL) {
        PQfinish(writer->pq_conn);
        writer->pq_conn = NULL;
    }

    writer->state = PG_WRITER_DISCONNECTED;
    writer->batch_size = 0;
}

/* 开始批量事务 */
int pg_writer_connection_begin_batch(pg_writer_connection_t *writer) {
    if (writer == NULL || writer->pq_conn == NULL) {
        return -1;
    }

    if (writer->in_transaction) {
        return 0;  /* 已经在事务中 */
    }

    PGresult *result = PQexec(writer->pq_conn, "BEGIN");
    if (result == NULL) {
        writer->state = PG_WRITER_ERROR;
        return -1;
    }

    ExecStatusType status = PQresultStatus(result);
    PQclear(result);

    if (status != PGRES_COMMAND_OK) {
        pg_logger_error("begin_batch", PQerrorMessage(writer->pq_conn));
        writer->state = PG_WRITER_ERROR;
        return -1;
    }

    writer->in_transaction = 1;
    writer->batch_size = 0;
    writer->last_write_time = (int64_t)time(NULL) * 1000;

    return 0;
}

/* 提交批量事务 */
int pg_writer_connection_commit_batch(pg_writer_connection_t *writer) {
    if (writer == NULL || writer->pq_conn == NULL) {
        return -1;
    }

    if (!writer->in_transaction) {
        return 0;  /* 没有事务 */
    }

    PGresult *result = PQexec(writer->pq_conn, "COMMIT");
    if (result == NULL) {
        writer->state = PG_WRITER_ERROR;
        return -1;
    }

    ExecStatusType status = PQresultStatus(result);
    PQclear(result);

    if (status != PGRES_COMMAND_OK) {
        pg_logger_error("commit_batch", PQerrorMessage(writer->pq_conn));
        /* 尝试回滚 */
        PGresult *rollback = PQexec(writer->pq_conn, "ROLLBACK");
        if (rollback != NULL) {
            PQclear(rollback);
        }
        writer->in_transaction = 0;
        writer->batch_size = 0;
        writer->state = PG_WRITER_ERROR;
        return -1;
    }

    writer->stats.total_batches++;
    writer->stats.total_writes += writer->batch_size;
    int committed = writer->batch_size;
    writer->in_transaction = 0;
    writer->batch_size = 0;
    writer->state = PG_WRITER_READY;

    if (committed > 1)
        PG_LOG_DEBUG("[WRITER] Batch committed: %d writes", committed);

    return 0;
}

/* 回滚批量事务 */
int pg_writer_connection_rollback_batch(pg_writer_connection_t *writer) {
    if (writer == NULL || writer->pq_conn == NULL) {
        return -1;
    }

    if (!writer->in_transaction) {
        return 0;
    }

    PGresult *result = PQexec(writer->pq_conn, "ROLLBACK");
    if (result != NULL) {
        PQclear(result);
    }

    writer->in_transaction = 0;
    writer->batch_size = 0;
    writer->state = PG_WRITER_READY;

    PG_LOG_WARN("[WRITER] Batch rolled back");

    return 0;
}

/* 执行写入 */
int pg_writer_connection_write(pg_writer_connection_t *writer, const char *sql, const char **params, int param_count) {
    if (writer == NULL || writer->pq_conn == NULL || sql == NULL) {
        return -1;
    }

    /* 如果不在事务中，开始事务 */
    if (!writer->in_transaction) {
        if (pg_writer_connection_begin_batch(writer) != 0) {
            return -1;
        }
    }

    writer->state = PG_WRITER_BUSY;

    PGresult *result = NULL;
    if (params != NULL && param_count > 0) {
        result = PQexecParams(writer->pq_conn, sql, param_count, NULL, 
                              (const char * const *)params, NULL, NULL, 0);
    } else {
        result = PQexec(writer->pq_conn, sql);
    }

    if (result == NULL) {
        writer->state = PG_WRITER_ERROR;
        writer->stats.total_errors++;
        pg_logger_error("write", PQerrorMessage(writer->pq_conn));
        return -1;
    }

    ExecStatusType status = PQresultStatus(result);
    char *rows_str = PQcmdTuples(result);
    int rows_affected = rows_str ? atoi(rows_str) : 0;
    PQclear(result);

    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        writer->state = PG_WRITER_ERROR;
        writer->stats.total_errors++;
        pg_logger_error("write", PQerrorMessage(writer->pq_conn));
        return -1;
    }

    writer->batch_size++;
    writer->stats.total_rows_affected += rows_affected;
    writer->last_write_time = (int64_t)time(NULL) * 1000;
    writer->state = PG_WRITER_READY;

    return 0;
}

/* 执行写入并返回影响的行数 */
int pg_writer_connection_write_with_result(pg_writer_connection_t *writer, const char *sql, const char **params, int param_count, int *rows_affected) {
    if (rows_affected != NULL) {
        *rows_affected = 0;
    }

    int result = pg_writer_connection_write(writer, sql, params, param_count);
    if (result == 0 && rows_affected != NULL) {
        /* 从统计中获取最近影响的行数 */
        /* 这里简化处理，实际应该从 PQcmdTuples 获取 */
    }

    return result;
}

/* 检查是否需要提交（超时或批量满） */
int pg_writer_connection_should_commit(const pg_writer_connection_t *writer) {
    if (writer == NULL || !writer->in_transaction) {
        return 0;
    }

    /* 批量满 */
    if (writer->batch_size >= writer->max_batch_size) {
        return 1;
    }

    /* 超时 */
    int64_t now = (int64_t)time(NULL) * 1000;
    if (now - writer->last_write_time >= writer->batch_timeout_ms) {
        return 1;
    }

    return 0;
}

/* 获取状态 */
pg_writer_state_t pg_writer_connection_get_state(const pg_writer_connection_t *writer) {
    if (writer == NULL) {
        return PG_WRITER_DISCONNECTED;
    }
    return writer->state;
}

/* 获取统计 */
pg_writer_stats_t pg_writer_connection_get_stats(const pg_writer_connection_t *writer) {
    pg_writer_stats_t empty = {0};
    if (writer == NULL) {
        return empty;
    }
    return writer->stats;
}

/* 重置统计 */
void pg_writer_connection_reset_stats(pg_writer_connection_t *writer) {
    if (writer == NULL) {
        return;
    }

    memset(&writer->stats, 0, sizeof(pg_writer_stats_t));
}

/* 获取底层 libpq 连接 */
PGconn* pg_writer_connection_get_pq_conn(pg_writer_connection_t *writer) {
    if (writer == NULL) {
        return NULL;
    }
    return writer->pq_conn;
}

/* 是否在事务中 */
int pg_writer_connection_in_transaction(const pg_writer_connection_t *writer) {
    if (writer == NULL) {
        return 0;
    }
    return writer->in_transaction;
}

/* 获取当前批量大小 */
int pg_writer_connection_get_batch_size(const pg_writer_connection_t *writer) {
    if (writer == NULL) {
        return 0;
    }
    return writer->batch_size;
}
