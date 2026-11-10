#ifndef SAFE_CPP_API_H
#define SAFE_CPP_API_H

#include "safe_cpp/common.h"
#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Runtime safety net initialization and shutdown
SAFE_CPP_API void safe_cpp_init(void);
SAFE_CPP_API void safe_cpp_shutdown(void);

// Allocation & deallocation tracking hooks
SAFE_CPP_API void safe_cpp_record_alloc(void* ptr, size_t size, const char* file, int line);
SAFE_CPP_API void safe_cpp_record_free(void* ptr, const char* file, int line);
SAFE_CPP_API void safe_cpp_record_realloc(void* old_ptr, void* new_ptr, size_t new_size, const char* file, int line);

// Memory access validation hook (guards against Use-After-Free at runtime)
SAFE_CPP_API void safe_cpp_validate_access(const void* ptr, size_t size, const char* file, int line);

// Diagnostic logger & options configuration
SAFE_CPP_API void safe_cpp_set_log_path(const char* log_filepath);
SAFE_CPP_API const char* safe_cpp_get_log_path(void);
SAFE_CPP_API void safe_cpp_set_mode(int mode); // 0 = Disabled, 1 = WarningOnly, 2 = FatalError
SAFE_CPP_API int  safe_cpp_is_active(void);

// Explicit manual dump of active allocations (for debugging / custom leak checks)
SAFE_CPP_API size_t safe_cpp_dump_unfreed(void);

// Legacy aliases for backwards compatibility
SAFE_CPP_API void memcheck_init(void);
SAFE_CPP_API void memcheck_shutdown(void);
SAFE_CPP_API void memcheck_record_alloc(void* ptr, size_t size, const char* file, int line);
SAFE_CPP_API void memcheck_record_free(void* ptr, const char* file, int line);
SAFE_CPP_API void memcheck_record_realloc(void* old_ptr, void* new_ptr, size_t new_size, const char* file, int line);
SAFE_CPP_API void memcheck_validate_access(const void* ptr, size_t size, const char* file, int line);
SAFE_CPP_API void memcheck_set_log_path(const char* log_filepath);
SAFE_CPP_API const char* memcheck_get_log_path(void);
SAFE_CPP_API void memcheck_set_mode(int mode);
SAFE_CPP_API int  memcheck_is_active(void);
SAFE_CPP_API size_t memcheck_dump_unfreed(void);

#ifdef __cplusplus
}
#endif

#endif // SAFE_CPP_API_H
