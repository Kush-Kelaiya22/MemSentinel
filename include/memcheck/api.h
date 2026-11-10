#ifndef MEMCHECK_API_H
#define MEMCHECK_API_H

#include "memcheck/common.h"
#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Runtime safety net initialization and shutdown
MEMCHECK_API void memcheck_init(void);
MEMCHECK_API void memcheck_shutdown(void);

// Allocation & deallocation tracking hooks
MEMCHECK_API void memcheck_record_alloc(void* ptr, size_t size, const char* file, int line);
MEMCHECK_API void memcheck_record_free(void* ptr, const char* file, int line);
MEMCHECK_API void memcheck_record_realloc(void* old_ptr, void* new_ptr, size_t new_size, const char* file, int line);

// Memory access validation hook (guards against Use-After-Free at runtime)
MEMCHECK_API void memcheck_validate_access(const void* ptr, size_t size, const char* file, int line);

// Diagnostic logger & options configuration
MEMCHECK_API void memcheck_set_log_path(const char* log_filepath);
MEMCHECK_API const char* memcheck_get_log_path(void);
MEMCHECK_API void memcheck_set_mode(int mode); // 0 = Disabled, 1 = WarningOnly, 2 = FatalError
MEMCHECK_API int  memcheck_is_active(void);

// Explicit manual dump of active allocations (for debugging / custom leak checks)
MEMCHECK_API size_t memcheck_dump_unfreed(void);

#ifdef __cplusplus
}
#endif

#endif // MEMCHECK_API_H
