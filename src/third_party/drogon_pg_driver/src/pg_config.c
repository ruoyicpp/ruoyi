#include "pg_config.h"
#include <string.h>
#include <stdlib.h>

static pg_config_t g_config_instance = {0};

pg_config_t* pg_config_get_instance(void) {
    static int initialized = 0;

    if (!initialized) {
        /* 设置默认值 */
        g_config_instance.connection_string = NULL;
        g_config_instance.driver_mode = PG_DRIVER_MODE_AUTO;

        /* 主连接池 */
        g_config_instance.pool_min_size = 3;
        g_config_instance.pool_init_size = 8;
        g_config_instance.pool_max_size = 24;
        g_config_instance.pool_checkout_timeout_ms = 5000;

        /* 降级连接池 */
        g_config_instance.fallback_pool_min_size = 2;
        g_config_instance.fallback_pool_init_size = 2;
        g_config_instance.fallback_pool_max_size = 3;

        /* 预编译语句缓存 */
        g_config_instance.stmt_cache_size = 128;

        /* 健康监控 */
        g_config_instance.health_check_interval_sec = 30;
        g_config_instance.slow_query_threshold_ms = 100;
        g_config_instance.reconnect_backoff_max_sec = 60;

        /* Circuit Breaker */
        g_config_instance.failure_threshold = 5;
        g_config_instance.recovery_threshold = 10;

        /* 异步写队列 */
        g_config_instance.async_write_queue_size = 50000;
        g_config_instance.async_write_batch_size = 200;

        initialized = 1;
    }

    return &g_config_instance;
}

void pg_config_set_connection_string(pg_config_t *config, const char *conn_str) {
    if (config == NULL) {
        return;
    }

    if (config->connection_string != NULL) {
        free(config->connection_string);
        config->connection_string = NULL;
    }

    if (conn_str != NULL) {
        config->connection_string = strdup(conn_str);
    }
}

const char* pg_config_get_connection_string(const pg_config_t *config) {
    if (config == NULL) {
        return "";
    }
    return config->connection_string ? config->connection_string : "";
}

void pg_config_set_driver_mode(pg_config_t *config, pg_driver_mode_t mode) {
    if (config != NULL) {
        config->driver_mode = mode;
    }
}

pg_driver_mode_t pg_config_get_driver_mode(const pg_config_t *config) {
    if (config == NULL) {
        return PG_DRIVER_MODE_AUTO;
    }
    return config->driver_mode;
}

void pg_config_set_pool_min_size(pg_config_t *config, int size) {
    if (config != NULL && size > 0) {
        config->pool_min_size = size;
    }
}

void pg_config_set_pool_init_size(pg_config_t *config, int size) {
    if (config != NULL && size > 0) {
        config->pool_init_size = size;
    }
}

void pg_config_set_pool_max_size(pg_config_t *config, int size) {
    if (config != NULL && size > 0) {
        config->pool_max_size = size;
    }
}

void pg_config_set_pool_checkout_timeout_ms(pg_config_t *config, int timeout_ms) {
    if (config != NULL && timeout_ms > 0) {
        config->pool_checkout_timeout_ms = timeout_ms;
    }
}

int pg_config_get_pool_min_size(const pg_config_t *config) {
    if (config == NULL) {
        return 3;
    }
    return config->pool_min_size;
}

int pg_config_get_pool_init_size(const pg_config_t *config) {
    if (config == NULL) {
        return 8;
    }
    return config->pool_init_size;
}

int pg_config_get_pool_max_size(const pg_config_t *config) {
    if (config == NULL) {
        return 24;
    }
    return config->pool_max_size;
}

int pg_config_get_pool_checkout_timeout_ms(const pg_config_t *config) {
    if (config == NULL) {
        return 5000;
    }
    return config->pool_checkout_timeout_ms;
}

void pg_config_set_fallback_pool_min_size(pg_config_t *config, int size) {
    if (config != NULL && size > 0) {
        config->fallback_pool_min_size = size;
    }
}

void pg_config_set_fallback_pool_init_size(pg_config_t *config, int size) {
    if (config != NULL && size > 0) {
        config->fallback_pool_init_size = size;
    }
}

void pg_config_set_fallback_pool_max_size(pg_config_t *config, int size) {
    if (config != NULL && size > 0) {
        config->fallback_pool_max_size = size;
    }
}

int pg_config_get_fallback_pool_min_size(const pg_config_t *config) {
    if (config == NULL) {
        return 2;
    }
    return config->fallback_pool_min_size;
}

int pg_config_get_fallback_pool_init_size(const pg_config_t *config) {
    if (config == NULL) {
        return 2;
    }
    return config->fallback_pool_init_size;
}

int pg_config_get_fallback_pool_max_size(const pg_config_t *config) {
    if (config == NULL) {
        return 3;
    }
    return config->fallback_pool_max_size;
}

void pg_config_set_stmt_cache_size(pg_config_t *config, int size) {
    if (config != NULL && size > 0) {
        config->stmt_cache_size = size;
    }
}

int pg_config_get_stmt_cache_size(const pg_config_t *config) {
    if (config == NULL) {
        return 128;
    }
    return config->stmt_cache_size;
}

void pg_config_set_health_check_interval_sec(pg_config_t *config, int interval_sec) {
    if (config != NULL && interval_sec > 0) {
        config->health_check_interval_sec = interval_sec;
    }
}

void pg_config_set_slow_query_threshold_ms(pg_config_t *config, int threshold_ms) {
    if (config != NULL && threshold_ms > 0) {
        config->slow_query_threshold_ms = threshold_ms;
    }
}

void pg_config_set_reconnect_backoff_max_sec(pg_config_t *config, int max_sec) {
    if (config != NULL && max_sec > 0) {
        config->reconnect_backoff_max_sec = max_sec;
    }
}

int pg_config_get_health_check_interval_sec(const pg_config_t *config) {
    if (config == NULL) {
        return 30;
    }
    return config->health_check_interval_sec;
}

int pg_config_get_slow_query_threshold_ms(const pg_config_t *config) {
    if (config == NULL) {
        return 100;
    }
    return config->slow_query_threshold_ms;
}

int pg_config_get_reconnect_backoff_max_sec(const pg_config_t *config) {
    if (config == NULL) {
        return 60;
    }
    return config->reconnect_backoff_max_sec;
}

void pg_config_set_failure_threshold(pg_config_t *config, int threshold) {
    if (config != NULL && threshold > 0) {
        config->failure_threshold = threshold;
    }
}

void pg_config_set_recovery_threshold(pg_config_t *config, int threshold) {
    if (config != NULL && threshold > 0) {
        config->recovery_threshold = threshold;
    }
}

int pg_config_get_failure_threshold(const pg_config_t *config) {
    if (config == NULL) {
        return 5;
    }
    return config->failure_threshold;
}

int pg_config_get_recovery_threshold(const pg_config_t *config) {
    if (config == NULL) {
        return 10;
    }
    return config->recovery_threshold;
}

void pg_config_set_async_write_queue_size(pg_config_t *config, int size) {
    if (config != NULL && size > 0) {
        config->async_write_queue_size = size;
    }
}

void pg_config_set_async_write_batch_size(pg_config_t *config, int size) {
    if (config != NULL && size > 0) {
        config->async_write_batch_size = size;
    }
}

int pg_config_get_async_write_queue_size(const pg_config_t *config) {
    if (config == NULL) {
        return 50000;
    }
    return config->async_write_queue_size;
}

int pg_config_get_async_write_batch_size(const pg_config_t *config) {
    if (config == NULL) {
        return 200;
    }
    return config->async_write_batch_size;
}

void pg_config_cleanup(pg_config_t *config) {
    if (config == NULL) {
        return;
    }

    if (config->connection_string != NULL) {
        free(config->connection_string);
        config->connection_string = NULL;
    }
}
