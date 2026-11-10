#include <vector>
#include <memory>

void safe_raii_code() {
    auto uptr = std::make_unique<int>(100);
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    numbers.push_back(*uptr);
}

int main() {
    safe_raii_code();
    return 0;
}
