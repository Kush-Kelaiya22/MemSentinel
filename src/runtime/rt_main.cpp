#include "safe_c/api.h"
#include "ledger.h"
#include "logger.h"
#include <atomic>
#include <cstdlib>

namespace {
std::atomic<bool> g_safe_c_active{true};
std::atomic<int> g_safe_c_mode{2}; // 2 = FatalError

void __attribute__((constructor)) safe_c_runtime_startup(void) {
    const char* env_disable = std::getenv("SAFE_C_DISABLE");
    if (!env_disable) env_disable = std::getenv("SAFE_CPP_DISABLE");
    if (!env_disable) env_disable = std::getenv("MEMCHECK_DISABLE");
    if (env_disable && (*env_disable == '1' || *env_disable == 'y' || *env_disable == 'Y')) {
        g_safe_c_active.store(false);
    }
}

void __attribute__((destructor)) safe_c_runtime_shutdown(void) {
    if (g_safe_c_active.load()) {
        safe_c::rt::AllocationLedger::instance().dump_unfreed_leaks();
    }
}
} // anonymous namespace

extern "C" {

void safe_c_init(void) {
    g_safe_c_active.store(true);
}

void safe_c_shutdown(void) {
    if (g_safe_c_active.load()) {
        safe_c::rt::AllocationLedger::instance().dump_unfreed_leaks();
        g_safe_c_active.store(false);
    }
}

void safe_c_record_alloc(void* ptr, size_t size, const char* file, int line) {
    if (!g_safe_c_active.load() || !ptr) return;
    safe_c::rt::AllocationLedger::instance().record_alloc(ptr, size, file, line);
}

void safe_c_record_free(void* ptr, const char* file, int line) {
    if (!g_safe_c_active.load() || !ptr) return;
    bool ok = safe_c::rt::AllocationLedger::instance().record_free(ptr, file, line);
    if (!ok && g_safe_c_mode.load() == 2) {
        std::abort();
    }
}

void safe_c_record_realloc(void* old_ptr, void* new_ptr, size_t new_size, const char* file, int line) {
    if (!g_safe_c_active.load()) return;
    if (old_ptr) {
        safe_c::rt::AllocationLedger::instance().record_free(old_ptr, file, line);
    }
    if (new_ptr) {
        safe_c::rt::AllocationLedger::instance().record_alloc(new_ptr, new_size, file, line);
    }
}

void safe_c_validate_access(const void* ptr, size_t size, const char* file, int line) {
    if (!g_safe_c_active.load() || !ptr) return;
    bool ok = safe_c::rt::AllocationLedger::instance().validate_access(ptr, size, file, line);
    if (!ok && g_safe_c_mode.load() == 2) {
        std::abort();
    }
}

void safe_c_set_log_path(const char* log_filepath) {
    if (log_filepath && *log_filepath) {
        safe_c::rt::Logger::instance().set_log_path(log_filepath);
    }
}

const char* safe_c_get_log_path(void) {
    return safe_c::rt::Logger::instance().get_log_path().c_str();
}

void safe_c_set_mode(int mode) {
    g_safe_c_mode.store(mode);
}

int safe_c_is_active(void) {
    return g_safe_c_active.load() ? 1 : 0;
}

size_t safe_c_dump_unfreed(void) {
    return safe_c::rt::AllocationLedger::instance().dump_unfreed_leaks();
}

// Backwards compatibility wrappers
void safe_cpp_init(void) { safe_c_init(); }
void safe_cpp_shutdown(void) { safe_c_shutdown(); }
void safe_cpp_record_alloc(void* ptr, size_t s, const char* f, int l) { safe_c_record_alloc(ptr, s, f, l); }
void safe_cpp_record_free(void* ptr, const char* f, int l) { safe_c_record_free(ptr, f, l); }
void safe_cpp_record_realloc(void* o, void* n, size_t s, const char* f, int l) { safe_c_record_realloc(o, n, s, f, l); }
void safe_cpp_validate_access(const void* p, size_t s, const char* f, int l) { safe_c_validate_access(p, s, f, l); }
void safe_cpp_set_log_path(const char* p) { safe_c_set_log_path(p); }
const char* safe_cpp_get_log_path(void) { return safe_c_get_log_path(); }
void safe_cpp_set_mode(int m) { safe_c_set_mode(m); }
int safe_cpp_is_active(void) { return safe_c_is_active(); }
size_t safe_cpp_dump_unfreed(void) { return safe_c_dump_unfreed(); }

void memcheck_init(void) { safe_c_init(); }
void memcheck_shutdown(void) { safe_c_shutdown(); }
void memcheck_record_alloc(void* ptr, size_t s, const char* f, int l) { safe_c_record_alloc(ptr, s, f, l); }
void memcheck_record_free(void* ptr, const char* f, int l) { safe_c_record_free(ptr, f, l); }
void memcheck_record_realloc(void* o, void* n, size_t s, const char* f, int l) { safe_c_record_realloc(o, n, s, f, l); }
void memcheck_validate_access(const void* p, size_t s, const char* f, int l) { safe_c_validate_access(p, s, f, l); }
void memcheck_set_log_path(const char* p) { safe_c_set_log_path(p); }
const char* memcheck_get_log_path(void) { return safe_c_get_log_path(); }
void memcheck_set_mode(int m) { safe_c_set_mode(m); }
int memcheck_is_active(void) { return safe_c_is_active(); }
size_t memcheck_dump_unfreed(void) { return safe_c_dump_unfreed(); }

} // extern "C"
