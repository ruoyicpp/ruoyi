#ifndef PG_TRANSACTION_H
#define PG_TRANSACTION_H

#include "pg_result.h"
#include "pg_connection.h"
#include "pg_pool.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 事务对象 */
typedef struct pg_transaction {
    int in_transaction;
    int auto_commit;
    int savepoint_count;
    pg_connection_t *conn;       /* 独占连接 */
    pg_pool_t *pool;             /* 所属连接池 */
} pg_transaction_t;

/* 创建事务对象 */
pg_transaction_t* pg_transaction_create(void);

/* 创建事务对象并绑定连接池 */
pg_transaction_t* pg_transaction_create_with_pool(pg_pool_t *pool);

/* 销毁事务对象 */
void pg_transaction_destroy(pg_transaction_t *txn);

/* 绑定连接池 */
void pg_transaction_set_pool(pg_transaction_t *txn, pg_pool_t *pool);

/* 开始事务 */
int pg_transaction_begin(pg_transaction_t *txn);

/* 提交事务 */
int pg_transaction_commit(pg_transaction_t *txn);

/* 回滚事务 */
int pg_transaction_rollback(pg_transaction_t *txn);

/* 执行事务中的查询 */
pg_result_t* pg_transaction_exec(pg_transaction_t *txn, const char *sql, const char **params, int param_count);

/* 创建保存点 */
int pg_transaction_savepoint(pg_transaction_t *txn, const char *name);

/* 回滚到保存点 */
int pg_transaction_rollback_to_savepoint(pg_transaction_t *txn, const char *name);

/* 释放保存点 */
int pg_transaction_release_savepoint(pg_transaction_t *txn, const char *name);

/* 是否在事务中 */
int pg_transaction_is_active(const pg_transaction_t *txn);

#ifdef __cplusplus
}
#endif

#endif /* PG_TRANSACTION_H */
