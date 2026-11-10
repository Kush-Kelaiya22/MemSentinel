#include "pointer_state.h"
#include <algorithm>

namespace safe_cpp::core {

StateTable::StateTable(const StateTable& other)
    : next_cell_id_(other.next_cell_id_) {
    // Deep clone allocation cells
    std::unordered_map<uint64_t, std::shared_ptr<AllocationCell>> cell_map;
    for (const auto& cell : other.all_cells_) {
        auto cloned_cell = std::make_shared<AllocationCell>(*cell);
        all_cells_.push_back(cloned_cell);
        cell_map[cell->cell_id] = cloned_cell;
    }

    // Clone bindings pointing to cloned cells
    for (const auto& [name, binding] : other.bindings_) {
        if (binding) {
            std::shared_ptr<AllocationCell> target_cell;
            if (binding->cell()) {
                auto it = cell_map.find(binding->cell()->cell_id);
                if (it != cell_map.end()) {
                    target_cell = it->second;
                }
            }
            bindings_[name] = std::make_unique<PointerBinding>(
                name, target_cell, binding->offset_bytes()
            );
        }
    }
}

StateTable& StateTable::operator=(const StateTable& other) {
    if (this != &other) {
        StateTable temp(other);
        *this = std::move(temp);
    }
    return *this;
}

std::shared_ptr<AllocationCell> StateTable::register_allocation(
    const std::string& symbol,
    SourceLocation loc,
    size_t size_bytes,
    bool is_array
) {
    auto cell = std::make_shared<AllocationCell>(next_cell_id_++, std::move(loc), size_bytes, is_array);
    all_cells_.push_back(cell);
    bindings_[symbol] = std::make_unique<PointerBinding>(symbol, cell, 0);
    return cell;
}

void StateTable::register_alias(
    const std::string& target,
    const std::string& source,
    int64_t offset_delta
) {
    auto it = bindings_.find(source);
    if (it != bindings_.end() && it->second) {
        int64_t new_offset = it->second->offset_bytes() + offset_delta;
        bindings_[target] = std::make_unique<PointerBinding>(target, it->second->cell(), new_offset);
    }
}

void StateTable::apply_pointer_arithmetic(const std::string& symbol, int64_t byte_delta) {
    auto it = bindings_.find(symbol);
    if (it != bindings_.end() && it->second) {
        it->second->add_offset(byte_delta);
    }
}

std::optional<ViolationType> StateTable::register_free(
    const std::string& symbol,
    SourceLocation loc,
    std::shared_ptr<AllocationCell>& out_cell,
    int64_t& out_offset
) {
    auto it = bindings_.find(symbol);
    if (it == bindings_.end() || !it->second || !it->second->cell()) {
        out_cell = nullptr;
        out_offset = 0;
        return ViolationType::UnallocatedFree;
    }

    auto& binding = it->second;
    out_cell = binding->cell();
    out_offset = binding->offset_bytes();

    // Invariant Check 1: Pointer Alteration Before Free (Interior Offset Free)
    if (binding->has_offset()) {
        return ViolationType::ModifiedBaseFree;
    }

    // Invariant Check 2: Double Free
    if (out_cell->state == Typestate::Freed) {
        return ViolationType::DoubleFree;
    }

    // Successfully transition state to FREED
    out_cell->state = Typestate::Freed;
    out_cell->free_site = std::move(loc);
    return std::nullopt;
}

std::optional<ViolationType> StateTable::validate_access(
    const std::string& symbol,
    std::shared_ptr<AllocationCell>& out_cell
) const {
    auto it = bindings_.find(symbol);
    if (it == bindings_.end() || !it->second || !it->second->cell()) {
        out_cell = nullptr;
        return std::nullopt; // Unmonitored pointer
    }

    out_cell = it->second->cell();
    if (out_cell->state == Typestate::Freed) {
        return ViolationType::UseAfterFree;
    }

    return std::nullopt;
}

void StateTable::mark_escaped(const std::string& symbol) {
    auto it = bindings_.find(symbol);
    if (it != bindings_.end() && it->second && it->second->cell()) {
        it->second->cell()->escaped = true;
    }
}

std::vector<std::pair<std::string, std::shared_ptr<AllocationCell>>> StateTable::find_leaks() const {
    std::vector<std::pair<std::string, std::shared_ptr<AllocationCell>>> leaks;
    for (const auto& [symbol, binding] : bindings_) {
        if (binding && binding->cell()) {
            const auto& cell = binding->cell();
            if (cell->state == Typestate::Allocated && !cell->escaped) {
                leaks.emplace_back(symbol, cell);
            }
        }
    }
    return leaks;
}

const PointerBinding* StateTable::get_binding(const std::string& symbol) const {
    auto it = bindings_.find(symbol);
    if (it != bindings_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool StateTable::has_symbol(const std::string& symbol) const {
    return bindings_.find(symbol) != bindings_.end();
}

} // namespace safe_cpp::core
