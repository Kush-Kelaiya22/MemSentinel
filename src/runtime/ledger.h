#ifndef SAFE_C_RUNTIME_LEDGER_H
#define SAFE_C_RUNTIME_LEDGER_H

#include "safe_c/common.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <vector>

namespace safe_c::rt {

struct RuntimeBlock {
    uintptr_t base_address{0};
    size_t size{0};
    std::string alloc_location;
    bool is_freed{false};
    std::string free_location;
};

class AllocationLedger {
public:
    static AllocationLedger& instance();

    void record_alloc(void* ptr, size_t size, const char* file, int line);
    bool record_free(void* ptr, const char* file, int line);
    bool validate_access(const void* ptr, size_t size, const char* file, int line);

    size_t dump_unfreed_leaks();

private:
    AllocationLedger() = default;
    ~AllocationLedger() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<uintptr_t, RuntimeBlock> blocks_;
};

} // namespace safe_c::rt

namespace safe_cpp::rt {
    using namespace safe_c::rt;
}

namespace memcheck::rt {
    using namespace safe_c::rt;
}

#endif // SAFE_C_RUNTIME_LEDGER_H
