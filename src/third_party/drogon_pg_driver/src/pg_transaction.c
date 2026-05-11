#include "pg_transaction.h"
#include "pg_client.h"
#include "pg_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libpq-fe.h>

/* 创建事务对象 */
pg_transaction_t* pg_transaction_create(void) {
    pg_transaction_t *txn = (pg_transaction_t*)malloc(sizeof(pg_transaction_t));
    if (txn == NULL) {
        return NULL;
    }

    memset(txn, 0, sizeof(pg_transaction_t));
    txn->in_transaction = 0;
    txn->auto_commit = 1;
    txn->savepoint_count = 0;
    txn->conn = NULL;
    txn->pool = NULL;

    return txn;
}

/* 创建事务对象并绑定连接池 */
pg_transaction_t* pg_transaction_create_with_pool(pg_pool_t *pool) {
    pg_transaction_t *txn = pg_transaction_create();
    if (txn == NULL) {
        return NULL;
    }

    txn->pool = pool;
    return txn;
}

/* 销毁事务对象 */
void pg_transaction_destroy(pg_transaction_t *txn) {
    if (txn == NULL) {
        return;
    }

    /* 如果还在事务中，尝试回滚 */
    if (txn->in_transaction && txn->conn != NULL) {
        pg_transaction_rollback(txn);
    }

    /* 如果持有独占连接，归还 */
    if (txn->conn != NULL && txn->pool != NULL) {
        pg_pool_checkin_exclusive(txn->pool, txn->conn);
        txn->conn = NULL;
    }

    free(txn);
}

/* 绑定连接池 */
void pg_transaction_set_pool(pg_transaction_t *txn, pg_pool_t *pool) {
    if (txn == NULL) {
        return;
    }
    txn->pool = pool;
}

/* 在独占连接上执行 SQL */
static pg_result_t* exec_on_conn(pg_connection_t *conn, const char *sql, const char **params, int param_count) {
    if (conn == NULL || sql == NULL) {
        return NULL;
    }

    PGconn *pq_conn = pg_connection_get_pq_conn(conn);
    if (pq_conn == NULL) {
        return NULL;
    }

    PGresult *pq_result = NULL;
    if (params != NULL && param_count > 0) {
        pq_result = PQexecParams(pq_conn, sql, param_count, NULL,
                                 (const char * const *)params, NULL, NULL, 0);
    } else {
        pq_result = PQexec(pq_conn, sql);
    }

    if (pq_result == NULL) {
        return NULL;
    }

    pg_result_t *result = pg_result_create(pq_result);
    PQclear(pq_result);

    return result;
}

/* 开始事务 */
int pg_transaction_begin(pg_transaction_t *txn) {
    if (txn == NULL) {
        return -1;
    }

    if (txn->in_transaction) {
        return -1; /* 已经在事务中 */
    }

    /* 获取独占连接 */
    if (txn->pool == NULL) {
        PG_LOG_ERROR("[TRANSACTION] No pool bound to transaction");
        return -1;
    }

    txn->conn = pg_pool_checkout_exclusive(txn->pool);
    if (txn->conn == NULL) {
        PG_LOG_ERROR("[TRANSACTION] Failed to checkout exclusive connection");
        return -1;
    }

    /* 在独占连接上执行 BEGIN */
    pg_result_t *result = exec_on_conn(txn->conn, "BEGIN", NULL, 0);
    if (result == NULL || pg_result_is_error(result)) {
        PG_LOG_ERROR("[TRANSACTION] Failed to begin transaction");
        if (result != NULL) {
            pg_result_destroy(result);
        }
        pg_pool_checkin_exclusive(txn->pool, txn->conn);
        txn->conn = NULL;
        return -1;
    }

    pg_result_destroy(result);
    txn->in_transaction = 1;
    txn->auto_commit = 0;

    PG_LOG_INFO("[TRANSACTION] Transaction started on exclusive connection");
    return 0;
}

/* 提交事务 */
int pg_transaction_commit(pg_transaction_t *txn) {
    if (txn == NULL) {
        return -1;
    }

    if (!txn->in_transaction) {
        return -1; /* 不在事务中 */
    }

    /* 在独占连接上执行 COMMIT */
    pg_result_t *result = exec_on_conn(txn->conn, "COMMIT", NULL, 0);
    if (result == NULL || pg_result_is_error(result)) {
        PG_LOG_ERROR("[TRANSACTION] Failed to commit transaction");
        if (result != NULL) {
            pg_result_destroy(result);
        }
        /* 提交失败，尝试回滚 */
        pg_transaction_rollback(txn);
        return -1;
    }

    pg_result_destroy(result);
    txn->in_transaction = 0;
    txn->auto_commit = 1;
    txn->savepoint_count = 0;

    /* 归还独占连接 */
    if (txn->conn != NULL && txn->pool != NULL) {
        pg_pool_checkin_exclusive(txn->pool, txn->conn);
        txn->conn = NULL;
    }

    PG_LOG_INFO("[TRANSACTION] Transaction committed");
    return 0;
}

/* 回滚事务 */
int pg_transaction_rollback(pg_transaction_t *txn) {
    if (txn == NULL) {
        return -1;
    }

    if (!txn->in_transaction) {
        return -1; /* 不在事务中 */
    }

    /* 在独占连接上执行 ROLLBACK */
    pg_result_t *result = exec_on_conn(txn->conn, "ROLLBACK", NULL, 0);
    if (result != NULL) {
        pg_result_destroy(result);
    }

    txn->in_transaction = 0;
    txn->auto_commit = 1;
    txn->savepoint_count = 0;

    /* 归还独占连接 */
    if (txn->conn != NULL && txn->pool != NULL) {
        pg_pool_checkin_exclusive(txn->pool, txn->conn);
        txn->conn = NULL;
    }

    PG_LOG_WARN("[TRANSACTION] Transaction rolled back");
    return 0;
}

/* 执行事务中的查询 */
pg_result_t* pg_transaction_exec(pg_transaction_t *txn, const char *sql, const char **params, int param_count) {
    if (txn == NULL || sql == NULL) {
        return NULL;
    }

    /* 如果不在事务中，自动开始 */
    if (!txn->in_transaction) {
        if (pg_transaction_begin(txn) != 0) {
            return NULL;
        }
    }

    /* 在独占连接上执行 */
    return exec_on_conn(txn->conn, sql, params, param_count);
}

/* 创建保存点 */
int pg_transaction_savepoint(pg_transaction_t *txn, const char *name) {
    if (txn == NULL || name == NULL) {
        return -1;
    }

    if (!txn->in_transaction) {
        return -1; /* 不在事务中 */
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "SAVEPOINT %s", name);

    pg_result_t *result = exec_on_conn(txn->conn, sql, NULL, 0);
    if (result == NULL) {
        return -1;
    }

    int is_ok = !pg_result_is_error(result);
    pg_result_destroy(result);

    if (is_ok) {
        txn->savepoint_count++;
    }

    return is_ok ? 0 : -1;
}

/* 回滚到保存点 */
int pg_transaction_rollback_to_savepoint(pg_transaction_t *txn, const char *name) {
    if (txn == NULL || name == NULL) {
        return -1;
    }

    if (!txn->in_transaction) {
        return -1; /* 不在事务中 */
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "ROLLBACK TO SAVEPOINT %s", name);

    pg_result_t *result = exec_on_conn(txn->conn, sql, NULL, 0);
    if (result == NULL) {
        return -1;
    }

    int is_ok = !pg_result_is_error(result);
    pg_result_destroy(result);

    if (is_ok && txn->savepoint_count > 0) {
        txn->savepoint_count--;
    }

    return is_ok ? 0 : -1;
}

/* 释放保存点 */
int pg_transaction_release_savepoint(pg_transaction_t *txn, const char *name) {
    if (txn == NULL || name == NULL) {
        return -1;
    }

    if (!txn->in_transaction) {
        return -1; /* 不在事务中 */
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "RELEASE SAVEPOINT %s", name);

    pg_result_t *result = exec_on_conn(txn->conn, sql, NULL, 0);
    if (result == NULL) {
        return -1;
    }

    int is_ok = !pg_result_is_error(result);
    pg_result_destroy(result);

    if (is_ok && txn->savepoint_count > 0) {
        txn->savepoint_count--;
    }

    return is_ok ? 0 : -1;
}

/* 是否在事务中 */
int pg_transaction_is_active(const pg_transaction_t *txn) {
    if (txn == NULL) {
        return 0;
    }
    return txn->in_transaction;
}
