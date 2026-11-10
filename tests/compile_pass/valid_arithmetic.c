#include <stdlib.h>

void safe_arithmetic(void) {
    int* base = (int*)malloc(sizeof(int) * 8);
    if (!base) return;

    // Use pointer arithmetic via a temporary pointer without altering base
    int* cursor = base;
    for (int i = 0; i < 8; ++i) {
        *cursor = i;
        cursor++;
    }

    // Always free original base pointer
    free(base);
}

int main(void) {
    safe_arithmetic();
    return 0;
}
