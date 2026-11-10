#include "ledger.h"
#include "logger.h"
#include <mutex>
#include <iostream>

namespace safe_c::rt {

AllocationLedger& AllocationLedger::instance() {
    static AllocationLedger inst;
    return inst;
}

void AllocationLedger::record_alloc(void* ptr, size_t size, const char* file, int line) {
    if (!ptr) return;
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

    std::unique_lock<std::shared_mutex> lock(mutex_);
    RuntimeBlock block;
    block.base_address = addr;
    block.size = size;
    block.alloc_location = (file ? std::string(file) : "<unknown>") + ":" + std::to_string(line);
    block.is_freed = false;

    blocks_[addr] = std::move(block);
}

bool AllocationLedger::record_free(void* ptr, const char* file, int line) {
    if (!ptr) return true; // Standard C permits free(NULL)
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    std::string current_loc = (file ? std::string(file) : "<unknown>") + ":" + std::to_string(line);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    // 1. Check exact base match
    auto it = blocks_.find(addr);
    if (it != blocks_.end()) {
        if (it->second.is_freed) {
            // DOUBLE FREE!
            LogEntry entry;
            entry.violation = ViolationType::DoubleFree;
            entry.target_address = ptr;
            entry.base_address = ptr;
            entry.allocated_size = it->second.size;
            entry.alloc_site = it->second.alloc_location;
            entry.free_site = it->second.free_location;
            entry.callstack = capture_stacktrace();
            entry.explanation = "Pointer was already deallocated previously and deallocation was attempted again.";
            Logger::instance().log_violation(entry);
            return false;
        }

        // Mark as freed
        it->second.is_freed = true;
        it->second.free_location = std::move(current_loc);
        return true;
    }

    // 2. Check if pointer points to an interior offset of an existing block: (base < addr < base + size)
    for (const auto& [base, block] : blocks_) {
        if (addr > base && addr < base + block.size) {
            // ALTERED BASE FREE!
            LogEntry entry;
            entry.violation = ViolationType::ModifiedBaseFree;
            entry.target_address = ptr;
            entry.base_address = reinterpret_cast<const void*>(base);
            entry.offset_bytes = static_cast<int64_t>(addr - base);
            entry.allocated_size = block.size;
            entry.alloc_site = block.alloc_location;
            entry.callstack = capture_stacktrace();
            entry.explanation = "Deallocation invoked on an interior pointer. Pointer arithmetic was applied without restoring base address.";
            Logger::instance().log_violation(entry);
            return false;
        }
    }

    // 3. Unallocated pointer free
    LogEntry entry;
    entry.violation = ViolationType::UnallocatedFree;
    entry.target_address = ptr;
    entry.callstack = capture_stacktrace();
    entry.explanation = "Attempt to deallocate a wild pointer, stack memory, or address not returned by allocator.";
    Logger::instance().log_violation(entry);
    return false;
}

bool AllocationLedger::validate_access(const void* ptr, size_t size, const char* file, int line) {
    if (!ptr) return true;
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    std::string access_loc = (file ? std::string(file) : "<unknown>") + ":" + std::to_string(line);

    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& [base, block] : blocks_) {
        if (addr >= base && addr + size <= base + block.size) {
            if (block.is_freed) {
                // USE AFTER FREE!
                LogEntry entry;
                entry.violation = ViolationType::UseAfterFree;
                entry.target_address = ptr;
                entry.base_address = reinterpret_cast<const void*>(base);
                entry.allocated_size = block.size;
                entry.alloc_site = block.alloc_location;
                entry.free_site = block.free_location;
                entry.callstack = capture_stacktrace();
                entry.explanation = "Memory access attempted on a heap block that has already been deallocated at " + access_loc + ".";
                Logger::instance().log_violation(entry);
                return false;
            }
            return true;
        }
    }
    return true;
}

size_t AllocationLedger::dump_unfreed_leaks() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t leak_count = 0;
    size_t total_leak_bytes = 0;

    for (const auto& [base, block] : blocks_) {
        if (!block.is_freed) {
            leak_count++;
            total_leak_bytes += block.size;
        }
    }

    if (leak_count > 0) {
        Logger::instance().log_leak_summary(leak_count, total_leak_bytes);
    }
    return leak_count;
}

} // namespace memcheck::rt
