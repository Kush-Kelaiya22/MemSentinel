#include "memcheck/api.h"
#include "ledger.h"
#include "logger.h"
#include <atomic>
#include <cstdlib>

namespace {
std::atomic<bool> g_memcheck_active{true};
std::atomic<int> g_memcheck_mode{2}; // 2 = FatalError

void __attribute__((constructor)) memcheck_runtime_startup(void) {
    const char* env_disable = std::getenv("MEMCHECK_DISABLE");
    if (env_disable && (*env_disable == '1' || *env_disable == 'y' || *env_disable == 'Y')) {
        g_memcheck_active.store(false);
    }
}

void __attribute__((destructor)) memcheck_runtime_shutdown(void) {
    if (g_memcheck_active.load()) {
        memcheck::rt::AllocationLedger::instance().dump_unfreed_leaks();
    }
}
} // anonymous namespace

extern "C" {

void memcheck_init(void) {
    g_memcheck_active.store(true);
}

void memcheck_shutdown(void) {
    if (g_memcheck_active.load()) {
        memcheck::rt::AllocationLedger::instance().dump_unfreed_leaks();
        g_memcheck_active.store(false);
    }
}

void memcheck_record_alloc(void* ptr, size_t size, const char* file, int line) {
    if (!g_memcheck_active.load() || !ptr) return;
    memcheck::rt::AllocationLedger::instance().record_alloc(ptr, size, file, line);
}

void memcheck_record_free(void* ptr, const char* file, int line) {
    if (!g_memcheck_active.load() || !ptr) return;
    bool ok = memcheck::rt::AllocationLedger::instance().record_free(ptr, file, line);
    if (!ok && g_memcheck_mode.load() == 2) {
        std::abort();
    }
}

void memcheck_record_realloc(void* old_ptr, void* new_ptr, size_t new_size, const char* file, int line) {
    if (!g_memcheck_active.load()) return;
    if (old_ptr) {
        memcheck::rt::AllocationLedger::instance().record_free(old_ptr, file, line);
    }
    if (new_ptr) {
        memcheck::rt::AllocationLedger::instance().record_alloc(new_ptr, new_size, file, line);
    }
}

void memcheck_validate_access(const void* ptr, size_t size, const char* file, int line) {
    if (!g_memcheck_active.load() || !ptr) return;
    bool ok = memcheck::rt::AllocationLedger::instance().validate_access(ptr, size, file, line);
    if (!ok && g_memcheck_mode.load() == 2) {
        std::abort();
    }
}

void memcheck_set_log_path(const char* log_filepath) {
    if (log_filepath && *log_filepath) {
        memcheck::rt::Logger::instance().set_log_path(log_filepath);
    }
}

const char* memcheck_get_log_path(void) {
    return memcheck::rt::Logger::instance().get_log_path().c_str();
}

void memcheck_set_mode(int mode) {
    g_memcheck_mode.store(mode);
}

int memcheck_is_active(void) {
    return g_memcheck_active.load() ? 1 : 0;
}

size_t memcheck_dump_unfreed(void) {
    return memcheck::rt::AllocationLedger::instance().dump_unfreed_leaks();
}

} // extern "C"
