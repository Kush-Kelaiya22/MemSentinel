#include <stdlib.h>
#include "safe_c/api.h"

/* Strict ISO C89 / C90 legacy compatibility test */
static int c89_sum(int n) {
    int *arr;
    int i;
    int total;

    (void)SAFE_C_VERSION_MAJOR;
    if (n <= 0) {
        return 0;
    }

    arr = (int *)malloc((size_t)n * sizeof(int));
    if (arr == NULL) {
        return 0;
    }

    for (i = 0; i < n; ++i) {
        arr[i] = i + 1;
    }

    total = 0;
    for (i = 0; i < n; ++i) {
        total += arr[i];
    }

    free(arr);
    return total;
}

int main(void) {
    return c89_sum(4) == 10 ? 0 : 1;
}
