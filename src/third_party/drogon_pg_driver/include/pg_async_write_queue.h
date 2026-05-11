#ifndef PG_ASYNC_WRITE_QUEUE_H
#define PG_ASYNC_WRITE_QUEUE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 写队列项 */
typedef struct pg_write_item {
    char *sql;
    char **params;
    int param_count;
    void *promise; /* 可选的 promise 对象 */
    struct pg_write_item *next;
} pg_write_item_t;

/* 异步写队列 */
typedef struct pg_async_write_queue {
    pg_write_item_t *head;
    pg_write_item_t *tail;
    int size;
    int capacity;
    int batch_size;
    int dropped_count;
} pg_async_write_queue_t;

/* 创建异步写队列 */
pg_async_write_queue_t* pg_async_write_queue_create(int capacity, int batch_size);

/* 销毁异步写队列 */
void pg_async_write_queue_destroy(pg_async_write_queue_t *queue);

/* 添加写请求 */
int pg_async_write_queue_push(pg_async_write_queue_t *queue, const char *sql, const char **params, int param_count, void *promise);

/* 批量执行写请求 */
int pg_async_write_queue_flush(pg_async_write_queue_t *queue);

/* 获取队列大小 */
int pg_async_write_queue_size(const pg_async_write_queue_t *queue);

/* 获取丢弃数量 */
int pg_async_write_queue_get_dropped_count(const pg_async_write_queue_t *queue);

/* 清空队列 */
void pg_async_write_queue_clear(pg_async_write_queue_t *queue);

/* 重置丢弃计数 */
void pg_async_write_queue_reset_dropped_count(pg_async_write_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* PG_ASYNC_WRITE_QUEUE_H */
