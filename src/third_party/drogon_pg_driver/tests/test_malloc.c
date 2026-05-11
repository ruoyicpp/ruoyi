#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("Test 1: printf works\n");

    char *test = malloc(100);
    if (test == NULL) {
        printf("malloc failed\n");
        return 1;
    }
    printf("Test 2: malloc works\n");

    strcpy(test, "Hello");
    printf("Test 3: strcpy works: %s\n", test);

    free(test);
    printf("Test 4: free works\n");

    return 0;
}
