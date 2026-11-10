void trigger_uaf() {
    int* data = new int;
    delete data;
    *data = 42; // ERROR: Use-after-free must abort compilation
}

int main() {
    trigger_uaf();
    return 0;
}
