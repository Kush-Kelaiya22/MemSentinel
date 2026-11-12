#include <cstddef>
#include <string>
#include <vector>

int main() {
    std::vector<int> v1 = {1, 2, 3, 4};
    for (auto it = v1.begin(); it != v1.end();) {
        if (*it % 2 == 0) {
            it = v1.erase(it);
        } else {
            ++it;
        }
    }

    std::vector<int> v2 = {1, 2, 3};
    for (int x : v2) {
        if (x == 2) {
            v2.push_back(99);
            break;
        }
    }

    std::vector<int> v3;
    v3.reserve(10);
    v3.push_back(7);
    int* raw = v3.data();
    int first = raw[0];
    v3.push_back(8);
    raw = v3.data();
    int second = raw[1];

    std::vector<int> v4 = {10, 20};
    for (std::size_t i = 0; i < 2; ++i) {
        v4.push_back(static_cast<int>(i));
    }

    std::string msg = "safe-c";
    const char* s = msg.c_str();
    char c0 = s[0];

    return (first + second + static_cast<int>(c0) + static_cast<int>(v4.size()) + static_cast<int>(v1.size())) & 1;
}
