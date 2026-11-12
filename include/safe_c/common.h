#ifndef SAFE_C_COMMON_H
#define SAFE_C_COMMON_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#define SAFE_C_VERSION_MAJOR 1
#define SAFE_C_VERSION_MINOR 1
#define SAFE_C_VERSION_PATCH 0
#define SAFE_C_VERSION_STRING "1.1.0"

// Minimum supported GCC version: GCC 4.5.0 introduced -fplugin support
#define SAFE_C_MIN_GCC_MAJOR 4
#define SAFE_C_MIN_GCC_MINOR 5

#define SAFE_CPP_MIN_GCC_MAJOR SAFE_C_MIN_GCC_MAJOR
#define SAFE_CPP_MIN_GCC_MINOR SAFE_C_MIN_GCC_MINOR
#define MEMCHECK_MIN_GCC_MAJOR SAFE_C_MIN_GCC_MAJOR
#define MEMCHECK_MIN_GCC_MINOR SAFE_C_MIN_GCC_MINOR
#define MEMCHECK_VERSION_MAJOR SAFE_C_VERSION_MAJOR
#define MEMCHECK_VERSION_MINOR SAFE_C_VERSION_MINOR
#define MEMCHECK_VERSION_PATCH SAFE_C_VERSION_PATCH
#define MEMCHECK_VERSION_STRING SAFE_C_VERSION_STRING

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(SAFE_C_BUILD_DLL) || defined(SAFE_CPP_BUILD_DLL)
    #define SAFE_C_API __declspec(dllexport)
  #elif defined(SAFE_C_USE_DLL) || defined(SAFE_CPP_USE_DLL)
    #define SAFE_C_API __declspec(dllimport)
  #else
    #define SAFE_C_API
  #endif
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define SAFE_C_API __attribute__((visibility("default")))
  #else
    #define SAFE_C_API
  #endif
#endif

#define SAFE_CPP_API SAFE_C_API
#define MEMCHECK_API SAFE_C_API

namespace safe_c {

enum class ViolationType {
    DoubleFree,
    UseAfterFree,
    MemoryLeakUnfreed,
    ModifiedBaseFree,
    UnallocatedFree,
    DoubleAllocation,
    OutOfBoundsAccess,
    NullPointerDeref,
    UninitializedPointer,
    DanglingStackReturn,
    MismatchedDealloc,
    IteratorInvalidation
};

constexpr std::string_view violation_name(ViolationType type) noexcept {
    switch (type) {
        case ViolationType::DoubleFree: return "DOUBLE_FREE";
        case ViolationType::UseAfterFree: return "USE_AFTER_FREE";
        case ViolationType::MemoryLeakUnfreed: return "MEMORY_LEAK_UNFREED";
        case ViolationType::ModifiedBaseFree: return "MODIFIED_BASE_FREE";
        case ViolationType::UnallocatedFree: return "UNALLOCATED_FREE";
        case ViolationType::DoubleAllocation: return "DOUBLE_ALLOCATION";
        case ViolationType::OutOfBoundsAccess: return "OUT_OF_BOUNDS_ACCESS";
        case ViolationType::NullPointerDeref: return "NULL_POINTER_DEREF";
        case ViolationType::UninitializedPointer: return "UNINITIALIZED_POINTER";
        case ViolationType::DanglingStackReturn: return "DANGLING_STACK_RETURN";
        case ViolationType::MismatchedDealloc: return "MISMATCHED_DEALLOC";
        case ViolationType::IteratorInvalidation: return "ITERATOR_INVALIDATION";
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
    const char* log_file_path = "safe_c_runtime.log";
};

} // namespace safe_c

namespace safe_cpp {
    using namespace safe_c;
}

namespace memcheck {
    using namespace safe_c;
}

#endif // SAFE_C_COMMON_H
