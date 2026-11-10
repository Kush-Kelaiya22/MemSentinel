#ifndef MEMCHECK_RUNTIME_STACKTRACE_H
#define MEMCHECK_RUNTIME_STACKTRACE_H

#include <string>
#include <vector>

namespace memcheck::rt {

struct StackFrame {
    void* address{nullptr};
    std::string module_name;
    std::string function_name;
    std::string file;
    int line{0};
};

std::vector<StackFrame> capture_stacktrace(size_t max_frames = 32, size_t skip_frames = 2);
std::string format_stacktrace(const std::vector<StackFrame>& frames);

} // namespace memcheck::rt

#endif // MEMCHECK_RUNTIME_STACKTRACE_H
