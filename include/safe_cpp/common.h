#ifndef SAFE_CPP_COMMON_H
#define SAFE_CPP_COMMON_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#define SAFE_CPP_VERSION_MAJOR 1
#define SAFE_CPP_VERSION_MINOR 0
#define SAFE_CPP_VERSION_PATCH 0
#define SAFE_CPP_VERSION_STRING "1.0.0"

// Minimum supported GCC version: GCC 4.5.0 introduced -fplugin support
#define SAFE_CPP_MIN_GCC_MAJOR 4
#define SAFE_CPP_MIN_GCC_MINOR 5

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(SAFE_CPP_BUILD_DLL)
    #define SAFE_CPP_API __declspec(dllexport)
  #elif defined(SAFE_CPP_USE_DLL)
    #define SAFE_CPP_API __declspec(dllimport)
  #else
    #define SAFE_CPP_API
  #endif
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define SAFE_CPP_API __attribute__((visibility("default")))
  #else
    #define SAFE_CPP_API
  #endif
#endif

#define MEMCHECK_API SAFE_CPP_API

namespace safe_cpp {

enum class ViolationType {
    DoubleFree,
    UseAfterFree,
    MemoryLeakUnfreed,
    ModifiedBaseFree,
    UnallocatedFree,
    DoubleAllocation
};

constexpr std::string_view violation_name(ViolationType type) noexcept {
    switch (type) {
        case ViolationType::DoubleFree: return "DOUBLE_FREE";
        case ViolationType::UseAfterFree: return "USE_AFTER_FREE";
        case ViolationType::MemoryLeakUnfreed: return "MEMORY_LEAK_UNFREED";
        case ViolationType::ModifiedBaseFree: return "MODIFIED_BASE_FREE";
        case ViolationType::UnallocatedFree: return "UNALLOCATED_FREE";
        case ViolationType::DoubleAllocation: return "DOUBLE_ALLOCATION";
    }
    return "UNKNOWN_VIOLATION";
}

enum class Mode {
    FatalError,
    WarningOnly,
    Disabled
};

struct Config {
    Mode mode = Mode::FatalError;
    bool enabled = true;
    bool check_double_free = true;
    bool check_use_after_free = true;
    bool check_leaks = true;
    bool check_pointer_arithmetic = true;
    bool enable_runtime_guard = true;
    const char* log_file_path = "safe_cpp_runtime.log";
};

} // namespace safe_cpp

#endif // SAFE_CPP_COMMON_H
