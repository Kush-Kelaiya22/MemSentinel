#include <stdlib.h>

int main(void) {
    char* buf = (char*)malloc(16);
    if (!buf) return 1;
    buf = (char*)malloc(32);
    if (!buf) return 1;
    free(buf);
    return 0;
}
