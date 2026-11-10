#include <vector>
#include <memory>

void safe_cpp_code() {
    auto uptr = std::make_unique<int>(100);
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    numbers.push_back(*uptr);
}

int main() {
    safe_cpp_code();
    return 0;
}
