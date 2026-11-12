#ifndef SAFE_C_CORE_POINTER_STATE_H
#define SAFE_C_CORE_POINTER_STATE_H

#include "safe_c/common.h"
#include "safe_c/typestate.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <vector>

namespace safe_c::core {

// Memory Allocation Cell represents an underlying heap chunk or tracked pointer target
struct AllocationCell {
    uint64_t cell_id{0};
    SourceLocation alloc_site;
    size_t size_bytes{0};
    Typestate state{Typestate::Allocated};
    SourceLocation free_site;
    bool is_array{false};
    bool escaped{false};
    bool size_known{false};
    bool maybe_freed{false};
    int alloc_scope_depth{0};
    AllocKind kind{AllocKind::Malloc};

    AllocationCell(uint64_t id, SourceLocation loc, size_t size, bool array = false,
                   int scope_depth = 0, AllocKind alloc_kind = AllocKind::Malloc, bool known_size = false)
        : cell_id(id), alloc_site(std::move(loc)), size_bytes(size),
          state(Typestate::Allocated), is_array(array), size_known(known_size),
          alloc_scope_depth(scope_depth), kind(alloc_kind) {}
};

// Pointer Binding represents a variable or expression pointing to an AllocationCell
class PointerBinding {
public:
    PointerBinding(std::string symbol, std::shared_ptr<AllocationCell> cell, int64_t offset = 0, int scope_depth = 0)
        : symbol_name_(std::move(symbol)), cell_(std::move(cell)), offset_bytes_(offset), scope_depth_(scope_depth) {}

    [[nodiscard]] const std::string& symbol_name() const noexcept { return symbol_name_; }
    [[nodiscard]] std::shared_ptr<AllocationCell> cell() const noexcept { return cell_; }
    [[nodiscard]] int64_t offset_bytes() const noexcept { return offset_bytes_; }
    [[nodiscard]] bool has_offset() const noexcept { return offset_bytes_ != 0; }
    [[nodiscard]] int scope_depth() const noexcept { return scope_depth_; }

    void add_offset(int64_t delta) noexcept { offset_bytes_ += delta; }
    void set_offset(int64_t offset) noexcept { offset_bytes_ = offset; }
    void set_scope_depth(int depth) noexcept { scope_depth_ = depth; }

private:
    std::string symbol_name_;
    std::shared_ptr<AllocationCell> cell_;
    int64_t offset_bytes_{0};
    int scope_depth_{0};
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

    // Scope management
    void enter_scope() noexcept { ++current_scope_depth_; }
    std::vector<std::pair<std::string, std::shared_ptr<AllocationCell>>> exit_scope();
    [[nodiscard]] int current_scope() const noexcept { return current_scope_depth_; }

    // Register a new dynamic allocation
    std::shared_ptr<AllocationCell> register_allocation(
        const std::string& symbol,
        SourceLocation loc,
        size_t size_bytes,
        bool is_array = false,
        int scope_depth = 0,
        AllocKind kind = AllocKind::Malloc,
        bool size_known = false
    );

    // Register a non-heap special pointer target (Stack, Null, Uninitialized)
    std::shared_ptr<AllocationCell> register_special(
        const std::string& symbol,
        SourceLocation loc,
        AllocKind kind,
        int scope_depth = 0
    );

    // Mark a symbol's underlying cell as Null (e.g. on a null-branch edge)
    void mark_null(const std::string& symbol);

    // Restore a cell from Freed back to Allocated (e.g. when realloc returns NULL)
    void restore_allocated(const std::string& symbol);

    // Register an alias: target = source + offset
    void register_alias(const std::string& target, const std::string& source, int64_t offset_delta = 0, int scope_depth = 0);

    // Check if any variable bound to cell (excluding exclude_symbol) retains base address (offset == 0)
    [[nodiscard]] bool has_base_variable(const std::shared_ptr<AllocationCell>& cell, const std::string& exclude_symbol = "") const;

    // Checks whether altering symbol's offset by byte_delta is permitted
    // (permitted as long as there is a variable that contains the original base address)
    [[nodiscard]] bool can_alter_offset(const std::string& symbol, int64_t byte_delta) const;

    // Apply pointer arithmetic: ptr += delta
    void apply_pointer_arithmetic(const std::string& symbol, int64_t byte_delta);

    // Perform deallocation
    std::optional<ViolationType> register_free(
        const std::string& symbol,
        SourceLocation loc,
        std::shared_ptr<AllocationCell>& out_cell,
        int64_t& out_offset,
        AllocKind expected_kind = AllocKind::Unknown
    );

    // Validate dereference access (*ptr, ptr->field, ptr[i]) and optional spatial bounds
    std::optional<ViolationType> validate_access(
        const std::string& symbol,
        std::shared_ptr<AllocationCell>& out_cell,
        int64_t extra_offset = 0,
        size_t access_size = 0
    ) const;

    // Mark pointer as escaped (passed to external function or returned)
    void mark_escaped(const std::string& symbol);

    // Clear a symbol's binding (e.g. when reassigned to an unrelated value)
    void clear_binding(const std::string& symbol);

    // Merge another incoming CFG state into this state (for join blocks)
    void merge_from(const StateTable& other);

    // Find any unescaped memory leaks remaining at scope exit
    std::vector<std::pair<std::string, std::shared_ptr<AllocationCell>>> find_leaks() const;

    [[nodiscard]] const PointerBinding* get_binding(const std::string& symbol) const;
    [[nodiscard]] bool has_symbol(const std::string& symbol) const;

    void clear() {
        bindings_.clear();
        all_cells_.clear();
        next_cell_id_ = 1;
        current_scope_depth_ = 0;
    }

private:
    uint64_t next_cell_id_{1};
    int current_scope_depth_{0};
    std::unordered_map<std::string, std::unique_ptr<PointerBinding>> bindings_;
    std::vector<std::shared_ptr<AllocationCell>> all_cells_;
};

} // namespace safe_c::core

namespace safe_cpp::core {
    using namespace safe_c::core;
}

namespace memcheck::core {
    using namespace safe_c::core;
}

#endif // SAFE_C_CORE_POINTER_STATE_H
