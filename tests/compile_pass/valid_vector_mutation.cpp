#include <memory>
#include <vector>

int main() {
    std::vector<int> vec = {1, 2, 3};
    vec.push_back(4);
    vec.resize(16, 7);
    vec[10] = 99;
    vec.clear();

    auto ptr = std::make_unique<int>(42);
    *ptr = 100;
    return *ptr - 100;
}
