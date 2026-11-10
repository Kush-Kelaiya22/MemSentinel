#include <stdlib.h>

void trigger_double_free(void) {
    int* ptr = (int*)malloc(sizeof(int) * 10);
    free(ptr);
    free(ptr); // ERROR: Double free must abort compilation
}

int main(void) {
    trigger_double_free();
    return 0;
}
