#include <stdlib.h>

void trigger_leak(void) {
    int* leaked = (int*)malloc(128);
    // ERROR: Returning from function without freeing must abort compilation
    return;
}

int main(void) {
    trigger_leak();
    return 0;
}
