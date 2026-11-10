#include "logger.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <cstdlib>

namespace memcheck::rt {

namespace {
std::mutex g_log_mutex;

std::string get_current_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
} // anonymous namespace

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() {
    const char* env_path = std::getenv("MEMCHECK_LOG_PATH");
    if (env_path && *env_path) {
        log_filepath_ = env_path;
    }
}

void Logger::set_log_path(std::string path) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    log_filepath_ = std::move(path);
}

const std::string& Logger::get_log_path() const noexcept {
    return log_filepath_;
}

void Logger::log_violation(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(g_log_mutex);

    std::ofstream out(log_filepath_, std::ios::app);
    if (!out.is_open()) {
        std::cerr << "memcheck: fatal error: failed to open log file '" << log_filepath_ << "'\n";
        return;
    }

    out << "======================================================================\n"
        << "MEMCHECK RUNTIME SAFETY VIOLATION REPORT\n"
        << "Timestamp      : " << get_current_iso_timestamp() << "\n"
        << "Violation Type : " << violation_name(entry.violation) << "\n"
        << "Target Address : 0x" << std::hex << reinterpret_cast<uintptr_t>(entry.target_address) << std::dec << "\n";

    if (entry.base_address) {
        out << "Base Address   : 0x" << std::hex << reinterpret_cast<uintptr_t>(entry.base_address) << std::dec
            << " (Size: " << entry.allocated_size << " bytes)\n";
    }

    if (entry.offset_bytes != 0) {
        out << "Offset Shift   : " << entry.offset_bytes << " bytes from allocation base\n";
    }

    if (!entry.alloc_site.empty()) {
        out << "Allocated At   : " << entry.alloc_site << "\n";
    }

    if (!entry.free_site.empty()) {
        out << "Deallocated At : " << entry.free_site << "\n";
    }

    if (!entry.explanation.empty()) {
        out << "Reason Details : " << entry.explanation << "\n";
    }

    if (!entry.callstack.empty()) {
        out << "Execution Stack:\n" << format_stacktrace(entry.callstack);
    }

    out << "Action Taken   : Safety guardrail triggered. Execution halted safely.\n"
        << "======================================================================\n\n";
    out.flush();

    // Also print concise notice to stderr
    std::cerr << "memcheck: [RUNTIME GUARDRAIL] " << violation_name(entry.violation)
              << " detected at address 0x" << std::hex << reinterpret_cast<uintptr_t>(entry.target_address)
              << std::dec << ". Diagnostic details saved to '" << log_filepath_ << "'.\n";
}

void Logger::log_leak_summary(size_t leak_count, size_t total_bytes) {
    if (leak_count == 0) return;

    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::ofstream out(log_filepath_, std::ios::app);
    if (!out.is_open()) return;

    out << "======================================================================\n"
        << "MEMCHECK RUNTIME LEAK SUMMARY\n"
        << "Timestamp       : " << get_current_iso_timestamp() << "\n"
        << "Unfreed Blocks  : " << leak_count << "\n"
        << "Total Leak Bytes: " << total_bytes << " bytes\n"
        << "======================================================================\n\n";
}

} // namespace memcheck::rt
