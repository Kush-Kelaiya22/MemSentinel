int* return_dangling(void) {
    int local = 42;
    return &local;
}

int main(void) {
    int* p = return_dangling();
    return p != 0 ? 0 : 1;
}
