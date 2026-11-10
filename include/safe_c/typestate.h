#ifndef SAFE_C_TYPESTATE_H
#define SAFE_C_TYPESTATE_H

#include "safe_c/common.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <cstdint>

namespace safe_c {

enum class Typestate {
    Unallocated,
    Allocated,
    Freed,
    InteriorOffset,
    Escaped,
    Unknown
};

constexpr std::string_view typestate_name(Typestate st) noexcept {
    switch (st) {
        case Typestate::Unallocated:    return "UNALLOCATED";
        case Typestate::Allocated:      return "ALLOCATED";
        case Typestate::Freed:          return "FREED";
        case Typestate::InteriorOffset: return "INTERIOR_OFFSET";
        case Typestate::Escaped:        return "ESCAPED";
        case Typestate::Unknown:        return "UNKNOWN";
    }
    return "INVALID";
}

struct SourceLocation {
    std::string filename;
    uint32_t line{0};
    uint32_t column{0};

    [[nodiscard]] std::string to_string() const {
        if (filename.empty()) return "<unknown location>";
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

struct PointerRecord {
    std::string symbol_name;
    Typestate state{Typestate::Unallocated};
    SourceLocation alloc_location;
    SourceLocation free_location;
    int64_t byte_offset{0};
    std::string base_symbol;
    std::unordered_set<std::string> aliases;

    [[nodiscard]] bool is_allocated() const noexcept {
        return state == Typestate::Allocated;
    }

    [[nodiscard]] bool is_freed() const noexcept {
        return state == Typestate::Freed;
    }

    [[nodiscard]] bool is_interior_offset() const noexcept {
        return state == Typestate::InteriorOffset;
    }
};

struct DiagnosticReport {
    ViolationType violation;
    std::string symbol_name;
    SourceLocation primary_loc;
    SourceLocation secondary_loc;
    std::string explanation;
    int64_t byte_offset{0};
};

} // namespace safe_c

namespace safe_cpp {
    using namespace safe_c;
}

namespace memcheck {
    using namespace safe_c;
}

#endif // SAFE_C_TYPESTATE_H
