#include "pg_circuit_breaker.h"
#include "pg_config.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 创建熔断器 */
pg_circuit_breaker_t* pg_circuit_breaker_create(void) {
    pg_circuit_breaker_t *cb = (pg_circuit_breaker_t*)malloc(sizeof(pg_circuit_breaker_t));
    if (cb == NULL) {
        return NULL;
    }

    memset(cb, 0, sizeof(pg_circuit_breaker_t));

    cb->state = PG_CB_CLOSED;
    cb->failure_count = 0;
    cb->success_count = 0;
    cb->consecutive_failures = 0;
    cb->consecutive_successes = 0;
    cb->last_failure_time = 0;
    cb->last_state_change_time = time(NULL);
    cb->half_open_attempts = 0;

    pg_config_t *config = pg_config_get_instance();
    cb->failure_threshold = pg_config_get_failure_threshold(config);
    cb->recovery_threshold = pg_config_get_recovery_threshold(config);
    cb->half_open_max_attempts = 3; /* 默认允许 3 次试探 */

    return cb;
}

/* 销毁熔断器 */
void pg_circuit_breaker_destroy(pg_circuit_breaker_t *cb) {
    if (cb == NULL) {
        return;
    }
    free(cb);
}

/* 记录成功 */
void pg_circuit_breaker_record_success(pg_circuit_breaker_t *cb) {
    if (cb == NULL) {
        return;
    }

    cb->success_count++;
    cb->consecutive_failures = 0;
    cb->consecutive_successes++;

    /* HALF_OPEN 状态下，连续成功达到阈值则恢复 CLOSED */
    if (cb->state == PG_CB_HALF_OPEN) {
        cb->half_open_attempts++;
        if (cb->consecutive_successes >= cb->recovery_threshold) {
            cb->state = PG_CB_CLOSED;
            cb->consecutive_successes = 0;
            cb->half_open_attempts = 0;
            cb->last_state_change_time = time(NULL);
        }
    }
}

/* 记录失败 */
void pg_circuit_breaker_record_failure(pg_circuit_breaker_t *cb) {
    if (cb == NULL) {
        return;
    }

    cb->failure_count++;
    cb->consecutive_failures++;
    cb->consecutive_successes = 0;
    cb->last_failure_time = time(NULL);

    /* CLOSED 状态下，连续失败达到阈值则切换到 OPEN */
    if (cb->state == PG_CB_CLOSED) {
        if (cb->consecutive_failures >= cb->failure_threshold) {
            cb->state = PG_CB_OPEN;
            cb->consecutive_failures = 0;
            cb->last_state_change_time = time(NULL);
        }
    }
    /* HALF_OPEN 状态下，失败则立即切换回 OPEN */
    else if (cb->state == PG_CB_HALF_OPEN) {
        cb->state = PG_CB_OPEN;
        cb->half_open_attempts = 0;
        cb->last_state_change_time = time(NULL);
    }
}

/* 获取当前状态 */
pg_cb_state_t pg_circuit_breaker_get_state(const pg_circuit_breaker_t *cb) {
    if (cb == NULL) {
        return PG_CB_OPEN;
    }
    return cb->state;
}

/* 是否允许请求 */
int pg_circuit_breaker_allow_request(const pg_circuit_breaker_t *cb) {
    if (cb == NULL) {
        return 0;
    }

    /* CLOSED 状态允许所有请求 */
    if (cb->state == PG_CB_CLOSED) {
        return 1;
    }

    /* OPEN 状态不允许请求 */
    if (cb->state == PG_CB_OPEN) {
        return 0;
    }

    /* HALF_OPEN 状态允许有限次数的试探请求 */
    if (cb->state == PG_CB_HALF_OPEN) {
        return cb->half_open_attempts < cb->half_open_max_attempts;
    }

    return 0;
}

/* 获取连续失败次数 */
int pg_circuit_breaker_get_consecutive_failures(const pg_circuit_breaker_t *cb) {
    if (cb == NULL) {
        return 0;
    }
    return cb->consecutive_failures;
}

/* 获取连续成功次数 */
int pg_circuit_breaker_get_consecutive_successes(const pg_circuit_breaker_t *cb) {
    if (cb == NULL) {
        return 0;
    }
    return cb->consecutive_successes;
}

/* 重置统计 */
void pg_circuit_breaker_reset_stats(pg_circuit_breaker_t *cb) {
    if (cb == NULL) {
        return;
    }

    cb->failure_count = 0;
    cb->success_count = 0;
    cb->consecutive_failures = 0;
    cb->consecutive_successes = 0;
    cb->half_open_attempts = 0;
}

/* 获取状态名称 */
const char* pg_circuit_breaker_state_to_string(pg_cb_state_t state) {
    switch (state) {
        case PG_CB_CLOSED:
            return "CLOSED";
        case PG_CB_OPEN:
            return "OPEN";
        case PG_CB_HALF_OPEN:
            return "HALF_OPEN";
        default:
            return "UNKNOWN";
    }
}
