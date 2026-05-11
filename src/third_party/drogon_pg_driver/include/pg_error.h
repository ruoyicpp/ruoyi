#ifndef PG_ERROR_H
#define PG_ERROR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PG_ERROR_SUCCESS = 0,
    PG_ERROR_CONNECTION_FAILED,
    PG_ERROR_CONNECTION_TIMEOUT,
    PG_ERROR_QUERY_FAILED,
    PG_ERROR_PARAMETER_ERROR,
    PG_ERROR_TYPE_CONVERSION_ERROR,
    PG_ERROR_TRANSACTION_ERROR,
    PG_ERROR_POOL_EXHAUSTED,
    PG_ERROR_POOL_TIMEOUT,
    PG_ERROR_STATEMENT_CACHE_ERROR,
    PG_ERROR_CIRCUIT_BREAKER_OPEN,
    PG_ERROR_HEALTH_CHECK_FAILED,
    PG_ERROR_UNKNOWN_ERROR
} pg_error_code_t;

typedef struct pg_error {
    pg_error_code_t code;
    char *message;
    char *sql_state;
    char *full_message;
} pg_error_t;

/* 创建错误对象 */
pg_error_t* pg_error_create(pg_error_code_t code, const char *message);
pg_error_t* pg_error_create_with_sqlstate(pg_error_code_t code, const char *message, const char *sql_state);

/* 销毁错误对象 */
void pg_error_destroy(pg_error_t *error);

/* 获取错误代码 */
pg_error_code_t pg_error_get_code(const pg_error_t *error);

/* 获取错误消息 */
const char* pg_error_get_message(const pg_error_t *error);

/* 获取 SQLSTATE */
const char* pg_error_get_sqlstate(const pg_error_t *error);

/* 获取完整错误消息 */
const char* pg_error_get_full_message(const pg_error_t *error);

/* 错误代码转字符串 */
const char* pg_error_code_to_string(pg_error_code_t code);

#ifdef __cplusplus
}
#endif

#endif /* PG_ERROR_H */
