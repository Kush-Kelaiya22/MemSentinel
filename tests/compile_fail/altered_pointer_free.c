#include <stdlib.h>

void trigger_altered_base_free(void) {
    char* buf = (char*)malloc(32);
    buf += 4;   // Pointer address altered!
    free(buf);  // ERROR: Freeing interior pointer must abort compilation
}

int main(void) {
    trigger_altered_base_free();
    return 0;
}
