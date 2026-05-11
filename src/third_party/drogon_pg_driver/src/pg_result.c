#include "pg_result.h"
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>

/* 创建空结果对象 */
pg_result_t* pg_result_create_empty(void) {
    pg_result_t *result = (pg_result_t*)malloc(sizeof(pg_result_t));
    if (result == NULL) {
        return NULL;
    }

    memset(result, 0, sizeof(pg_result_t));
    result->pq_result = NULL;
    result->status = PG_RES_COMMAND_OK;
    result->n_tuples = 0;
    result->n_columns = 0;
    result->column_names = NULL;
    result->column_oids = NULL;
    result->error_message = NULL;
    result->sql_state = NULL;

    return result;
}

/* 创建结果对象 */
pg_result_t* pg_result_create(PGresult *pq_result) {
    if (pq_result == NULL) {
        return NULL;
    }

    pg_result_t *result = (pg_result_t*)malloc(sizeof(pg_result_t));
    if (result == NULL) {
        return NULL;
    }

    memset(result, 0, sizeof(pg_result_t));

    result->pq_result = pq_result;

    /* 映射 libpq 状态 */
    ExecStatusType pq_status = PQresultStatus(pq_result);
    switch (pq_status) {
        case PGRES_EMPTY_QUERY:
            result->status = PG_RES_EMPTY_QUERY;
            break;
        case PGRES_COMMAND_OK:
            result->status = PG_RES_COMMAND_OK;
            break;
        case PGRES_TUPLES_OK:
            result->status = PG_RES_TUPLES_OK;
            break;
        case PGRES_COPY_OUT:
            result->status = PG_RES_COPY_OUT;
            break;
        case PGRES_COPY_IN:
            result->status = PG_RES_COPY_IN;
            break;
        case PGRES_BAD_RESPONSE:
            result->status = PG_RES_BAD_RESPONSE;
            break;
        case PGRES_NONFATAL_ERROR:
            result->status = PG_RES_NONFATAL_ERROR;
            break;
        case PGRES_FATAL_ERROR:
            result->status = PG_RES_FATAL_ERROR;
            break;
        case PGRES_COPY_BOTH:
            result->status = PG_RES_COPY_BOTH;
            break;
        case PGRES_SINGLE_TUPLE:
            result->status = PG_RES_SINGLE_TUPLE;
            break;
        case PGRES_PIPELINE_SYNC:
            result->status = PG_RES_PIPELINE_SYNC;
            break;
        case PGRES_PIPELINE_ABORTED:
            result->status = PG_RES_PIPELINE_ABORTED;
            break;
        default:
            result->status = PG_RES_TUPLES_OK;
            break;
    }

    result->n_tuples = PQntuples(pq_result);
    result->n_columns = PQnfields(pq_result);

    /* 复制列名和 OID */
    if (result->n_columns > 0) {
        result->column_names = (char**)malloc(result->n_columns * sizeof(char*));
        result->column_oids = (uint32_t*)malloc(result->n_columns * sizeof(uint32_t));

        if (result->column_names == NULL || result->column_oids == NULL) {
            if (result->column_names) free(result->column_names);
            if (result->column_oids) free(result->column_oids);
            free(result);
            return NULL;
        }

        for (int i = 0; i < result->n_columns; i++) {
            result->column_names[i] = strdup(PQfname(pq_result, i));
            result->column_oids[i] = PQftype(pq_result, i);
        }
    }

    /* 复制错误信息 */
    if (result->status == PG_RES_FATAL_ERROR || result->status == PG_RES_NONFATAL_ERROR) {
        const char *error_msg = PQresultErrorMessage(pq_result);
        if (error_msg != NULL) {
            result->error_message = strdup(error_msg);
        }

        const char *sql_state = PQresultErrorField(pq_result, PG_DIAG_SQLSTATE);
        if (sql_state != NULL) {
            result->sql_state = strdup(sql_state);
        }
    }

    return result;
}

/* 销毁结果对象 */
void pg_result_destroy(pg_result_t *result) {
    if (result == NULL) {
        return;
    }

    if (result->column_names != NULL) {
        for (int i = 0; i < result->n_columns; i++) {
            if (result->column_names[i] != NULL) {
                free(result->column_names[i]);
            }
        }
        free(result->column_names);
        result->column_names = NULL;
    }

    if (result->column_oids != NULL) {
        free(result->column_oids);
        result->column_oids = NULL;
    }

    if (result->error_message != NULL) {
        free(result->error_message);
        result->error_message = NULL;
    }

    if (result->sql_state != NULL) {
        free(result->sql_state);
        result->sql_state = NULL;
    }

    if (result->pq_result != NULL) {
        PQclear(result->pq_result);
        result->pq_result = NULL;
    }

    free(result);
}

/* 获取结果状态 */
pg_result_status_t pg_result_status(const pg_result_t *result) {
    if (result == NULL) {
        return PG_RES_FATAL_ERROR;
    }
    return result->status;
}

/* 获取行数 */
int pg_result_n_tuples(const pg_result_t *result) {
    if (result == NULL) {
        return 0;
    }
    return result->n_tuples;
}

/* 获取列数 */
int pg_result_n_columns(const pg_result_t *result) {
    if (result == NULL) {
        return 0;
    }
    return result->n_columns;
}

/* 获取列名 */
const char* pg_result_column_name(const pg_result_t *result, int col) {
    if (result == NULL || result->column_names == NULL) {
        return NULL;
    }

    if (col < 0 || col >= result->n_columns) {
        return NULL;
    }

    return result->column_names[col];
}

/* 获取列 OID */
uint32_t pg_result_column_oid(const pg_result_t *result, int col) {
    if (result == NULL || result->column_oids == NULL) {
        return 0;
    }

    if (col < 0 || col >= result->n_columns) {
        return 0;
    }

    return result->column_oids[col];
}

/* 获取行对象 */
pg_row_t pg_result_row(pg_result_t *result, int row) {
    pg_row_t row_obj;
    row_obj.result = result;
    row_obj.row_index = row;
    return row_obj;
}

/* 检查是否为 NULL */
int pg_row_is_null(const pg_row_t *row, int col) {
    if (row == NULL || row->result == NULL) {
        return 1;
    }

    if (row->row_index < 0 || row->row_index >= row->result->n_tuples) {
        return 1;
    }

    if (col < 0 || col >= row->result->n_columns) {
        return 1;
    }

    return PQgetisnull(row->result->pq_result, row->row_index, col);
}

/* 获取字段值 */
const char* pg_row_get_value(const pg_row_t *row, int col) {
    if (row == NULL || row->result == NULL) {
        return NULL;
    }

    if (row->row_index < 0 || row->row_index >= row->result->n_tuples) {
        return NULL;
    }

    if (col < 0 || col >= row->result->n_columns) {
        return NULL;
    }

    return PQgetvalue(row->result->pq_result, row->row_index, col);
}

/* 获取字段长度 */
int pg_row_get_length(const pg_row_t *row, int col) {
    if (row == NULL || row->result == NULL) {
        return 0;
    }

    if (row->row_index < 0 || row->row_index >= row->result->n_tuples) {
        return 0;
    }

    if (col < 0 || col >= row->result->n_columns) {
        return 0;
    }

    return PQgetlength(row->result->pq_result, row->row_index, col);
}

/* 获取受影响的行数 */
int pg_result_affected_rows(const pg_result_t *result) {
    if (result == NULL || result->pq_result == NULL) {
        return 0;
    }

    const char *cmd_tag = PQcmdTuples(result->pq_result);
    if (cmd_tag == NULL || cmd_tag[0] == '\0') {
        return 0;
    }

    return atoi(cmd_tag);
}

/* 获取命令标记 */
const char* pg_result_command_tag(const pg_result_t *result) {
    if (result == NULL || result->pq_result == NULL) {
        return NULL;
    }
    return PQcmdStatus(result->pq_result);
}

/* 获取错误消息 */
const char* pg_result_error_message(const pg_result_t *result) {
    if (result == NULL) {
        return NULL;
    }
    return result->error_message ? result->error_message : "";
}

/* 获取 SQLSTATE */
const char* pg_result_sql_state(const pg_result_t *result) {
    if (result == NULL) {
        return NULL;
    }
    return result->sql_state ? result->sql_state : "";
}

/* 是否为错误 */
int pg_result_is_error(const pg_result_t *result) {
    if (result == NULL) {
        return 1;
    }
    return (result->status == PG_RES_FATAL_ERROR || result->status == PG_RES_NONFATAL_ERROR);
}

/* 释放底层 libpq PGresult */
void pg_result_clear(pg_result_t *result) {
    if (result == NULL || result->pq_result == NULL) {
        return;
    }
    PQclear(result->pq_result);
    result->pq_result = NULL;
}
