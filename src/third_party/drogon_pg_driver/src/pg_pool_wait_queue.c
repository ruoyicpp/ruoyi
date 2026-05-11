#include "pg_pool_wait_queue.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 创建等待队列 */
pg_pool_wait_queue_t* pg_pool_wait_queue_create(int max_size) {
    pg_pool_wait_queue_t *queue = (pg_pool_wait_queue_t*)malloc(sizeof(pg_pool_wait_queue_t));
    if (queue == NULL) {
        return NULL;
    }

    memset(queue, 0, sizeof(pg_pool_wait_queue_t));

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->max_size = max_size > 0 ? max_size : 1000;
    queue->total_waiters = 0;
    queue->total_timeouts = 0;
    queue->total_served = 0;

    return queue;
}

/* 销毁等待队列 */
void pg_pool_wait_queue_destroy(pg_pool_wait_queue_t *queue) {
    if (queue == NULL) {
        return;
    }

    pg_pool_wait_queue_clear(queue);
    free(queue);
}

/* 添加等待者 */
int pg_pool_wait_queue_push(pg_pool_wait_queue_t *queue, void *promise, int timeout_ms) {
    if (queue == NULL) {
        return -1;
    }

    /* 队列已满 */
    if (queue->size >= queue->max_size) {
        return -1;
    }

    pg_pool_waiter_t *waiter = (pg_pool_waiter_t*)malloc(sizeof(pg_pool_waiter_t));
    if (waiter == NULL) {
        return -1;
    }

    memset(waiter, 0, sizeof(pg_pool_waiter_t));

    waiter->promise = promise;
    waiter->timeout_ms = timeout_ms;
    waiter->start_time = time(NULL);
    waiter->waiter_id = queue->total_waiters++;
    waiter->next = NULL;

    /* 添加到队列尾部 */
    if (queue->tail != NULL) {
        queue->tail->next = waiter;
    }
    queue->tail = waiter;

    if (queue->head == NULL) {
        queue->head = waiter;
    }

    queue->size++;

    return waiter->waiter_id;
}

/* 弹出等待者 */
pg_pool_waiter_t* pg_pool_wait_queue_pop(pg_pool_wait_queue_t *queue) {
    if (queue == NULL || queue->head == NULL) {
        return NULL;
    }

    pg_pool_waiter_t *waiter = queue->head;
    queue->head = waiter->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    queue->size--;
    queue->total_served++;

    return waiter;
}

/* 查看队首等待者 */
pg_pool_waiter_t* pg_pool_wait_queue_peek(pg_pool_wait_queue_t *queue) {
    if (queue == NULL) {
        return NULL;
    }
    return queue->head;
}

/* 检查超时 */
int pg_pool_wait_queue_check_timeouts(pg_pool_wait_queue_t *queue) {
    if (queue == NULL) {
        return 0;
    }

    time_t now = time(NULL);
    int timeout_count = 0;

    pg_pool_waiter_t *prev = NULL;
    pg_pool_waiter_t *curr = queue->head;

    while (curr != NULL) {
        int elapsed_ms = (int)(now - curr->start_time) * 1000;

        if (elapsed_ms >= curr->timeout_ms) {
            /* 超时，从队列中移除 */
            pg_pool_waiter_t *timeout_waiter = curr;

            if (prev == NULL) {
                queue->head = curr->next;
            } else {
                prev->next = curr->next;
            }

            if (curr == queue->tail) {
                queue->tail = prev;
            }

            curr = curr->next;
            queue->size--;
            queue->total_timeouts++;
            timeout_count++;

            /* 释放超时等待者 */
            free(timeout_waiter);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    return timeout_count;
}

/* 获取队列大小 */
int pg_pool_wait_queue_size(const pg_pool_wait_queue_t *queue) {
    if (queue == NULL) {
        return 0;
    }
    return queue->size;
}

/* 获取超时数量 */
int pg_pool_wait_queue_get_timeout_count(const pg_pool_wait_queue_t *queue) {
    if (queue == NULL) {
        return 0;
    }
    return queue->total_timeouts;
}

/* 获取服务数量 */
int pg_pool_wait_queue_get_served_count(const pg_pool_wait_queue_t *queue) {
    if (queue == NULL) {
        return 0;
    }
    return queue->total_served;
}

/* 清空队列 */
void pg_pool_wait_queue_clear(pg_pool_wait_queue_t *queue) {
    if (queue == NULL) {
        return;
    }

    pg_pool_waiter_t *curr = queue->head;
    while (curr != NULL) {
        pg_pool_waiter_t *next = curr->next;
        free(curr);
        curr = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}
