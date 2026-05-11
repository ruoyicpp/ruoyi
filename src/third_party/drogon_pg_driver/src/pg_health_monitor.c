#include "pg_health_monitor.h"
#include "pg_client.h"
#include "pg_config.h"
#include "pg_logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 创建健康监控对象 */
pg_health_monitor_t* pg_health_monitor_create(void) {
    pg_health_monitor_t *monitor = (pg_health_monitor_t*)malloc(sizeof(pg_health_monitor_t));
    if (monitor == NULL) {
        return NULL;
    }

    memset(monitor, 0, sizeof(pg_health_monitor_t));

    monitor->running = 0;
    monitor->check_interval_sec = 30;
    monitor->last_check_time = 0;
    monitor->healthy_connections = 0;
    monitor->failed_connections = 0;
    monitor->slow_queries = 0;
    monitor->consecutive_failures = 0;
    monitor->slow_query_threshold_ms = 100;

    /* 创建指数退避重连对象 */
    monitor->reconnect_backoff = pg_reconnect_backoff_create(1, 60);
    if (monitor->reconnect_backoff == NULL) {
        free(monitor);
        return NULL;
    }

    return monitor;
}

/* 销毁健康监控对象 */
void pg_health_monitor_destroy(pg_health_monitor_t *monitor) {
    if (monitor == NULL) {
        return;
    }

    pg_health_monitor_stop(monitor);

    if (monitor->reconnect_backoff != NULL) {
        pg_reconnect_backoff_destroy(monitor->reconnect_backoff);
    }

    free(monitor);
}

/* 启动健康监控 */
int pg_health_monitor_start(pg_health_monitor_t *monitor) {
    if (monitor == NULL) {
        return -1;
    }

    if (monitor->running) {
        return 0;
    }

    pg_config_t *config = pg_config_get_instance();
    monitor->check_interval_sec = pg_config_get_health_check_interval_sec(config);
    monitor->slow_query_threshold_ms = pg_config_get_slow_query_threshold_ms(config);

    /* 设置重连退避最大延迟 */
    pg_reconnect_backoff_t *backoff = monitor->reconnect_backoff;
    if (backoff != NULL) {
        backoff->max_delay_sec = pg_config_get_reconnect_backoff_max_sec(config);
    }

    monitor->running = 1;
    monitor->last_check_time = time(NULL);

    PG_LOG_INFO("[HEALTH_MONITOR] Started with interval=%ds, slow_query_threshold=%dms",
                monitor->check_interval_sec, monitor->slow_query_threshold_ms);

    return 0;
}

/* 停止健康监控 */
void pg_health_monitor_stop(pg_health_monitor_t *monitor) {
    if (monitor == NULL) {
        return;
    }

    monitor->running = 0;
    PG_LOG_INFO("[HEALTH_MONITOR] Stopped");
}

/* 执行一次健康检查 */
int pg_health_monitor_check(pg_health_monitor_t *monitor) {
    if (monitor == NULL) {
        return -1;
    }

    time_t now = time(NULL);

    /* 检查是否到达检查时间 */
    if (now - monitor->last_check_time < monitor->check_interval_sec) {
        return 0;
    }

    monitor->last_check_time = now;

    /* 检查是否可以重试（指数退避） */
    if (!pg_reconnect_backoff_can_retry(monitor->reconnect_backoff)) {
        PG_LOG_DEBUG("[HEALTH_MONITOR] Waiting for backoff, next retry in %ds",
                     (int)(pg_reconnect_backoff_get_next_retry_time(monitor->reconnect_backoff) - now));
        return 0;
    }

    /* 执行健康检查 */
    int healthy = pg_client_health_check();

    if (healthy >= 0) {
        monitor->healthy_connections = healthy;
        monitor->failed_connections = 0;
        monitor->consecutive_failures = 0;

        /* 重置退避 */
        pg_reconnect_backoff_record_success(monitor->reconnect_backoff);

        PG_LOG_INFO("[HEALTH_MONITOR] Health check passed: %d connections healthy", healthy);
    } else {
        monitor->failed_connections++;
        monitor->consecutive_failures++;

        /* 记录失败，计算下次重试延迟 */
        int delay = pg_reconnect_backoff_record_failure(monitor->reconnect_backoff);

        PG_LOG_WARN("[HEALTH_MONITOR] Health check failed (consecutive=%d), next retry in %ds",
                    monitor->consecutive_failures, delay);

        /* 如果连续失败超过阈值，触发降级 */
        if (monitor->consecutive_failures >= 5) {
            PG_LOG_ERROR("[HEALTH_MONITOR] Too many consecutive failures, consider triggering fallback mode");
        }
    }

    return healthy;
}

/* 记录慢查询 */
void pg_health_monitor_record_slow_query(pg_health_monitor_t *monitor, const char *sql, int64_t elapsed_ms) {
    if (monitor == NULL) {
        return;
    }

    if (elapsed_ms >= monitor->slow_query_threshold_ms) {
        monitor->slow_queries++;
        pg_logger_slow_query(sql, elapsed_ms, monitor->slow_query_threshold_ms);
    }
}

/* 获取健康连接数 */
int pg_health_monitor_get_healthy_connections(const pg_health_monitor_t *monitor) {
    if (monitor == NULL) {
        return 0;
    }
    return monitor->healthy_connections;
}

/* 获取失败连接数 */
int pg_health_monitor_get_failed_connections(const pg_health_monitor_t *monitor) {
    if (monitor == NULL) {
        return 0;
    }
    return monitor->failed_connections;
}

/* 获取慢查询数 */
int pg_health_monitor_get_slow_queries(const pg_health_monitor_t *monitor) {
    if (monitor == NULL) {
        return 0;
    }
    return monitor->slow_queries;
}

/* 获取连续失败次数 */
int pg_health_monitor_get_consecutive_failures(const pg_health_monitor_t *monitor) {
    if (monitor == NULL) {
        return 0;
    }
    return monitor->consecutive_failures;
}

/* 重置统计 */
void pg_health_monitor_reset_stats(pg_health_monitor_t *monitor) {
    if (monitor == NULL) {
        return;
    }

    monitor->healthy_connections = 0;
    monitor->failed_connections = 0;
    monitor->slow_queries = 0;
    monitor->consecutive_failures = 0;

    if (monitor->reconnect_backoff != NULL) {
        pg_reconnect_backoff_reset(monitor->reconnect_backoff);
    }
}

/* 设置检查间隔 */
void pg_health_monitor_set_check_interval(pg_health_monitor_t *monitor, int interval_sec) {
    if (monitor == NULL || interval_sec <= 0) {
        return;
    }
    monitor->check_interval_sec = interval_sec;
}

/* 设置慢查询阈值 */
void pg_health_monitor_set_slow_query_threshold(pg_health_monitor_t *monitor, int threshold_ms) {
    if (monitor == NULL || threshold_ms <= 0) {
        return;
    }
    monitor->slow_query_threshold_ms = threshold_ms;
}

/* 获取下次重试时间 */
time_t pg_health_monitor_get_next_retry_time(const pg_health_monitor_t *monitor) {
    if (monitor == NULL || monitor->reconnect_backoff == NULL) {
        return 0;
    }
    return pg_reconnect_backoff_get_next_retry_time(monitor->reconnect_backoff);
}

/* 获取当前重连延迟 */
int pg_health_monitor_get_reconnect_delay(const pg_health_monitor_t *monitor) {
    if (monitor == NULL || monitor->reconnect_backoff == NULL) {
        return 0;
    }
    return pg_reconnect_backoff_get_delay(monitor->reconnect_backoff);
}
