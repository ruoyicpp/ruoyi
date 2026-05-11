#include "pg_async_write_queue.h"
#include "pg_client.h"
#include <stdlib.h>
#include <string.h>

/* 创建写队列项 */
static pg_write_item_t* write_item_create(const char *sql, const char **params, int param_count, void *promise) {
    pg_write_item_t *item = (pg_write_item_t*)malloc(sizeof(pg_write_item_t));
    if (item == NULL) {
        return NULL;
    }

    item->sql = sql ? strdup(sql) : NULL;
    item->param_count = param_count;
    item->promise = promise;
    item->next = NULL;

    if (sql && !item->sql) {
        free(item);
        return NULL;
    }

    /* 复制参数 */
    if (params != NULL && param_count > 0) {
        item->params = (char**)malloc(param_count * sizeof(char*));
        if (item->params == NULL) {
            if (item->sql) free(item->sql);
            free(item);
            return NULL;
        }

        for (int i = 0; i < param_count; i++) {
            item->params[i] = params[i] ? strdup(params[i]) : NULL;
        }
    } else {
        item->params = NULL;
    }

    return item;
}

/* 销毁写队列项 */
static void write_item_destroy(pg_write_item_t *item) {
    if (item == NULL) {
        return;
    }

    if (item->sql != NULL) {
        free(item->sql);
        item->sql = NULL;
    }

    if (item->params != NULL) {
        for (int i = 0; i < item->param_count; i++) {
            if (item->params[i] != NULL) {
                free(item->params[i]);
                item->params[i] = NULL;
            }
        }
        free(item->params);
        item->params = NULL;
    }

    free(item);
}

/* 创建异步写队列 */
pg_async_write_queue_t* pg_async_write_queue_create(int capacity, int batch_size) {
    pg_async_write_queue_t *queue = (pg_async_write_queue_t*)malloc(sizeof(pg_async_write_queue_t));
    if (queue == NULL) {
        return NULL;
    }

    memset(queue, 0, sizeof(pg_async_write_queue_t));

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->capacity = capacity > 0 ? capacity : 50000;
    queue->batch_size = batch_size > 0 ? batch_size : 200;
    queue->dropped_count = 0;

    return queue;
}

/* 销毁异步写队列 */
void pg_async_write_queue_destroy(pg_async_write_queue_t *queue) {
    if (queue == NULL) {
        return;
    }

    pg_async_write_queue_clear(queue);
    free(queue);
}

/* 添加写请求 */
int pg_async_write_queue_push(pg_async_write_queue_t *queue, const char *sql, const char **params, int param_count, void *promise) {
    if (queue == NULL || sql == NULL) {
        return -1;
    }

    /* 队列已满，丢弃请求 */
    if (queue->size >= queue->capacity) {
        queue->dropped_count++;
        return -1;
    }

    pg_write_item_t *item = write_item_create(sql, params, param_count, promise);
    if (item == NULL) {
        return -1;
    }

    /* 添加到队列尾部 */
    if (queue->tail != NULL) {
        queue->tail->next = item;
    }
    queue->tail = item;

    if (queue->head == NULL) {
        queue->head = item;
    }

    queue->size++;

    /* 达到批量大小，自动刷新 */
    if (queue->size >= queue->batch_size) {
        pg_async_write_queue_flush(queue);
    }

    return 0;
}

/* 批量执行写请求 */
int pg_async_write_queue_flush(pg_async_write_queue_t *queue) {
    if (queue == NULL) {
        return -1;
    }

    if (queue->size == 0) {
        return 0;
    }

    /* 开始事务 */
    if (pg_client_begin_transaction() != 0) {
        return -1;
    }

    int executed = 0;
    pg_write_item_t *item = queue->head;

    while (item != NULL) {
        pg_result_t *result = pg_client_exec(item->sql, (const char **)item->params, item->param_count);
        if (result != NULL) {
            pg_result_destroy(result);
            executed++;
        }

        pg_write_item_t *next = item->next;
        write_item_destroy(item);
        item = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;

    /* 提交事务 */
    if (executed > 0) {
        pg_client_commit_transaction();
    } else {
        pg_client_rollback_transaction();
    }

    return executed;
}

/* 获取队列大小 */
int pg_async_write_queue_size(const pg_async_write_queue_t *queue) {
    if (queue == NULL) {
        return 0;
    }
    return queue->size;
}

/* 获取丢弃数量 */
int pg_async_write_queue_get_dropped_count(const pg_async_write_queue_t *queue) {
    if (queue == NULL) {
        return 0;
    }
    return queue->dropped_count;
}

/* 清空队列 */
void pg_async_write_queue_clear(pg_async_write_queue_t *queue) {
    if (queue == NULL) {
        return;
    }

    pg_write_item_t *item = queue->head;
    while (item != NULL) {
        pg_write_item_t *next = item->next;
        write_item_destroy(item);
        item = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}

/* 重置丢弃计数 */
void pg_async_write_queue_reset_dropped_count(pg_async_write_queue_t *queue) {
    if (queue == NULL) {
        return;
    }
    queue->dropped_count = 0;
}
