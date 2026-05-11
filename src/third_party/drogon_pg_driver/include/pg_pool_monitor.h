#ifndef PG_POOL_MONITOR_H
#define PG_POOL_MONITOR_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 连接池统计 */
typedef struct pg_pool_stats {
    int64_t total_checkouts;
    int64_t total_checkins;
    int64_t total_timeouts;
    int64_t total_errors;
    int64_t total_leaks;
    int current_in_use;
    int current_waiting;
    int peak_in_use;
    int peak_waiting;
    time_t last_exhausted_time;
    time_t last_leak_detected_time;
    double avg_checkout_time_ms;
    double avg_query_time_ms;
} pg_pool_stats_t;

/* 连接租约 */
typedef struct pg_connection_lease {
    int connection_index;
    time_t checkout_time;
    char *caller_info;
    int lease_id;
    int is_active;
} pg_connection_lease_t;

/* 连接池监控对象 */
typedef struct pg_pool_monitor {
    pg_pool_stats_t stats;
    pg_connection_lease_t *leases;
    int lease_capacity;
    int lease_count;
    int next_lease_id;
    int leak_timeout_sec;
    int enabled;
} pg_pool_monitor_t;

/* 创建监控对象 */
pg_pool_monitor_t* pg_pool_monitor_create(int max_connections);

/* 销毁监控对象 */
void pg_pool_monitor_destroy(pg_pool_monitor_t *monitor);

/* 记录 checkout */
int pg_pool_monitor_record_checkout(pg_pool_monitor_t *monitor, int connection_index, const char *caller_info);

/* 记录 checkin */
void pg_pool_monitor_record_checkin(pg_pool_monitor_t *monitor, int lease_id);

/* 检查连接泄漏 */
int pg_pool_monitor_check_leaks(pg_pool_monitor_t *monitor);

/* 记录超时 */
void pg_pool_monitor_record_timeout(pg_pool_monitor_t *monitor);

/* 记录错误 */
void pg_pool_monitor_record_error(pg_pool_monitor_t *monitor);

/* 获取统计信息 */
pg_pool_stats_t pg_pool_monitor_get_stats(const pg_pool_monitor_t *monitor);

/* 重置统计 */
void pg_pool_monitor_reset_stats(pg_pool_monitor_t *monitor);

/* 设置泄漏超时 */
void pg_pool_monitor_set_leak_timeout(pg_pool_monitor_t *monitor, int timeout_sec);

/* 启用/禁用监控 */
void pg_pool_monitor_set_enabled(pg_pool_monitor_t *monitor, int enabled);

/* 获取当前使用数 */
int pg_pool_monitor_get_in_use_count(const pg_pool_monitor_t *monitor);

/* 更新峰值 */
void pg_pool_monitor_update_peaks(pg_pool_monitor_t *monitor, int in_use, int waiting);

#ifdef __cplusplus
}
#endif

#endif /* PG_POOL_MONITOR_H */
