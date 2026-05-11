#include "pg_fallback_pool.h"
#include "pg_config.h"
#include <stdlib.h>

/* 创建降级连接池 */
pg_pool_t* pg_fallback_pool_create(const char *conn_str) {
    pg_config_t *config = pg_config_get_instance();

    int min_size = pg_config_get_fallback_pool_min_size(config);
    int max_size = pg_config_get_fallback_pool_max_size(config);
    int timeout_ms = pg_config_get_pool_checkout_timeout_ms(config);

    return pg_pool_create(conn_str, min_size, max_size, timeout_ms, 1); /* is_fallback = 1 */
}

/* 销毁降级连接池 */
void pg_fallback_pool_destroy(pg_pool_t *pool) {
    pg_pool_destroy(pool);
}

/* 初始化降级连接池 */
int pg_fallback_pool_init(pg_pool_t *pool) {
    return pg_pool_init(pool);
}

/* 从降级连接池获取连接 */
pg_connection_t* pg_fallback_pool_checkout(pg_pool_t *pool) {
    return pg_pool_checkout(pool);
}

/* 归还连接到降级连接池 */
void pg_fallback_pool_checkin(pg_pool_t *pool, pg_connection_t *conn) {
    pg_pool_checkin(pool, conn);
}

/* 降级连接池健康检查 */
int pg_fallback_pool_health_check(pg_pool_t *pool) {
    return pg_pool_health_check(pool);
}
