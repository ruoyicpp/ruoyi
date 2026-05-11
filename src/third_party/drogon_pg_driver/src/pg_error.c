#include "pg_error.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

pg_error_t* pg_error_create(pg_error_code_t code, const char *message) {
    return pg_error_create_with_sqlstate(code, message, NULL);
}

pg_error_t* pg_error_create_with_sqlstate(pg_error_code_t code, const char *message, const char *sql_state) {
    pg_error_t *error = (pg_error_t*)malloc(sizeof(pg_error_t));
    if (error == NULL) {
        return NULL;
    }

    error->code = code;
    error->message = message ? strdup(message) : strdup("Unknown error");
    error->sql_state = sql_state ? strdup(sql_state) : NULL;

    /* 构建完整错误消息 */
    const char *code_str = pg_error_code_to_string(code);
    size_t full_len = strlen(code_str) + strlen(error->message) + 4; /* "[code] message\0" */
    if (error->sql_state != NULL) {
        full_len += strlen(error->sql_state) + 12; /* " SQLSTATE: xxx\0" */
    }

    error->full_message = (char*)malloc(full_len + 64); /* 额外空间防止溢出 */
    if (error->full_message != NULL) {
        if (error->sql_state != NULL) {
            snprintf(error->full_message, full_len + 64, "[%s] %s SQLSTATE: %s",
                     code_str, error->message, error->sql_state);
        } else {
            snprintf(error->full_message, full_len + 64, "[%s] %s", code_str, error->message);
        }
    } else {
        error->full_message = strdup(error->message);
    }

    return error;
}

void pg_error_destroy(pg_error_t *error) {
    if (error == NULL) {
        return;
    }

    if (error->message != NULL) {
        free(error->message);
        error->message = NULL;
    }

    if (error->sql_state != NULL) {
        free(error->sql_state);
        error->sql_state = NULL;
    }

    if (error->full_message != NULL) {
        free(error->full_message);
        error->full_message = NULL;
    }

    free(error);
}

pg_error_code_t pg_error_get_code(const pg_error_t *error) {
    if (error == NULL) {
        return PG_ERROR_UNKNOWN_ERROR;
    }
    return error->code;
}

const char* pg_error_get_message(const pg_error_t *error) {
    if (error == NULL || error->message == NULL) {
        return "Unknown error";
    }
    return error->message;
}

const char* pg_error_get_sqlstate(const pg_error_t *error) {
    if (error == NULL || error->sql_state == NULL) {
        return "";
    }
    return error->sql_state;
}

const char* pg_error_get_full_message(const pg_error_t *error) {
    if (error == NULL || error->full_message == NULL) {
        return "Unknown error";
    }
    return error->full_message;
}

const char* pg_error_code_to_string(pg_error_code_t code) {
    switch (code) {
        case PG_ERROR_SUCCESS:
            return "SUCCESS";
        case PG_ERROR_CONNECTION_FAILED:
            return "CONNECTION_FAILED";
        case PG_ERROR_CONNECTION_TIMEOUT:
            return "CONNECTION_TIMEOUT";
        case PG_ERROR_QUERY_FAILED:
            return "QUERY_FAILED";
        case PG_ERROR_PARAMETER_ERROR:
            return "PARAMETER_ERROR";
        case PG_ERROR_TYPE_CONVERSION_ERROR:
            return "TYPE_CONVERSION_ERROR";
        case PG_ERROR_TRANSACTION_ERROR:
            return "TRANSACTION_ERROR";
        case PG_ERROR_POOL_EXHAUSTED:
            return "POOL_EXHAUSTED";
        case PG_ERROR_POOL_TIMEOUT:
            return "POOL_TIMEOUT";
        case PG_ERROR_STATEMENT_CACHE_ERROR:
            return "STATEMENT_CACHE_ERROR";
        case PG_ERROR_CIRCUIT_BREAKER_OPEN:
            return "CIRCUIT_BREAKER_OPEN";
        case PG_ERROR_HEALTH_CHECK_FAILED:
            return "HEALTH_CHECK_FAILED";
        case PG_ERROR_UNKNOWN_ERROR:
        default:
            return "UNKNOWN_ERROR";
    }
}
