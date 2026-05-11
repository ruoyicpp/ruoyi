#include "pg_error.h"
#include <stdio.h>

int main(void) {
    printf("Creating error...\n");
    pg_error_t *error = pg_error_create_with_sqlstate(
        PG_ERROR_QUERY_FAILED,
        "Syntax error",
        "42601"
    );

    if (error == NULL) {
        printf("Failed to create error\n");
        return 1;
    }

    printf("Error created\n");
    const char *full_msg = pg_error_get_full_message(error);
    printf("Full message: '%s'\n", full_msg);

    pg_error_destroy(error);
    printf("Error destroyed\n");

    return 0;
}
