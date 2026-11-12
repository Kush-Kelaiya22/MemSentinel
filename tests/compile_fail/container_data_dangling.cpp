#include <vector>

int main() {
    std::vector<int> vec = {1, 2, 3};
    int* data = vec.data();
    vec.push_back(4);
    vec.push_back(5);
    return data[0];
}
