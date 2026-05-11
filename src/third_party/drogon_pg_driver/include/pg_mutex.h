#ifndef PG_MUTEX_H
#define PG_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

/* 跨平台互斥锁 */
#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION pg_mutex_t;
#else
#include <pthread.h>
typedef pthread_mutex_t pg_mutex_t;
#endif

/* 创建互斥锁 */
int pg_mutex_init(pg_mutex_t *mutex);

/* 销毁互斥锁 */
int pg_mutex_destroy(pg_mutex_t *mutex);

/* 加锁 */
int pg_mutex_lock(pg_mutex_t *mutex);

/* 尝试加锁 */
int pg_mutex_trylock(pg_mutex_t *mutex);

/* 解锁 */
int pg_mutex_unlock(pg_mutex_t *mutex);

/* 自动锁（RAII 风格） */
typedef struct pg_mutex_guard {
    pg_mutex_t *mutex;
    int locked;
} pg_mutex_guard_t;

/* 创建自动锁 */
pg_mutex_guard_t pg_mutex_guard_create(pg_mutex_t *mutex);

/* 释放自动锁 */
void pg_mutex_guard_destroy(pg_mutex_guard_t *guard);

#ifdef __cplusplus
}
#endif

#endif /* PG_MUTEX_H */
