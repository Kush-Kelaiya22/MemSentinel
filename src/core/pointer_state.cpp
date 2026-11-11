#include "pointer_state.h"
#include <algorithm>

namespace safe_c::core {

StateTable::StateTable(const StateTable& other)
    : next_cell_id_(other.next_cell_id_), current_scope_depth_(other.current_scope_depth_) {
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
                name, target_cell, binding->offset_bytes(), binding->scope_depth()
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

std::vector<std::pair<std::string, std::shared_ptr<AllocationCell>>> StateTable::exit_scope() {
    std::vector<std::pair<std::string, std::shared_ptr<AllocationCell>>> leaks;
    if (current_scope_depth_ <= 0) return leaks;

    std::vector<std::string> to_remove;
    std::unordered_set<uint64_t> reported_cells;

    for (const auto& [name, binding] : bindings_) {
        if (!binding) continue;
        if (binding->scope_depth() >= current_scope_depth_) {
            to_remove.push_back(name);
            const auto& cell = binding->cell();
            if (cell && cell->state == Typestate::Allocated && !cell->escaped) {
                // Check if any surviving outer binding references this cell
                bool has_outer_owner = false;
                for (const auto& [other_name, other_b] : bindings_) {
                    if (other_b && other_b->scope_depth() < current_scope_depth_ && other_b->cell() == cell) {
                        has_outer_owner = true;
                        break;
                    }
                }
                if (!has_outer_owner && reported_cells.insert(cell->cell_id).second) {
                    leaks.emplace_back(name, cell);
                    cell->escaped = true;
                }
            }
        }
    }

    for (const auto& sym : to_remove) {
        bindings_.erase(sym);
    }

    --current_scope_depth_;
    return leaks;
}

std::shared_ptr<AllocationCell> StateTable::register_allocation(
    const std::string& symbol,
    SourceLocation loc,
    size_t size_bytes,
    bool is_array,
    int scope_depth
) {
    int depth = (scope_depth > 0) ? scope_depth : current_scope_depth_;
    auto cell = std::make_shared<AllocationCell>(next_cell_id_++, std::move(loc), size_bytes, is_array, depth);
    all_cells_.push_back(cell);

    int binding_scope = depth;
    auto it = bindings_.find(symbol);
    if (it != bindings_.end() && it->second && it->second->scope_depth() < binding_scope) {
        binding_scope = it->second->scope_depth();
    }

    bindings_[symbol] = std::make_unique<PointerBinding>(symbol, cell, 0, binding_scope);
    return cell;
}

void StateTable::register_alias(
    const std::string& target,
    const std::string& source,
    int64_t offset_delta,
    int scope_depth
) {
    auto it = bindings_.find(source);
    if (it != bindings_.end() && it->second) {
        int64_t new_offset = it->second->offset_bytes() + offset_delta;
        int binding_scope = (scope_depth > 0) ? scope_depth : current_scope_depth_;
        auto target_it = bindings_.find(target);
        if (target_it != bindings_.end() && target_it->second && target_it->second->scope_depth() < binding_scope) {
            binding_scope = target_it->second->scope_depth();
        }
        bindings_[target] = std::make_unique<PointerBinding>(target, it->second->cell(), new_offset, binding_scope);
    }
}

bool StateTable::has_base_variable(const std::shared_ptr<AllocationCell>& cell, const std::string& exclude_symbol) const {
    if (!cell) return false;
    for (const auto& [name, binding] : bindings_) {
        if (!name.empty() && name != exclude_symbol && binding && binding->cell() == cell) {
            if (binding->offset_bytes() == 0) {
                return true;
            }
        }
    }
    return false;
}

bool StateTable::can_alter_offset(const std::string& symbol, int64_t byte_delta) const {
    auto it = bindings_.find(symbol);
    if (it == bindings_.end() || !it->second || !it->second->cell()) {
        return true; // Not an actively tracked heap cell
    }
    const auto& binding = it->second;
    int64_t target_offset = binding->offset_bytes() + byte_delta;
    if (target_offset == 0) {
        // Restoring to base address is always permitted
        return true;
    }
    // Non-zero offset is permitted as long as there is a variable containing the original base address
    return has_base_variable(binding->cell(), symbol);
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
    std::unordered_set<uint64_t> reported_cells;
    for (const auto& [symbol, binding] : bindings_) {
        if (binding && binding->cell()) {
            const auto& cell = binding->cell();
            if (cell->state == Typestate::Allocated && !cell->escaped) {
                if (reported_cells.insert(cell->cell_id).second) {
                    leaks.emplace_back(symbol, cell);
                }
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

} // namespace safe_c::core
