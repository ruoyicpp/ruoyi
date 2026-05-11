#ifndef PG_RESULT_H
#define PG_RESULT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PostgreSQL libpq 前向声明 */
typedef struct pg_result PGresult;

/* 结果状态 */
typedef enum {
    PG_RES_EMPTY_QUERY,
    PG_RES_COMMAND_OK,
    PG_RES_TUPLES_OK,
    PG_RES_COPY_OUT,
    PG_RES_COPY_IN,
    PG_RES_BAD_RESPONSE,
    PG_RES_NONFATAL_ERROR,
    PG_RES_FATAL_ERROR,
    PG_RES_COPY_BOTH,
    PG_RES_SINGLE_TUPLE,
    PG_RES_PIPELINE_SYNC,
    PG_RES_PIPELINE_ABORTED,
    PG_RES_TUPLES_CHUNK
} pg_result_status_t;

/* 结果对象 */
typedef struct pg_result {
    PGresult *pq_result;
    pg_result_status_t status;
    int n_tuples;
    int n_columns;
    char **column_names;
    uint32_t *column_oids;
    char *error_message;
    char *sql_state;
} pg_result_t;

/* 行对象 */
typedef struct pg_row {
    pg_result_t *result;
    int row_index;
} pg_row_t;

/* 创建结果对象（从 libpq PGresult） */
pg_result_t* pg_result_create(PGresult *pq_result);

/* 创建空结果对象（用于写入操作成功） */
pg_result_t* pg_result_create_empty(void);

/* 销毁结果对象 */
void pg_result_destroy(pg_result_t *result);

/* 获取结果状态 */
pg_result_status_t pg_result_status(const pg_result_t *result);

/* 获取行数 */
int pg_result_n_tuples(const pg_result_t *result);

/* 获取列数 */
int pg_result_n_columns(const pg_result_t *result);

/* 获取列名 */
const char* pg_result_column_name(const pg_result_t *result, int col);

/* 获取列 OID */
uint32_t pg_result_column_oid(const pg_result_t *result, int col);

/* 获取行对象 */
pg_row_t pg_result_row(pg_result_t *result, int row);

/* 检查是否为 NULL */
int pg_row_is_null(const pg_row_t *row, int col);

/* 获取字段值（文本格式） */
const char* pg_row_get_value(const pg_row_t *row, int col);

/* 获取字段长度 */
int pg_row_get_length(const pg_row_t *row, int col);

/* 获取受影响的行数 */
int pg_result_affected_rows(const pg_result_t *result);

/* 获取命令标记（如 "INSERT 0 1"） */
const char* pg_result_command_tag(const pg_result_t *result);

/* 获取错误消息 */
const char* pg_result_error_message(const pg_result_t *result);

/* 获取 SQLSTATE */
const char* pg_result_sql_state(const pg_result_t *result);

/* 是否为错误 */
int pg_result_is_error(const pg_result_t *result);

/* 释放底层 libpq PGresult */
void pg_result_clear(pg_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* PG_RESULT_H */
