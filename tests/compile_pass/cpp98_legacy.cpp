#include "safe_c/api.h"

struct Box98 {
    int value;
    Box98() : value(0) {}
    explicit Box98(int v) : value(v) {}
    ~Box98() {}
};

int main() {
    int *p = new int(42);
    Box98 *b = new Box98(*p);
    Box98 *arr = new Box98[2];
    int sum = b->value + arr[0].value;
    (void)SAFE_C_VERSION_MAJOR;
    delete p;
    delete b;
    delete[] arr;
    return sum == 42 ? 0 : 1;
}
