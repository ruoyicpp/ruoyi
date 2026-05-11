#ifndef PG_CLIENT_H
#define PG_CLIENT_H

#include "pg_result.h"
#include "pg_error.h"
#include "pg_circuit_breaker.h"
#include "pg_pool.h"
#include "pg_async_write_queue.h"
#include "pg_writer_connection.h"
#include "pg_mutex.h"
#include "pg_sql_parser.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 客户端对象 */
typedef struct pg_client {
    pg_pool_t *main_pool;
    pg_pool_t *fallback_pool;
    pg_circuit_breaker_t *circuit_breaker;
    pg_async_write_queue_t *write_queue;
    pg_writer_connection_t *writer_conn;
    pg_mutex_t mutex;
    char *connection_string;
    int initialized;
} pg_client_t;

/* 获取客户端单例 */
pg_client_t* pg_client_get_instance(void);

/* 初始化客户端 */
int pg_client_init(const char *conn_str);

/* 清理客户端 */
void pg_client_cleanup(void);

/* 执行查询（同步） */
pg_result_t* pg_client_exec(const char *sql, const char **params, int param_count);

/* 执行查询（参数化） */
pg_result_t* pg_client_exec_params(const char *sql, const char **params, const char **param_types, int param_count);

/* 开始事务 */
int pg_client_begin_transaction(void);

/* 提交事务 */
int pg_client_commit_transaction(void);

/* 回滚事务 */
int pg_client_rollback_transaction(void);

/* 获取熔断器状态 */
pg_cb_state_t pg_client_get_circuit_breaker_state(void);

/* 获取主连接池大小 */
int pg_client_get_main_pool_size(void);

/* 获取降级连接池大小 */
int pg_client_get_fallback_pool_size(void);

/* 健康检查 */
int pg_client_health_check(void);

/* 占位符转换：? → $1, $2, ... */
char* pg_client_convert_placeholders(const char *sql);

/* 执行写入操作（异步队列） */
int pg_client_exec_write(const char *sql, const char **params, int param_count, void *promise);

/* 获取写队列 */
pg_async_write_queue_t* pg_client_get_write_queue(void);

/* 获取连接池统计 */
pg_pool_stats_t pg_client_get_pool_stats(void);

/* 检查连接泄漏 */
int pg_client_check_leaks(void);

/* 设置泄漏超时 */
void pg_client_set_leak_timeout(int timeout_sec);

/* SQL 参数转义（防止注入） */
char* pg_client_escape_string(const char *input);

/* SQL 标识符转义（表名、列名） */
char* pg_client_escape_identifier(const char *input);

/* 验证 SQL 参数 */
int pg_client_validate_params(const char **params, int param_count);

/* 验证 SQL 语句 */
int pg_client_validate_sql(const char *sql);

/* 批量插入（使用 COPY 命令） */
int pg_client_bulk_insert(const char *table, const char *columns, const char **rows, int row_count);

/* 批量插入（使用多值 INSERT） */
int pg_client_batch_insert(const char *table, const char *columns, const char **rows, int row_count, int batch_size);

/* 获取连接池预热状态 */
int pg_client_warmup_pool(void);

/* 获取可用连接数 */
int pg_client_available_connections(void);

#ifdef __cplusplus
}
#endif

#endif /* PG_CLIENT_H */
