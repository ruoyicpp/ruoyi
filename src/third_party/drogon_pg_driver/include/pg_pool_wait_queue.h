#ifndef PG_POOL_WAIT_QUEUE_H
#define PG_POOL_WAIT_QUEUE_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 等待者对象 */
typedef struct pg_pool_waiter {
    void *promise;              /* Drogon promise */
    int timeout_ms;
    time_t start_time;
    int waiter_id;
    struct pg_pool_waiter *next;
} pg_pool_waiter_t;

/* 等待队列 */
typedef struct pg_pool_wait_queue {
    pg_pool_waiter_t *head;
    pg_pool_waiter_t *tail;
    int size;
    int max_size;
    int total_waiters;
    int total_timeouts;
    int total_served;
} pg_pool_wait_queue_t;

/* 创建等待队列 */
pg_pool_wait_queue_t* pg_pool_wait_queue_create(int max_size);

/* 销毁等待队列 */
void pg_pool_wait_queue_destroy(pg_pool_wait_queue_t *queue);

/* 添加等待者 */
int pg_pool_wait_queue_push(pg_pool_wait_queue_t *queue, void *promise, int timeout_ms);

/* 弹出等待者 */
pg_pool_waiter_t* pg_pool_wait_queue_pop(pg_pool_wait_queue_t *queue);

/* 查看队首等待者 */
pg_pool_waiter_t* pg_pool_wait_queue_peek(pg_pool_wait_queue_t *queue);

/* 检查超时 */
int pg_pool_wait_queue_check_timeouts(pg_pool_wait_queue_t *queue);

/* 获取队列大小 */
int pg_pool_wait_queue_size(const pg_pool_wait_queue_t *queue);

/* 获取超时数量 */
int pg_pool_wait_queue_get_timeout_count(const pg_pool_wait_queue_t *queue);

/* 获取服务数量 */
int pg_pool_wait_queue_get_served_count(const pg_pool_wait_queue_t *queue);

/* 清空队列 */
void pg_pool_wait_queue_clear(pg_pool_wait_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* PG_POOL_WAIT_QUEUE_H */
