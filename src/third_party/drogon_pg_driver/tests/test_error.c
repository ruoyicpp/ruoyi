#include "pg_error.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_error_create(void) {
    pg_error_t *error = pg_error_create(PG_ERROR_CONNECTION_FAILED, "Cannot connect to database");

    assert(error != NULL);
    assert(pg_error_get_code(error) == PG_ERROR_CONNECTION_FAILED);
    assert(strcmp(pg_error_get_message(error), "Cannot connect to database") == 0);
    assert(strcmp(pg_error_get_sqlstate(error), "") == 0);

    pg_error_destroy(error);
    printf("✓ Error create test passed\n");
}

void test_error_create_with_sqlstate(void) {
    pg_error_t *error = pg_error_create_with_sqlstate(
        PG_ERROR_QUERY_FAILED,
        "Syntax error",
        "42601"
    );

    assert(error != NULL);
    assert(pg_error_get_code(error) == PG_ERROR_QUERY_FAILED);
    assert(strcmp(pg_error_get_message(error), "Syntax error") == 0);
    assert(strcmp(pg_error_get_sqlstate(error), "42601") == 0);

    pg_error_destroy(error);
    printf("✓ Error create with sqlstate test passed\n");
}

void test_error_full_message(void) {
    pg_error_t *error = pg_error_create(PG_ERROR_CONNECTION_FAILED, "Cannot connect");

    const char *full_msg = pg_error_get_full_message(error);
    assert(full_msg != NULL);
    assert(strstr(full_msg, "CONNECTION_FAILED") != NULL);
    assert(strstr(full_msg, "Cannot connect") != NULL);

    pg_error_destroy(error);
    printf("✓ Error full message test passed\n");
}

void test_error_full_message_with_sqlstate(void) {
    pg_error_t *error = pg_error_create_with_sqlstate(
        PG_ERROR_QUERY_FAILED,
        "Syntax error",
        "42601"
    );

    const char *full_msg = pg_error_get_full_message(error);
    printf("Full message: '%s'\n", full_msg);
    assert(full_msg != NULL);
    assert(strstr(full_msg, "QUERY_FAILED") != NULL);
    assert(strstr(full_msg, "Syntax error") != NULL);
    assert(strstr(full_msg, "SQLSTATE: 42601") != NULL);

    pg_error_destroy(error);
    printf("✓ Error full message with sqlstate test passed\n");
}

void test_error_null_safety(void) {
    assert(pg_error_get_code(NULL) == PG_ERROR_UNKNOWN_ERROR);
    assert(strcmp(pg_error_get_message(NULL), "Unknown error") == 0);
    assert(strcmp(pg_error_get_sqlstate(NULL), "") == 0);
    assert(strcmp(pg_error_get_full_message(NULL), "Unknown error") == 0);

    pg_error_destroy(NULL);

    printf("✓ Error null safety test passed\n");
}

void test_error_code_to_string(void) {
    assert(strcmp(pg_error_code_to_string(PG_ERROR_SUCCESS), "SUCCESS") == 0);
    assert(strcmp(pg_error_code_to_string(PG_ERROR_CONNECTION_FAILED), "CONNECTION_FAILED") == 0);
    assert(strcmp(pg_error_code_to_string(PG_ERROR_QUERY_FAILED), "QUERY_FAILED") == 0);
    assert(strcmp(pg_error_code_to_string(PG_ERROR_CIRCUIT_BREAKER_OPEN), "CIRCUIT_BREAKER_OPEN") == 0);
    assert(strcmp(pg_error_code_to_string(PG_ERROR_UNKNOWN_ERROR), "UNKNOWN_ERROR") == 0);

    printf("✓ Error code to string test passed\n");
}

void test_error_create_null_message(void) {
    pg_error_t *error = pg_error_create(PG_ERROR_CONNECTION_FAILED, NULL);

    assert(error != NULL);
    assert(strcmp(pg_error_get_message(error), "Unknown error") == 0);

    pg_error_destroy(error);
    printf("✓ Error create null message test passed\n");
}

int main(void) {
    printf("Running pg_error tests...\n\n");

    test_error_create();
    test_error_create_with_sqlstate();
    test_error_full_message();
    test_error_full_message_with_sqlstate();
    test_error_null_safety();
    test_error_code_to_string();
    test_error_create_null_message();

    printf("\nAll tests passed!\n");

    return 0;
}
