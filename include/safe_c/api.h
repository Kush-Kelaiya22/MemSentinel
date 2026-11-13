#ifndef SAFE_C_API_H
#define SAFE_C_API_H

#include "safe_c/common.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime safety net initialization and shutdown */
SAFE_C_API void safe_c_init(void);
SAFE_C_API void safe_c_shutdown(void);

/* Allocation & deallocation tracking hooks */
SAFE_C_API void safe_c_record_alloc(void* ptr, size_t size, const char* file, int line);
SAFE_C_API void safe_c_record_free(void* ptr, const char* file, int line);
SAFE_C_API void safe_c_record_realloc(void* old_ptr, void* new_ptr, size_t new_size, const char* file, int line);

/* Memory access validation hook (guards against Use-After-Free at runtime) */
SAFE_C_API void safe_c_validate_access(const void* ptr, size_t size, const char* file, int line);

/* Diagnostic logger & options configuration */
SAFE_C_API void safe_c_set_log_path(const char* log_filepath);
SAFE_C_API const char* safe_c_get_log_path(void);
SAFE_C_API void safe_c_set_mode(int mode); /* 0 = Disabled, 1 = WarningOnly, 2 = FatalError */
SAFE_C_API int  safe_c_is_active(void);

/* Explicit manual dump of active allocations */
SAFE_C_API size_t safe_c_dump_unfreed(void);

/* Backwards compatibility aliases */
SAFE_C_API void safe_cpp_init(void);
SAFE_C_API void safe_cpp_shutdown(void);
SAFE_C_API void safe_cpp_record_alloc(void* ptr, size_t size, const char* file, int line);
SAFE_C_API void safe_cpp_record_free(void* ptr, const char* file, int line);
SAFE_C_API void safe_cpp_record_realloc(void* old_ptr, void* new_ptr, size_t new_size, const char* file, int line);
SAFE_C_API void safe_cpp_validate_access(const void* ptr, size_t size, const char* file, int line);
SAFE_C_API void safe_cpp_set_log_path(const char* log_filepath);
SAFE_C_API const char* safe_cpp_get_log_path(void);
SAFE_C_API void safe_cpp_set_mode(int mode);
SAFE_C_API int  safe_cpp_is_active(void);
SAFE_C_API size_t safe_cpp_dump_unfreed(void);

SAFE_C_API void memcheck_init(void);
SAFE_C_API void memcheck_shutdown(void);
SAFE_C_API void memcheck_record_alloc(void* ptr, size_t size, const char* file, int line);
SAFE_C_API void memcheck_record_free(void* ptr, const char* file, int line);
SAFE_C_API void memcheck_record_realloc(void* old_ptr, void* new_ptr, size_t new_size, const char* file, int line);
SAFE_C_API void memcheck_validate_access(const void* ptr, size_t size, const char* file, int line);
SAFE_C_API void memcheck_set_log_path(const char* log_filepath);
SAFE_C_API const char* memcheck_get_log_path(void);
SAFE_C_API void memcheck_set_mode(int mode);
SAFE_C_API int  memcheck_is_active(void);
SAFE_C_API size_t memcheck_dump_unfreed(void);

#ifdef __cplusplus
}
#endif

#endif /* SAFE_C_API_H */
