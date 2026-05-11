#include "pg_reconnect_backoff.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* 创建退避对象 */
pg_reconnect_backoff_t* pg_reconnect_backoff_create(int base_delay_sec, int max_delay_sec) {
    pg_reconnect_backoff_t *backoff = (pg_reconnect_backoff_t*)malloc(sizeof(pg_reconnect_backoff_t));
    if (backoff == NULL) {
        return NULL;
    }

    memset(backoff, 0, sizeof(pg_reconnect_backoff_t));

    backoff->attempt_count = 0;
    backoff->current_delay_sec = 0;
    backoff->base_delay_sec = base_delay_sec > 0 ? base_delay_sec : 1;
    backoff->max_delay_sec = max_delay_sec > 0 ? max_delay_sec : 60;
    backoff->multiplier = 2;
    backoff->next_retry_time = 0;
    backoff->jitter_percent = 10;

    return backoff;
}

/* 销毁退避对象 */
void pg_reconnect_backoff_destroy(pg_reconnect_backoff_t *backoff) {
    if (backoff == NULL) {
        return;
    }
    free(backoff);
}

/* 重置退避状态 */
void pg_reconnect_backoff_reset(pg_reconnect_backoff_t *backoff) {
    if (backoff == NULL) {
        return;
    }

    backoff->attempt_count = 0;
    backoff->current_delay_sec = 0;
    backoff->next_retry_time = 0;
}

/* 计算抖动 */
static int calculate_jitter(int delay, int jitter_percent) {
    if (jitter_percent <= 0) {
        return delay;
    }

    /* 随机抖动：±jitter_percent% */
    int jitter_range = delay * jitter_percent / 100;
    int jitter = (rand() % (jitter_range * 2 + 1)) - jitter_range;
    
    return delay + jitter;
}

/* 记录失败，计算下次重试时间 */
int pg_reconnect_backoff_record_failure(pg_reconnect_backoff_t *backoff) {
    if (backoff == NULL) {
        return -1;
    }

    backoff->attempt_count++;

    /* 计算延迟：base * multiplier^attempt */
    int delay = backoff->base_delay_sec;
    for (int i = 1; i < backoff->attempt_count; i++) {
        delay *= backoff->multiplier;
    }

    /* 限制最大延迟 */
    if (delay > backoff->max_delay_sec) {
        delay = backoff->max_delay_sec;
    }

    /* 添加抖动 */
    delay = calculate_jitter(delay, backoff->jitter_percent);
    if (delay < 1) {
        delay = 1;
    }

    backoff->current_delay_sec = delay;
    backoff->next_retry_time = time(NULL) + delay;

    return delay;
}

/* 记录成功，重置退避 */
void pg_reconnect_backoff_record_success(pg_reconnect_backoff_t *backoff) {
    if (backoff == NULL) {
        return;
    }

    pg_reconnect_backoff_reset(backoff);
}

/* 获取当前延迟 */
int pg_reconnect_backoff_get_delay(const pg_reconnect_backoff_t *backoff) {
    if (backoff == NULL) {
        return 0;
    }
    return backoff->current_delay_sec;
}

/* 获取下次重试时间 */
time_t pg_reconnect_backoff_get_next_retry_time(const pg_reconnect_backoff_t *backoff) {
    if (backoff == NULL) {
        return 0;
    }
    return backoff->next_retry_time;
}

/* 判断是否可以重试 */
int pg_reconnect_backoff_can_retry(const pg_reconnect_backoff_t *backoff) {
    if (backoff == NULL) {
        return 0;
    }

    time_t now = time(NULL);
    return now >= backoff->next_retry_time;
}

/* 等待到下次重试时间 */
void pg_reconnect_backoff_wait(const pg_reconnect_backoff_t *backoff) {
    if (backoff == NULL) {
        return;
    }

    time_t now = time(NULL);
    time_t wait_seconds = backoff->next_retry_time - now;

    if (wait_seconds > 0) {
#ifdef _WIN32
        Sleep((DWORD)(wait_seconds * 1000));
#else
        sleep(wait_seconds);
#endif
    }
}

/* 获取尝试次数 */
int pg_reconnect_backoff_get_attempt_count(const pg_reconnect_backoff_t *backoff) {
    if (backoff == NULL) {
        return 0;
    }
    return backoff->attempt_count;
}

/* 设置抖动百分比 */
void pg_reconnect_backoff_set_jitter(pg_reconnect_backoff_t *backoff, int jitter_percent) {
    if (backoff == NULL) {
        return;
    }
    backoff->jitter_percent = jitter_percent > 0 ? jitter_percent : 0;
}

/* 设置延迟倍数 */
void pg_reconnect_backoff_set_multiplier(pg_reconnect_backoff_t *backoff, int multiplier) {
    if (backoff == NULL || multiplier <= 1) {
        return;
    }
    backoff->multiplier = multiplier;
}
