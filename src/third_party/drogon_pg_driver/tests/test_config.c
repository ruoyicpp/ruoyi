#include "pg_config.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_config_singleton(void) {
    pg_config_t *config1 = pg_config_get_instance();
    pg_config_t *config2 = pg_config_get_instance();

    assert(config1 == config2);
    printf("✓ Config singleton test passed\n");
}

void test_config_defaults(void) {
    pg_config_t *config = pg_config_get_instance();

    assert(pg_config_get_pool_min_size(config) == 3);
    assert(pg_config_get_pool_init_size(config) == 8);
    assert(pg_config_get_pool_max_size(config) == 24);
    assert(pg_config_get_pool_checkout_timeout_ms(config) == 5000);

    assert(pg_config_get_fallback_pool_min_size(config) == 2);
    assert(pg_config_get_fallback_pool_init_size(config) == 2);
    assert(pg_config_get_fallback_pool_max_size(config) == 3);

    assert(pg_config_get_stmt_cache_size(config) == 128);
    assert(pg_config_get_health_check_interval_sec(config) == 30);
    assert(pg_config_get_slow_query_threshold_ms(config) == 100);
    assert(pg_config_get_reconnect_backoff_max_sec(config) == 60);

    assert(pg_config_get_failure_threshold(config) == 5);
    assert(pg_config_get_recovery_threshold(config) == 10);

    assert(pg_config_get_async_write_queue_size(config) == 50000);
    assert(pg_config_get_async_write_batch_size(config) == 200);

    printf("✓ Config defaults test passed\n");
}

void test_config_setters(void) {
    pg_config_t *config = pg_config_get_instance();

    pg_config_set_connection_string(config, "host=localhost port=5432");
    assert(strcmp(pg_config_get_connection_string(config), "host=localhost port=5432") == 0);

    pg_config_set_driver_mode(config, PG_DRIVER_MODE_NATIVE);
    assert(pg_config_get_driver_mode(config) == PG_DRIVER_MODE_NATIVE);

    pg_config_set_pool_min_size(config, 5);
    assert(pg_config_get_pool_min_size(config) == 5);

    pg_config_set_pool_init_size(config, 10);
    assert(pg_config_get_pool_init_size(config) == 10);

    pg_config_set_pool_max_size(config, 30);
    assert(pg_config_get_pool_max_size(config) == 30);

    pg_config_set_failure_threshold(config, 10);
    assert(pg_config_get_failure_threshold(config) == 10);

    pg_config_set_recovery_threshold(config, 20);
    assert(pg_config_get_recovery_threshold(config) == 20);

    printf("✓ Config setters test passed\n");
}

void test_config_null_safety(void) {
    pg_config_t *null_config = NULL;

    assert(pg_config_get_pool_min_size(null_config) == 3);
    assert(pg_config_get_driver_mode(null_config) == PG_DRIVER_MODE_AUTO);
    assert(strcmp(pg_config_get_connection_string(null_config), "") == 0);

    pg_config_set_pool_min_size(null_config, 10);
    assert(pg_config_get_pool_min_size(null_config) == 3);

    printf("✓ Config null safety test passed\n");
}

int main(void) {
    printf("Running pg_config tests...\n\n");

    test_config_singleton();
    test_config_defaults();
    test_config_setters();
    test_config_null_safety();

    printf("\nAll tests passed!\n");

    pg_config_cleanup(pg_config_get_instance());

    return 0;
}
