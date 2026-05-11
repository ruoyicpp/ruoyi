#ifndef PG_WRITER_CONNECTION_H
#define PG_WRITER_CONNECTION_H

#include <libpq-fe.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Writer 连接状态 */
typedef enum {
    PG_WRITER_DISCONNECTED,
    PG_WRITER_CONNECTING,
    PG_WRITER_READY,
    PG_WRITER_BUSY,
    PG_WRITER_ERROR
} pg_writer_state_t;

/* Writer 连接统计 */
typedef struct pg_writer_stats {
    int64_t total_writes;
    int64_t total_batches;
    int64_t total_errors;
    int64_t total_rows_affected;
    int64_t total_time_ms;
    int current_queue_size;
    int peak_queue_size;
} pg_writer_stats_t;

/* Writer 连接对象 */
typedef struct pg_writer_connection {
    PGconn *pq_conn;
    pg_writer_state_t state;
    char *connection_string;
    int in_transaction;
    int batch_size;
    int max_batch_size;
    int batch_timeout_ms;
    int64_t last_write_time;
    pg_writer_stats_t stats;
} pg_writer_connection_t;

/* 创建 Writer 连接 */
pg_writer_connection_t* pg_writer_connection_create(const char *conn_str, int max_batch_size, int batch_timeout_ms);

/* 销毁 Writer 连接 */
void pg_writer_connection_destroy(pg_writer_connection_t *writer);

/* 连接 */
int pg_writer_connection_connect(pg_writer_connection_t *writer);

/* 断开连接 */
void pg_writer_connection_disconnect(pg_writer_connection_t *writer);

/* 开始批量事务 */
int pg_writer_connection_begin_batch(pg_writer_connection_t *writer);

/* 提交批量事务 */
int pg_writer_connection_commit_batch(pg_writer_connection_t *writer);

/* 回滚批量事务 */
int pg_writer_connection_rollback_batch(pg_writer_connection_t *writer);

/* 执行写入 */
int pg_writer_connection_write(pg_writer_connection_t *writer, const char *sql, const char **params, int param_count);

/* 执行写入并返回影响的行数 */
int pg_writer_connection_write_with_result(pg_writer_connection_t *writer, const char *sql, const char **params, int param_count, int *rows_affected);

/* 检查是否需要提交（超时或批量满） */
int pg_writer_connection_should_commit(const pg_writer_connection_t *writer);

/* 获取状态 */
pg_writer_state_t pg_writer_connection_get_state(const pg_writer_connection_t *writer);

/* 获取统计 */
pg_writer_stats_t pg_writer_connection_get_stats(const pg_writer_connection_t *writer);

/* 重置统计 */
void pg_writer_connection_reset_stats(pg_writer_connection_t *writer);

/* 获取底层 libpq 连接 */
PGconn* pg_writer_connection_get_pq_conn(pg_writer_connection_t *writer);

/* 是否在事务中 */
int pg_writer_connection_in_transaction(const pg_writer_connection_t *writer);

/* 获取当前批量大小 */
int pg_writer_connection_get_batch_size(const pg_writer_connection_t *writer);

#ifdef __cplusplus
}
#endif

#endif /* PG_WRITER_CONNECTION_H */
