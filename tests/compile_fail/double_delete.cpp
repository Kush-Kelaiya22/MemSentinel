int main() {
    int* p = new int(42);
    delete p;
    delete p;
    return 0;
}
