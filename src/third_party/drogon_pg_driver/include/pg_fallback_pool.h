#ifndef PG_FALLBACK_POOL_H
#define PG_FALLBACK_POOL_H

#include "pg_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 创建降级连接池 */
pg_pool_t* pg_fallback_pool_create(const char *conn_str);

/* 销毁降级连接池 */
void pg_fallback_pool_destroy(pg_pool_t *pool);

/* 初始化降级连接池 */
int pg_fallback_pool_init(pg_pool_t *pool);

/* 从降级连接池获取连接 */
pg_connection_t* pg_fallback_pool_checkout(pg_pool_t *pool);

/* 归还连接到降级连接池 */
void pg_fallback_pool_checkin(pg_pool_t *pool, pg_connection_t *conn);

/* 降级连接池健康检查 */
int pg_fallback_pool_health_check(pg_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* PG_FALLBACK_POOL_H */
