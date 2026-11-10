#ifndef SAFE_C_RUNTIME_STACKTRACE_H
#define SAFE_C_RUNTIME_STACKTRACE_H

#include <string>
#include <vector>

namespace safe_c::rt {

struct StackFrame {
    void* address{nullptr};
    std::string module_name;
    std::string function_name;
    std::string file;
    int line{0};
};

std::vector<StackFrame> capture_stacktrace(size_t max_frames = 32, size_t skip_frames = 2);
std::string format_stacktrace(const std::vector<StackFrame>& frames);

} // namespace safe_c::rt

namespace safe_cpp::rt {
    using namespace safe_c::rt;
}

namespace memcheck::rt {
    using namespace safe_c::rt;
}

#endif // SAFE_C_RUNTIME_STACKTRACE_H
