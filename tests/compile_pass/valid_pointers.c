#include <stdlib.h>

void safe_pointer_usage(void) {
    int* ptr = (int*)malloc(sizeof(int) * 10);
    if (!ptr) return;

    for (int i = 0; i < 10; ++i) {
        ptr[i] = i * 2;
    }

    free(ptr); // Valid: freed exactly at allocation base
}

int main(void) {
    safe_pointer_usage();
    return 0;
}
