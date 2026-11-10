#ifndef SAFE_C_RUNTIME_LOGGER_H
#define SAFE_C_RUNTIME_LOGGER_H

#include "safe_c/common.h"
#include "stacktrace.h"
#include <string>

namespace safe_c::rt {

struct LogEntry {
    ViolationType violation;
    const void* target_address{nullptr};
    const void* base_address{nullptr};
    int64_t offset_bytes{0};
    size_t allocated_size{0};
    std::string alloc_site;
    std::string free_site;
    std::vector<StackFrame> callstack;
    std::string explanation;
};

class Logger {
public:
    static Logger& instance();

    void set_log_path(std::string path);
    [[nodiscard]] const std::string& get_log_path() const noexcept;

    void log_violation(const LogEntry& entry);
    void log_leak_summary(size_t leak_count, size_t total_bytes);

private:
    Logger();
    ~Logger() = default;

    std::string log_filepath_{"safe_c_runtime.log"};
};

} // namespace safe_c::rt

namespace safe_cpp::rt {
    using namespace safe_c::rt;
}

namespace memcheck::rt {
    using namespace safe_c::rt;
}

#endif // SAFE_C_RUNTIME_LOGGER_H
