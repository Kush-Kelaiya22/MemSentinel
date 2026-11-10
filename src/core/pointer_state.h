#ifndef MEMCHECK_CORE_POINTER_STATE_H
#define MEMCHECK_CORE_POINTER_STATE_H

#include "memcheck/common.h"
#include "memcheck/typestate.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <vector>

namespace safe_cpp::core {

// Memory Allocation Cell represents an underlying heap chunk
struct AllocationCell {
    uint64_t cell_id{0};
    SourceLocation alloc_site;
    size_t size_bytes{0};
    Typestate state{Typestate::Allocated};
    SourceLocation free_site;
    bool is_array{false};
    bool escaped{false};

    AllocationCell(uint64_t id, SourceLocation loc, size_t size, bool array = false)
        : cell_id(id), alloc_site(std::move(loc)), size_bytes(size),
          state(Typestate::Allocated), is_array(array) {}
};

// Pointer Binding represents a variable or expression pointing to an AllocationCell
class PointerBinding {
public:
    PointerBinding(std::string symbol, std::shared_ptr<AllocationCell> cell, int64_t offset = 0)
        : symbol_name_(std::move(symbol)), cell_(std::move(cell)), offset_bytes_(offset) {}

    [[nodiscard]] const std::string& symbol_name() const noexcept { return symbol_name_; }
    [[nodiscard]] std::shared_ptr<AllocationCell> cell() const noexcept { return cell_; }
    [[nodiscard]] int64_t offset_bytes() const noexcept { return offset_bytes_; }
    [[nodiscard]] bool has_offset() const noexcept { return offset_bytes_ != 0; }

    void add_offset(int64_t delta) noexcept { offset_bytes_ += delta; }
    void set_offset(int64_t offset) noexcept { offset_bytes_ = offset; }

private:
    std::string symbol_name_;
    std::shared_ptr<AllocationCell> cell_;
    int64_t offset_bytes_{0};
};

// State Table tracking all active pointer bindings and allocation cells in a scope
class StateTable {
public:
    StateTable() = default;
    ~StateTable() = default;

    // Non-copyable, movable
    StateTable(const StateTable& other);
    StateTable& operator=(const StateTable& other);
    StateTable(StateTable&&) noexcept = default;
    StateTable& operator=(StateTable&&) noexcept = default;

    // Register a new dynamic allocation
    std::shared_ptr<AllocationCell> register_allocation(
        const std::string& symbol,
        SourceLocation loc,
        size_t size_bytes,
        bool is_array = false
    );

    // Register an alias: target = source + offset
    void register_alias(const std::string& target, const std::string& source, int64_t offset_delta = 0);

    // Apply pointer arithmetic: ptr += delta
    void apply_pointer_arithmetic(const std::string& symbol, int64_t byte_delta);

    // Perform deallocation
    std::optional<ViolationType> register_free(
        const std::string& symbol,
        SourceLocation loc,
        std::shared_ptr<AllocationCell>& out_cell,
        int64_t& out_offset
    );

    // Validate dereference access (*ptr, ptr->field, ptr[i])
    std::optional<ViolationType> validate_access(
        const std::string& symbol,
        std::shared_ptr<AllocationCell>& out_cell
    ) const;

    // Mark pointer as escaped (passed to external function or returned)
    void mark_escaped(const std::string& symbol);

    // Find any unescaped memory leaks remaining at scope exit
    std::vector<std::pair<std::string, std::shared_ptr<AllocationCell>>> find_leaks() const;

    [[nodiscard]] const PointerBinding* get_binding(const std::string& symbol) const;
    [[nodiscard]] bool has_symbol(const std::string& symbol) const;

private:
    uint64_t next_cell_id_{1};
    std::unordered_map<std::string, std::unique_ptr<PointerBinding>> bindings_;
    std::vector<std::shared_ptr<AllocationCell>> all_cells_;
};

} // namespace safe_cpp::core

namespace memcheck::core {
    using namespace safe_cpp::core;
}

#endif // MEMCHECK_CORE_POINTER_STATE_H
