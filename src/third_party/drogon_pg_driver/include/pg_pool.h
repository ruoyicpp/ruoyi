#ifndef PG_POOL_H
#define PG_POOL_H

#include "pg_connection.h"
#include "pg_pool_wait_queue.h"
#include "pg_pool_monitor.h"
#include "pg_mutex.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 连接池对象 */
typedef struct pg_pool {
    pg_connection_t **connections;
    int capacity;
    int size;
    int min_size;
    int max_size;
    int checkout_timeout_ms;
    char *connection_string;
    int is_fallback;
    pg_pool_wait_queue_t *wait_queue;
    pg_pool_monitor_t *monitor;
    int *connection_lease_ids;  /* 每个连接的租约 ID */
    pg_mutex_t mutex;           /* 保护并发 checkout/checkin */
} pg_pool_t;

/* 创建连接池 */
pg_pool_t* pg_pool_create(const char *conn_str, int min_size, int max_size, int checkout_timeout_ms, int is_fallback);

/* 销毁连接池 */
void pg_pool_destroy(pg_pool_t *pool);

/* 初始化连接池 */
int pg_pool_init(pg_pool_t *pool);

/* 获取连接（阻塞） */
pg_connection_t* pg_pool_checkout(pg_pool_t *pool);

/* 获取连接（非阻塞，超时返回 NULL） */
pg_connection_t* pg_pool_checkout_timeout(pg_pool_t *pool, int timeout_ms);

/* 归还连接 */
void pg_pool_checkin(pg_pool_t *pool, pg_connection_t *conn);

/* 获取独占连接（用于事务） */
pg_connection_t* pg_pool_checkout_exclusive(pg_pool_t *pool);

/* 归还独占连接 */
void pg_pool_checkin_exclusive(pg_pool_t *pool, pg_connection_t *conn);

/* 获取连接池大小 */
int pg_pool_size(const pg_pool_t *pool);

/* 获取空闲连接数 */
int pg_pool_idle_count(const pg_pool_t *pool);

/* 扩容连接池 */
int pg_pool_expand(pg_pool_t *pool, int target_size);

/* 缩容连接池 */
int pg_pool_shrink(pg_pool_t *pool, int target_size);

/* 健康检查所有连接 */
int pg_pool_health_check(pg_pool_t *pool);

/* 是否为降级连接池 */
int pg_pool_is_fallback(const pg_pool_t *pool);

/* 获取等待队列大小 */
int pg_pool_get_wait_queue_size(const pg_pool_t *pool);

/* 获取监控统计 */
pg_pool_stats_t pg_pool_get_stats(const pg_pool_t *pool);

/* 检查连接泄漏 */
int pg_pool_check_leaks(pg_pool_t *pool);

/* 设置泄漏超时 */
void pg_pool_set_leak_timeout(pg_pool_t *pool, int timeout_sec);

/* 获取当前使用中的连接数 */
int pg_pool_in_use_count(const pg_pool_t *pool);

/* 获取等待中的请求数 */
int pg_pool_waiting_count(const pg_pool_t *pool);

/* 处理等待队列（归还连接时调用） */
void pg_pool_process_wait_queue(pg_pool_t *pool, pg_connection_t *conn);

/* 预热连接池（创建 min_size 个连接） */
int pg_pool_warmup(pg_pool_t *pool);

/* 获取可用连接数 */
int pg_pool_available_count(const pg_pool_t *pool);

/* 检查连接池是否已满 */
int pg_pool_is_full(const pg_pool_t *pool);

/* 检查连接池是否为空 */
int pg_pool_is_empty(const pg_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* PG_POOL_H */
