#ifndef SAFE_C_TYPESTATE_H
#define SAFE_C_TYPESTATE_H

#include "safe_c/common.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <cstdint>
#include <filesystem>
#include <cctype>

namespace safe_c {

// Canonicalize and resolve true file path across ./, .\, .., symlinks, and casing
inline std::string resolve_canonical_path(std::string_view path_str) {
    if (path_str.empty()) return {};
    std::error_code ec;
    std::filesystem::path p(path_str);
    std::filesystem::path canon;
    if (std::filesystem::exists(p, ec)) {
        canon = std::filesystem::canonical(p, ec);
        if (ec) {
            canon = std::filesystem::weakly_canonical(p, ec);
        }
    } else {
        canon = std::filesystem::weakly_canonical(p, ec);
    }
    std::string res = canon.lexically_normal().generic_string();
#if defined(_WIN32)
    for (char& c : res) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
#endif
    return res;
}

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
    std::string canonical_path;
    uint32_t line{0};
    uint32_t column{0};
    uint32_t length{1};

    SourceLocation() = default;
    SourceLocation(std::string fn, uint32_t l, uint32_t c, uint32_t len = 1)
        : filename(std::move(fn)), canonical_path(resolve_canonical_path(filename)),
          line(l), column(c), length(len) {}

    SourceLocation(std::string fn, std::string canon, uint32_t l, uint32_t c, uint32_t len = 1)
        : filename(std::move(fn)), canonical_path(std::move(canon)),
          line(l), column(c), length(len) {}

    [[nodiscard]] std::string to_string() const {
        if (filename.empty()) return "<unknown location>";
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }

    [[nodiscard]] bool matches_file(const SourceLocation& other) const {
        if (!canonical_path.empty() && !other.canonical_path.empty()) {
            return canonical_path == other.canonical_path;
        }
        return filename == other.filename;
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
