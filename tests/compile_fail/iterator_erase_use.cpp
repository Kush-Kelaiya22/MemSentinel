#include <vector>

int main() {
    std::vector<int> vec = {10, 20, 30};
    auto it = vec.begin();
    vec.push_back(40);
    return *it;
}
