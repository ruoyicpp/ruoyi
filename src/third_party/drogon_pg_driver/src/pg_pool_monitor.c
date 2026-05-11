#include "pg_pool_monitor.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

/* 创建监控对象 */
pg_pool_monitor_t* pg_pool_monitor_create(int max_connections) {
    pg_pool_monitor_t *monitor = (pg_pool_monitor_t*)malloc(sizeof(pg_pool_monitor_t));
    if (monitor == NULL) {
        return NULL;
    }

    memset(monitor, 0, sizeof(pg_pool_monitor_t));

    monitor->lease_capacity = max_connections > 0 ? max_connections : 100;
    monitor->leases = (pg_connection_lease_t*)calloc(monitor->lease_capacity, sizeof(pg_connection_lease_t));
    if (monitor->leases == NULL) {
        free(monitor);
        return NULL;
    }

    memset(monitor->leases, 0, monitor->lease_capacity * sizeof(pg_connection_lease_t));

    monitor->lease_count = 0;
    monitor->next_lease_id = 1;
    monitor->leak_timeout_sec = 30;  /* 默认 30 秒 */
    monitor->enabled = 1;

    return monitor;
}

/* 销毁监控对象 */
void pg_pool_monitor_destroy(pg_pool_monitor_t *monitor) {
    if (monitor == NULL) {
        return;
    }

    if (monitor->leases != NULL) {
        /* 释放所有 caller_info */
        for (int i = 0; i < monitor->lease_capacity; i++) {
            if (monitor->leases[i].caller_info != NULL) {
                free(monitor->leases[i].caller_info);
                monitor->leases[i].caller_info = NULL;
            }
        }
        free(monitor->leases);
        monitor->leases = NULL;
    }

    free(monitor);
}

/* 记录 checkout */
int pg_pool_monitor_record_checkout(pg_pool_monitor_t *monitor, int connection_index, const char *caller_info) {
    if (monitor == NULL || !monitor->enabled) {
        return -1;
    }

    /* 查找空闲租约槽 */
    int slot = -1;
    for (int i = 0; i < monitor->lease_capacity; i++) {
        if (!monitor->leases[i].is_active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return -1;  /* 没有可用槽 */
    }

    monitor->leases[slot].connection_index = connection_index;
    monitor->leases[slot].checkout_time = time(NULL);
    monitor->leases[slot].caller_info = caller_info ? strdup(caller_info) : NULL;
    monitor->leases[slot].lease_id = monitor->next_lease_id++;
    monitor->leases[slot].is_active = 1;

    monitor->lease_count++;
    monitor->stats.total_checkouts++;
    monitor->stats.current_in_use++;

    /* 更新峰值 */
    if (monitor->stats.current_in_use > monitor->stats.peak_in_use) {
        monitor->stats.peak_in_use = monitor->stats.current_in_use;
    }

    return monitor->leases[slot].lease_id;
}

/* 记录 checkin */
void pg_pool_monitor_record_checkin(pg_pool_monitor_t *monitor, int lease_id) {
    if (monitor == NULL || !monitor->enabled || lease_id <= 0) {
        return;
    }

    /* 查找租约 */
    for (int i = 0; i < monitor->lease_capacity; i++) {
        if (monitor->leases[i].is_active && monitor->leases[i].lease_id == lease_id) {
            /* 释放租约 */
            if (monitor->leases[i].caller_info != NULL) {
                free(monitor->leases[i].caller_info);
                monitor->leases[i].caller_info = NULL;
            }

            monitor->leases[i].is_active = 0;
            monitor->lease_count--;
            monitor->stats.total_checkins++;
            monitor->stats.current_in_use--;

            return;
        }
    }
}

/* 检查连接泄漏 */
int pg_pool_monitor_check_leaks(pg_pool_monitor_t *monitor) {
    if (monitor == NULL || !monitor->enabled) {
        return 0;
    }

    time_t now = time(NULL);
    int leak_count = 0;

    for (int i = 0; i < monitor->lease_capacity; i++) {
        if (monitor->leases[i].is_active) {
            int elapsed_sec = (int)(now - monitor->leases[i].checkout_time);

            if (elapsed_sec >= monitor->leak_timeout_sec) {
                /* 检测到泄漏 */
                leak_count++;
                monitor->stats.total_leaks++;
                monitor->stats.last_leak_detected_time = now;

                /* 记录日志 */
                printf("[LEAK] Connection %d leaked for %d seconds (lease_id=%d, caller=%s)\n",
                       monitor->leases[i].connection_index,
                       elapsed_sec,
                       monitor->leases[i].lease_id,
                       monitor->leases[i].caller_info ? monitor->leases[i].caller_info : "unknown");
            }
        }
    }

    return leak_count;
}

/* 记录超时 */
void pg_pool_monitor_record_timeout(pg_pool_monitor_t *monitor) {
    if (monitor == NULL) {
        return;
    }

    monitor->stats.total_timeouts++;
    monitor->stats.last_exhausted_time = time(NULL);
}

/* 记录错误 */
void pg_pool_monitor_record_error(pg_pool_monitor_t *monitor) {
    if (monitor == NULL) {
        return;
    }

    monitor->stats.total_errors++;
}

/* 获取统计信息 */
pg_pool_stats_t pg_pool_monitor_get_stats(const pg_pool_monitor_t *monitor) {
    pg_pool_stats_t empty = {0};

    if (monitor == NULL) {
        return empty;
    }

    return monitor->stats;
}

/* 重置统计 */
void pg_pool_monitor_reset_stats(pg_pool_monitor_t *monitor) {
    if (monitor == NULL) {
        return;
    }

    monitor->stats.total_checkouts = 0;
    monitor->stats.total_checkins = 0;
    monitor->stats.total_timeouts = 0;
    monitor->stats.total_errors = 0;
    monitor->stats.total_leaks = 0;
    monitor->stats.peak_in_use = monitor->stats.current_in_use;
    monitor->stats.peak_waiting = monitor->stats.current_waiting;
    monitor->stats.avg_checkout_time_ms = 0.0;
    monitor->stats.avg_query_time_ms = 0.0;
}

/* 设置泄漏超时 */
void pg_pool_monitor_set_leak_timeout(pg_pool_monitor_t *monitor, int timeout_sec) {
    if (monitor == NULL || timeout_sec <= 0) {
        return;
    }
    monitor->leak_timeout_sec = timeout_sec;
}

/* 启用/禁用监控 */
void pg_pool_monitor_set_enabled(pg_pool_monitor_t *monitor, int enabled) {
    if (monitor == NULL) {
        return;
    }
    monitor->enabled = enabled;
}

/* 获取当前使用数 */
int pg_pool_monitor_get_in_use_count(const pg_pool_monitor_t *monitor) {
    if (monitor == NULL) {
        return 0;
    }
    return monitor->stats.current_in_use;
}

/* 更新峰值 */
void pg_pool_monitor_update_peaks(pg_pool_monitor_t *monitor, int in_use, int waiting) {
    if (monitor == NULL) {
        return;
    }

    monitor->stats.current_in_use = in_use;
    monitor->stats.current_waiting = waiting;

    if (in_use > monitor->stats.peak_in_use) {
        monitor->stats.peak_in_use = in_use;
    }

    if (waiting > monitor->stats.peak_waiting) {
        monitor->stats.peak_waiting = waiting;
    }
}
