#include <stdlib.h>

int main(void) {
    char* buf = (char*)malloc(8);
    if (!buf) return 1;
    buf[12] = 'x';
    free(buf);
    return 0;
}
