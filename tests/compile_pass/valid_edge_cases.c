#include <stdlib.h>

int main(void) {
    // 1. free(NULL) is always valid in C
    int* null_ptr = NULL;
    free(null_ptr);

    // 2. Conditional free after null check
    int* cond_ptr = (int*)malloc(sizeof(int));
    if (cond_ptr != NULL) {
        *cond_ptr = 42;
        free(cond_ptr);
    }

    // 3. Reassignment after free
    char* reused = (char*)malloc(16);
    if (reused != NULL) {
        reused[0] = 'A';
        free(reused);
    }
    reused = (char*)malloc(32);
    if (reused != NULL) {
        reused[31] = 'Z';
        free(reused);
    }

    // 4. realloc ownership transfer
    char* grown = (char*)malloc(16);
    if (grown != NULL) {
        char* tmp = (char*)realloc(grown, 64);
        if (tmp != NULL) {
            grown = tmp;
        }
        free(grown);
    }

    return 0;
}
