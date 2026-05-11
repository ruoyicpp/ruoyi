#ifndef PG_CONFIG_H
#define PG_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PG_DRIVER_MODE_NATIVE,     /* 正常模式：使用自研驱动层 */
    PG_DRIVER_MODE_FALLBACK,   /* 降级模式：使用 FallbackPool */
    PG_DRIVER_MODE_AUTO        /* 自动模式：Circuit Breaker 自动切换 */
} pg_driver_mode_t;

typedef enum {
    PG_CB_STATE_CLOSED,        /* 正常状态 */
    PG_CB_STATE_OPEN,          /* 降级状态 */
    PG_CB_STATE_HALF_OPEN      /* 试探状态 */
} pg_circuit_breaker_state_t;

typedef struct pg_config {
    char *connection_string;       /* 连接字符串 */
    pg_driver_mode_t driver_mode;   /* 驱动模式 */

    /* 主连接池配置 */
    int pool_min_size;
    int pool_init_size;
    int pool_max_size;
    int pool_checkout_timeout_ms;

    /* 降级连接池配置 */
    int fallback_pool_min_size;
    int fallback_pool_init_size;
    int fallback_pool_max_size;

    /* 预编译语句缓存 */
    int stmt_cache_size;

    /* 健康监控 */
    int health_check_interval_sec;
    int slow_query_threshold_ms;
    int reconnect_backoff_max_sec;

    /* Circuit Breaker */
    int failure_threshold;
    int recovery_threshold;

    /* 异步写队列 */
    int async_write_queue_size;
    int async_write_batch_size;
} pg_config_t;

/* 获取配置单例 */
pg_config_t* pg_config_get_instance(void);

/* 连接字符串 */
void pg_config_set_connection_string(pg_config_t *config, const char *conn_str);
const char* pg_config_get_connection_string(const pg_config_t *config);

/* 驱动模式 */
void pg_config_set_driver_mode(pg_config_t *config, pg_driver_mode_t mode);
pg_driver_mode_t pg_config_get_driver_mode(const pg_config_t *config);

/* 主连接池配置 */
void pg_config_set_pool_min_size(pg_config_t *config, int size);
void pg_config_set_pool_init_size(pg_config_t *config, int size);
void pg_config_set_pool_max_size(pg_config_t *config, int size);
void pg_config_set_pool_checkout_timeout_ms(pg_config_t *config, int timeout_ms);

int pg_config_get_pool_min_size(const pg_config_t *config);
int pg_config_get_pool_init_size(const pg_config_t *config);
int pg_config_get_pool_max_size(const pg_config_t *config);
int pg_config_get_pool_checkout_timeout_ms(const pg_config_t *config);

/* 降级连接池配置 */
void pg_config_set_fallback_pool_min_size(pg_config_t *config, int size);
void pg_config_set_fallback_pool_init_size(pg_config_t *config, int size);
void pg_config_set_fallback_pool_max_size(pg_config_t *config, int size);

int pg_config_get_fallback_pool_min_size(const pg_config_t *config);
int pg_config_get_fallback_pool_init_size(const pg_config_t *config);
int pg_config_get_fallback_pool_max_size(const pg_config_t *config);

/* 预编译语句缓存 */
void pg_config_set_stmt_cache_size(pg_config_t *config, int size);
int pg_config_get_stmt_cache_size(const pg_config_t *config);

/* 健康监控 */
void pg_config_set_health_check_interval_sec(pg_config_t *config, int interval_sec);
void pg_config_set_slow_query_threshold_ms(pg_config_t *config, int threshold_ms);
void pg_config_set_reconnect_backoff_max_sec(pg_config_t *config, int max_sec);

int pg_config_get_health_check_interval_sec(const pg_config_t *config);
int pg_config_get_slow_query_threshold_ms(const pg_config_t *config);
int pg_config_get_reconnect_backoff_max_sec(const pg_config_t *config);

/* Circuit Breaker */
void pg_config_set_failure_threshold(pg_config_t *config, int threshold);
void pg_config_set_recovery_threshold(pg_config_t *config, int threshold);

int pg_config_get_failure_threshold(const pg_config_t *config);
int pg_config_get_recovery_threshold(const pg_config_t *config);

/* 异步写队列 */
void pg_config_set_async_write_queue_size(pg_config_t *config, int size);
void pg_config_set_async_write_batch_size(pg_config_t *config, int size);

int pg_config_get_async_write_queue_size(const pg_config_t *config);
int pg_config_get_async_write_batch_size(const pg_config_t *config);

/* 慢查询阈值 */
void pg_config_set_slow_query_threshold_ms(pg_config_t *config, int threshold_ms);
int pg_config_get_slow_query_threshold_ms(const pg_config_t *config);

/* 重连退避最大延迟 */
void pg_config_set_reconnect_backoff_max_sec(pg_config_t *config, int max_sec);
int pg_config_get_reconnect_backoff_max_sec(const pg_config_t *config);

/* 释放配置 */
void pg_config_cleanup(pg_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* PG_CONFIG_H */
