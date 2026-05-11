#ifndef PG_RECONNECT_BACKOFF_H
#define PG_RECONNECT_BACKOFF_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 指数退避配置 */
typedef struct pg_reconnect_backoff {
    int attempt_count;          /* 尝试次数 */
    int current_delay_sec;      /* 当前延迟（秒） */
    int base_delay_sec;         /* 基础延迟（秒） */
    int max_delay_sec;          /* 最大延迟（秒） */
    int multiplier;             /* 延迟倍数 */
    time_t next_retry_time;     /* 下次重试时间 */
    int jitter_percent;         /* 抖动百分比 */
} pg_reconnect_backoff_t;

/* 创建退避对象 */
pg_reconnect_backoff_t* pg_reconnect_backoff_create(int base_delay_sec, int max_delay_sec);

/* 销毁退避对象 */
void pg_reconnect_backoff_destroy(pg_reconnect_backoff_t *backoff);

/* 重置退避状态 */
void pg_reconnect_backoff_reset(pg_reconnect_backoff_t *backoff);

/* 记录失败，计算下次重试时间 */
int pg_reconnect_backoff_record_failure(pg_reconnect_backoff_t *backoff);

/* 记录成功，重置退避 */
void pg_reconnect_backoff_record_success(pg_reconnect_backoff_t *backoff);

/* 获取当前延迟 */
int pg_reconnect_backoff_get_delay(const pg_reconnect_backoff_t *backoff);

/* 获取下次重试时间 */
time_t pg_reconnect_backoff_get_next_retry_time(const pg_reconnect_backoff_t *backoff);

/* 判断是否可以重试 */
int pg_reconnect_backoff_can_retry(const pg_reconnect_backoff_t *backoff);

/* 等待到下次重试时间 */
void pg_reconnect_backoff_wait(const pg_reconnect_backoff_t *backoff);

/* 获取尝试次数 */
int pg_reconnect_backoff_get_attempt_count(const pg_reconnect_backoff_t *backoff);

/* 设置抖动百分比 */
void pg_reconnect_backoff_set_jitter(pg_reconnect_backoff_t *backoff, int jitter_percent);

/* 设置延迟倍数 */
void pg_reconnect_backoff_set_multiplier(pg_reconnect_backoff_t *backoff, int multiplier);

#ifdef __cplusplus
}
#endif

#endif /* PG_RECONNECT_BACKOFF_H */
