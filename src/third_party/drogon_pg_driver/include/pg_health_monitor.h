#ifndef PG_HEALTH_MONITOR_H
#define PG_HEALTH_MONITOR_H

#include "pg_reconnect_backoff.h"
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 健康监控对象 */
typedef struct pg_health_monitor {
    int running;
    int check_interval_sec;
    time_t last_check_time;
    int healthy_connections;
    int failed_connections;
    int slow_queries;
    int consecutive_failures;
    int slow_query_threshold_ms;
    pg_reconnect_backoff_t *reconnect_backoff;
} pg_health_monitor_t;

/* 创建健康监控对象 */
pg_health_monitor_t* pg_health_monitor_create(void);

/* 销毁健康监控对象 */
void pg_health_monitor_destroy(pg_health_monitor_t *monitor);

/* 启动健康监控 */
int pg_health_monitor_start(pg_health_monitor_t *monitor);

/* 停止健康监控 */
void pg_health_monitor_stop(pg_health_monitor_t *monitor);

/* 执行一次健康检查 */
int pg_health_monitor_check(pg_health_monitor_t *monitor);

/* 获取健康连接数 */
int pg_health_monitor_get_healthy_connections(const pg_health_monitor_t *monitor);

/* 获取失败连接数 */
int pg_health_monitor_get_failed_connections(const pg_health_monitor_t *monitor);

/* 获取慢查询数 */
int pg_health_monitor_get_slow_queries(const pg_health_monitor_t *monitor);

/* 获取连续失败次数 */
int pg_health_monitor_get_consecutive_failures(const pg_health_monitor_t *monitor);

/* 重置统计 */
void pg_health_monitor_reset_stats(pg_health_monitor_t *monitor);

/* 设置检查间隔 */
void pg_health_monitor_set_check_interval(pg_health_monitor_t *monitor, int interval_sec);

/* 设置慢查询阈值 */
void pg_health_monitor_set_slow_query_threshold(pg_health_monitor_t *monitor, int threshold_ms);

/* 记录慢查询 */
void pg_health_monitor_record_slow_query(pg_health_monitor_t *monitor, const char *sql, int64_t elapsed_ms);

/* 获取下次重试时间 */
time_t pg_health_monitor_get_next_retry_time(const pg_health_monitor_t *monitor);

/* 获取当前重连延迟 */
int pg_health_monitor_get_reconnect_delay(const pg_health_monitor_t *monitor);

#ifdef __cplusplus
}
#endif

#endif /* PG_HEALTH_MONITOR_H */
