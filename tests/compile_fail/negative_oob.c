#include <stdlib.h>

int main(void) {
    int* arr = (int*)malloc(sizeof(int) * 4);
    if (!arr) return 1;
    arr[-1] = 7;
    free(arr);
    return 0;
}
