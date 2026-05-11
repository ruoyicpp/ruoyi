#ifndef PG_CIRCUIT_BREAKER_H
#define PG_CIRCUIT_BREAKER_H

#include "pg_config.h"
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 熔断器状态 */
typedef enum {
    PG_CB_CLOSED,        /* 正常状态 */
    PG_CB_OPEN,          /* 降级状态 */
    PG_CB_HALF_OPEN      /* 试探状态 */
} pg_cb_state_t;

/* 熔断器对象 */
typedef struct pg_circuit_breaker {
    pg_cb_state_t state;
    int failure_count;
    int success_count;
    int consecutive_failures;
    int consecutive_successes;
    time_t last_failure_time;
    time_t last_state_change_time;
    int failure_threshold;
    int recovery_threshold;
    int half_open_max_attempts;
    int half_open_attempts;
} pg_circuit_breaker_t;

/* 创建熔断器 */
pg_circuit_breaker_t* pg_circuit_breaker_create(void);

/* 销毁熔断器 */
void pg_circuit_breaker_destroy(pg_circuit_breaker_t *cb);

/* 记录成功 */
void pg_circuit_breaker_record_success(pg_circuit_breaker_t *cb);

/* 记录失败 */
void pg_circuit_breaker_record_failure(pg_circuit_breaker_t *cb);

/* 获取当前状态 */
pg_cb_state_t pg_circuit_breaker_get_state(const pg_circuit_breaker_t *cb);

/* 是否允许请求 */
int pg_circuit_breaker_allow_request(const pg_circuit_breaker_t *cb);

/* 获取连续失败次数 */
int pg_circuit_breaker_get_consecutive_failures(const pg_circuit_breaker_t *cb);

/* 获取连续成功次数 */
int pg_circuit_breaker_get_consecutive_successes(const pg_circuit_breaker_t *cb);

/* 重置统计 */
void pg_circuit_breaker_reset_stats(pg_circuit_breaker_t *cb);

/* 获取状态名称 */
const char* pg_circuit_breaker_state_to_string(pg_cb_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* PG_CIRCUIT_BREAKER_H */
