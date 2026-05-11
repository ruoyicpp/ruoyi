#include "pg_mutex.h"
#include <stdlib.h>

/* 创建互斥锁 */
int pg_mutex_init(pg_mutex_t *mutex) {
    if (mutex == NULL) {
        return -1;
    }

#ifdef _WIN32
    InitializeCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

/* 销毁互斥锁 */
int pg_mutex_destroy(pg_mutex_t *mutex) {
    if (mutex == NULL) {
        return -1;
    }

#ifdef _WIN32
    DeleteCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_destroy(mutex);
#endif
}

/* 加锁 */
int pg_mutex_lock(pg_mutex_t *mutex) {
    if (mutex == NULL) {
        return -1;
    }

#ifdef _WIN32
    EnterCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_lock(mutex);
#endif
}

/* 尝试加锁 */
int pg_mutex_trylock(pg_mutex_t *mutex) {
    if (mutex == NULL) {
        return -1;
    }

#ifdef _WIN32
    return TryEnterCriticalSection(mutex) ? 0 : -1;
#else
    return pthread_mutex_trylock(mutex);
#endif
}

/* 解锁 */
int pg_mutex_unlock(pg_mutex_t *mutex) {
    if (mutex == NULL) {
        return -1;
    }

#ifdef _WIN32
    LeaveCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_unlock(mutex);
#endif
}

/* 创建自动锁 */
pg_mutex_guard_t pg_mutex_guard_create(pg_mutex_t *mutex) {
    pg_mutex_guard_t guard = {NULL, 0};

    if (mutex == NULL) {
        return guard;
    }

    guard.mutex = mutex;
    pg_mutex_lock(mutex);
    guard.locked = 1;

    return guard;
}

/* 释放自动锁 */
void pg_mutex_guard_destroy(pg_mutex_guard_t *guard) {
    if (guard == NULL || !guard->locked) {
        return;
    }

    pg_mutex_unlock(guard->mutex);
    guard->locked = 0;
}
