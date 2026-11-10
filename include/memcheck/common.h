#ifndef MEMCHECK_COMMON_H
#define MEMCHECK_COMMON_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#define MEMCHECK_VERSION_MAJOR 1
#define MEMCHECK_VERSION_MINOR 0
#define MEMCHECK_VERSION_PATCH 0
#define MEMCHECK_VERSION_STRING "1.0.0"

// Minimum supported GCC version: GCC 4.5.0 introduced -fplugin support
#define MEMCHECK_MIN_GCC_MAJOR 4
#define MEMCHECK_MIN_GCC_MINOR 5

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(MEMCHECK_BUILD_DLL)
    #define MEMCHECK_API __declspec(dllexport)
  #elif defined(MEMCHECK_USE_DLL)
    #define MEMCHECK_API __declspec(dllimport)
  #else
    #define MEMCHECK_API
  #endif
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define MEMCHECK_API __attribute__((visibility("default")))
  #else
    #define MEMCHECK_API
  #endif
#endif

namespace memcheck {

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

} // namespace memcheck

#endif // MEMCHECK_COMMON_H
