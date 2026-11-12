#include "gimple_pass.h"
#include "pointer_state.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef HAVE_GCC_PLUGIN_HEADERS
  #include <gcc-plugin.h>
  #include <context.h>
  #include <tree.h>
  #include <gimple.h>
  #include <gimple-iterator.h>
  #include <tree-pass.h>
  #include <diagnostic-core.h>

#define SAFEC_DIAG(loc, fmt, ...)                                      \
    do {                                                               \
        if (safe_c::plugin::g_config.mode == safe_c::Mode::WarningOnly)\
            warning_at((loc), 0, (fmt), ##__VA_ARGS__);                \
        else                                                           \
            error_at((loc), (fmt), ##__VA_ARGS__);                     \
    } while (0)

namespace {

struct LocInfo {
    location_t loc{UNKNOWN_LOCATION};
    std::string name;
};

static bool cst_to_size(tree t, size_t& out) {
    if (!t || TREE_CODE(t) != INTEGER_CST || !tree_fits_uhwi_p(t)) {
        return false;
    }
    out = static_cast<size_t>(tree_to_uhwi(t));
    return true;
}

static bool cst_to_shwi(tree t, int64_t& out) {
    if (!t || TREE_CODE(t) != INTEGER_CST) {
        return false;
    }
    if (tree_fits_shwi_p(t)) {
        out = static_cast<int64_t>(tree_to_shwi(t));
        return true;
    }
    if (tree_fits_uhwi_p(t)) {
        out = static_cast<int64_t>(tree_to_uhwi(t));
        return true;
    }
    return false;
}

static bool is_dealloc_fn(const char* fn_name, const char* asm_name, safe_c::AllocKind& out_kind) {
    if (fn_name) {
        if (strcmp(fn_name, "free") == 0 || strcmp(fn_name, "cfree") == 0) {
            out_kind = safe_c::AllocKind::Malloc;
            return true;
        }
        if (strcmp(fn_name, "operator delete") == 0) {
            out_kind = safe_c::AllocKind::New;
            return true;
        }
        if (strcmp(fn_name, "operator delete[]") == 0) {
            out_kind = safe_c::AllocKind::NewArray;
            return true;
        }
    }
    if (asm_name) {
        if (strcmp(asm_name, "free") == 0 || strcmp(asm_name, "cfree") == 0) {
            out_kind = safe_c::AllocKind::Malloc;
            return true;
        }
        if (strncmp(asm_name, "_Zdl", 4) == 0) {
            out_kind = safe_c::AllocKind::New;
            return true;
        }
        if (strncmp(asm_name, "_Zda", 4) == 0) {
            out_kind = safe_c::AllocKind::NewArray;
            return true;
        }
    }
    return false;
}

static bool is_alloc_fn(const char* fn_name, const char* asm_name, safe_c::AllocKind& out_kind) {
    if (fn_name) {
        if (strcmp(fn_name, "malloc") == 0 || strcmp(fn_name, "calloc") == 0 ||
            strcmp(fn_name, "aligned_alloc") == 0 || strcmp(fn_name, "valloc") == 0) {
            out_kind = safe_c::AllocKind::Malloc;
            return true;
        }
        if (strcmp(fn_name, "operator new") == 0) {
            out_kind = safe_c::AllocKind::New;
            return true;
        }
        if (strcmp(fn_name, "operator new[]") == 0) {
            out_kind = safe_c::AllocKind::NewArray;
            return true;
        }
    }
    if (asm_name) {
        if (strcmp(asm_name, "malloc") == 0 || strcmp(asm_name, "calloc") == 0 ||
            strcmp(asm_name, "aligned_alloc") == 0 || strcmp(asm_name, "valloc") == 0) {
            out_kind = safe_c::AllocKind::Malloc;
            return true;
        }
        if (strncmp(asm_name, "_Zn", 3) == 0) {
            // Placement new (_ZnwmPv / _ZnamPv / etc.) constructs in-place and does not allocate.
            size_t len = strlen(asm_name);
            if (len >= 2 && asm_name[len - 2] == 'P' && asm_name[len - 1] == 'v') {
                return false;
            }
            out_kind = (asm_name[3] == 'a') ? safe_c::AllocKind::NewArray : safe_c::AllocKind::New;
            return true;
        }
    }
    return false;
}

static bool is_realloc_fn(const char* fn_name, const char* asm_name) {
    return (fn_name && strcmp(fn_name, "realloc") == 0) ||
           (asm_name && strcmp(asm_name, "realloc") == 0);
}

static bool is_non_escaping_fn(const char* fn_name) {
    if (!fn_name) return false;
    static const char* const kKnownFns[] = {
        "printf", "fprintf", "sprintf", "snprintf", "vprintf", "vfprintf",
        "puts", "fputs", "strlen", "strnlen", "strcmp", "strncmp", "strchr",
        "strrchr", "strstr", "memcmp", "memcpy", "memmove", "memset",
        "strcpy", "strncpy", "strcat", "strncat", "write", "send",
        "__builtin_memcpy", "__builtin_memmove", "__builtin_memset",
        "__builtin_strlen", "__builtin_strcmp", "__builtin_printf",
        "__builtin_puts"
    };
    for (const char* name : kKnownFns) {
        if (strcmp(fn_name, name) == 0) return true;
    }
    return false;
}

static bool is_mem_write_fn(const char* fn_name, int& out_ptr_arg, int& out_size_arg) {
    if (!fn_name) return false;
    if (strcmp(fn_name, "memcpy") == 0 || strcmp(fn_name, "memmove") == 0 ||
        strcmp(fn_name, "memset") == 0 || strcmp(fn_name, "__builtin_memcpy") == 0 ||
        strcmp(fn_name, "__builtin_memmove") == 0 || strcmp(fn_name, "__builtin_memset") == 0) {
        out_ptr_arg = 0;
        out_size_arg = 2;
        return true;
    }
    return false;
}

static const char* ssa_var_name(tree ssa) {
    if (!ssa || TREE_CODE(ssa) != SSA_NAME) return nullptr;
    tree var = SSA_NAME_VAR(ssa);
    if (var && DECL_NAME(var)) {
        return IDENTIFIER_POINTER(DECL_NAME(var));
    }
    return nullptr;
}

static std::string ssa_sym(tree ssa) {
    return "ptr_" + std::to_string(SSA_NAME_VERSION(ssa));
}

static bool extract_mem_ref(tree expr, tree& out_ptr, int64_t& out_offset, size_t& out_access_size) {
    if (!expr || TREE_CODE(expr) != MEM_REF) return false;
    tree op0 = TREE_OPERAND(expr, 0);
    if (!op0 || TREE_CODE(op0) != SSA_NAME) return false;

    out_ptr = op0;
    out_offset = 0;
    cst_to_shwi(TREE_OPERAND(expr, 1), out_offset);

    out_access_size = 0;
    cst_to_size(TYPE_SIZE_UNIT(TREE_TYPE(expr)), out_access_size);
    return true;
}

static void compute_rpo_dfs(basic_block bb, std::unordered_set<int>& visited, std::vector<basic_block>& postorder) {
    if (!bb || !visited.insert(bb->index).second) return;
    edge e;
    edge_iterator ei;
    FOR_EACH_EDGE(e, ei, bb->succs) {
        if (e->dest && e->dest->index != EXIT_BLOCK) {
            compute_rpo_dfs(e->dest, visited, postorder);
        }
    }
    postorder.push_back(bb);
}

static std::vector<basic_block> get_rpo_blocks(function* fun) {
    std::unordered_set<int> visited;
    std::vector<basic_block> order;
    compute_rpo_dfs(ENTRY_BLOCK_PTR_FOR_FN(fun), visited, order);
    std::reverse(order.begin(), order.end());
    return order;
}

static std::string resolve_display_name(
    tree ssa,
    const std::string& sym_name,
    const std::shared_ptr<safe_c::core::AllocationCell>& cell,
    const std::unordered_map<std::string, LocInfo>& sym_alloc_loc,
    const std::unordered_map<uint64_t, LocInfo>& cell_alloc_loc
) {
    const char* vname = ssa_var_name(ssa);
    if (vname && *vname) return vname;

    auto s_it = sym_alloc_loc.find(sym_name);
    if (s_it != sym_alloc_loc.end() && !s_it->second.name.empty()) {
        return s_it->second.name;
    }
    if (cell) {
        auto c_it = cell_alloc_loc.find(cell->cell_id);
        if (c_it != cell_alloc_loc.end() && !c_it->second.name.empty()) {
            return c_it->second.name;
        }
    }
    return {};
}

static void report_dealloc_error(
    location_t loc,
    safe_c::ViolationType err,
    const std::string& dname,
    const std::shared_ptr<safe_c::core::AllocationCell>& cell,
    int64_t offset,
    const std::unordered_map<uint64_t, LocInfo>& cell_alloc_loc,
    const std::unordered_map<uint64_t, LocInfo>& cell_free_loc
) {
    const auto& cfg = safe_c::plugin::g_config;
    const char* name_or_ptr = dname.empty() ? "pointer" : dname.c_str();

    switch (err) {
    case safe_c::ViolationType::DoubleFree:
        if (!cfg.check_double_free) return;
        SAFEC_DIAG(loc, "double free of %qs [-Wsafec-double-free]", name_or_ptr);
        if (cell) {
            auto prev = cell_free_loc.find(cell->cell_id);
            if (prev != cell_free_loc.end() && prev->second.loc != UNKNOWN_LOCATION) {
                inform(prev->second.loc, "previously deallocated here");
            }
        }
        break;
    case safe_c::ViolationType::ModifiedBaseFree:
        if (!cfg.check_arithmetic) return;
        SAFEC_DIAG(loc, "attempt to free interior pointer %qs at offset %ld bytes [-Wsafec-modified-base]",
                   name_or_ptr, (long)offset);
        if (cell) {
            auto orig = cell_alloc_loc.find(cell->cell_id);
            if (orig != cell_alloc_loc.end() && orig->second.loc != UNKNOWN_LOCATION) {
                inform(orig->second.loc, "original allocation base created here");
            }
        }
        break;
    default:
        break;
    }
}

static void report_access_error(
    location_t loc,
    safe_c::ViolationType err,
    const std::string& dname,
    const std::shared_ptr<safe_c::core::AllocationCell>& cell,
    int64_t total_off,
    const std::unordered_map<uint64_t, LocInfo>& cell_alloc_loc,
    const std::unordered_map<uint64_t, LocInfo>& cell_free_loc
) {
    const auto& cfg = safe_c::plugin::g_config;
    const char* name_or_ptr = dname.empty() ? "pointer" : dname.c_str();

    switch (err) {
    case safe_c::ViolationType::UseAfterFree:
        if (!cfg.check_uaf) return;
        SAFEC_DIAG(loc, "use-after-free on pointer %qs [-Wsafec-use-after-free]", name_or_ptr);
        if (cell) {
            auto f_it = cell_free_loc.find(cell->cell_id);
            if (f_it != cell_free_loc.end() && f_it->second.loc != UNKNOWN_LOCATION) {
                inform(f_it->second.loc, "pointer deallocated here");
            }
        }
        break;
    case safe_c::ViolationType::OutOfBoundsAccess:
        if (!cfg.check_bounds || !cell) return;
        SAFEC_DIAG(loc, "out-of-bounds memory access on %qs at offset %ld (allocated size %zu bytes) [-Wsafec-bounds]",
                   name_or_ptr, (long)total_off, cell->size_bytes);
        {
            auto a_it = cell_alloc_loc.find(cell->cell_id);
            if (a_it != cell_alloc_loc.end() && a_it->second.loc != UNKNOWN_LOCATION) {
                inform(a_it->second.loc, "allocated here with size %zu bytes", cell->size_bytes);
            }
        }
        break;
    case safe_c::ViolationType::NullPointerDeref:
        if (!cfg.check_null) return;
        SAFEC_DIAG(loc, "null pointer dereference of %qs [-Wsafec-null-deref]", name_or_ptr);
        break;
    default:
        break;
    }
}

struct IterEntry {
    std::string container_key;
    std::string container_name;
    location_t start_loc{UNKNOWN_LOCATION};
    location_t mutated_loc{UNKNOWN_LOCATION};
    bool invalidated{false};
};

using IterMap = std::unordered_map<std::string, IterEntry>;

static const char* decl_ctx_type_name(tree fndecl) {
    tree ctx = DECL_CONTEXT(fndecl);
    if (ctx && TYPE_P(ctx)) {
        tree id = TYPE_IDENTIFIER(ctx);
        if (id) return IDENTIFIER_POINTER(id);
    }
    return nullptr;
}

static bool has_substr(const char* hay, const char* needle) {
    return hay && needle && strstr(hay, needle) != nullptr;
}

static bool is_stl_container_call(tree fndecl, const char* asm_name) {
    const char* ctx = decl_ctx_type_name(fndecl);
    if (has_substr(ctx, "vector") || has_substr(ctx, "deque") ||
        has_substr(ctx, "basic_string") || has_substr(ctx, "unordered_") ||
        has_substr(ctx, "map") || has_substr(ctx, "set") || has_substr(ctx, "list")) {
        return true;
    }
    return has_substr(asm_name, "St6vector") || has_substr(asm_name, "St5deque") ||
           has_substr(asm_name, "basic_string") || has_substr(asm_name, "unordered_");
}

static bool is_iter_begin_method(const char* fn_name) {
    if (!fn_name) return false;
    return strcmp(fn_name, "begin") == 0 || strcmp(fn_name, "cbegin") == 0 ||
           strcmp(fn_name, "rbegin") == 0 || strcmp(fn_name, "crbegin") == 0 ||
           strcmp(fn_name, "find") == 0 || strcmp(fn_name, "lower_bound") == 0 ||
           strcmp(fn_name, "upper_bound") == 0 ||
           strcmp(fn_name, "data") == 0 || strcmp(fn_name, "c_str") == 0;
}

static bool is_container_mutator(const char* fn_name) {
    if (!fn_name) return false;
    return strcmp(fn_name, "push_back") == 0 || strcmp(fn_name, "emplace_back") == 0 ||
           strcmp(fn_name, "push_front") == 0 || strcmp(fn_name, "emplace_front") == 0 ||
           strcmp(fn_name, "insert") == 0 || strcmp(fn_name, "emplace") == 0 ||
           strcmp(fn_name, "erase") == 0 || strcmp(fn_name, "clear") == 0 ||
           strcmp(fn_name, "resize") == 0 || strcmp(fn_name, "reserve") == 0 ||
           strcmp(fn_name, "assign") == 0 || strcmp(fn_name, "pop_back") == 0 ||
           strcmp(fn_name, "pop_front") == 0 ||
           strcmp(fn_name, "shrink_to_fit") == 0 || strcmp(fn_name, "rehash") == 0;
}

static void report_iter_invalidation(
    IterMap& iters,
    std::unordered_set<std::string>& reported_iters,
    const std::string& ikey,
    location_t fallback_loc
) {
    if (ikey.empty()) return;
    auto it = iters.find(ikey);
    if (it != iters.end() && it->second.invalidated && reported_iters.insert(ikey).second) {
        location_t diag_loc = (it->second.mutated_loc != UNKNOWN_LOCATION) ? it->second.mutated_loc : fallback_loc;
        SAFEC_DIAG(diag_loc,
                   "container %qs mutated while iterating with invalidated iterator [-Wsafec-iterator-invalidation]",
                   it->second.container_name.empty() ? "container" : it->second.container_name.c_str());
        if (it->second.start_loc != UNKNOWN_LOCATION) {
            inform(it->second.start_loc, "iteration started here");
        }
    }
}

static bool is_destructor_call(const char* fn_name, const char* asm_name) {
    if (fn_name && fn_name[0] == '~') return true;
    return has_substr(asm_name, "D0Ev") || has_substr(asm_name, "D1Ev") || has_substr(asm_name, "D2Ev");
}

static std::string canonical_obj_key(tree t, std::string& out_name) {
    for (int depth = 0; t && depth < 8; ++depth) {
        if (TREE_CODE(t) == ADDR_EXPR) {
            t = TREE_OPERAND(t, 0);
            continue;
        }
        if (TREE_CODE(t) == SSA_NAME) {
            gimple *def = SSA_NAME_DEF_STMT(t);
            if (def && is_gimple_assign(def)) {
                enum tree_code rc = gimple_assign_rhs_code(def);
                if (rc == ADDR_EXPR || rc == SSA_NAME || rc == NOP_EXPR || rc == CONVERT_EXPR) {
                    t = gimple_assign_rhs1(def);
                    continue;
                }
            }
            const char* n = ssa_var_name(t);
            if (n && *n) out_name = n;
            return ssa_sym(t);
        }
        if (DECL_P(t)) {
            if (DECL_NAME(t)) out_name = IDENTIFIER_POINTER(DECL_NAME(t));
            return "decl_" + std::to_string(DECL_UID(t));
        }
        break;
    }
    return {};
}

const pass_data safe_c_pass_data = {
    GIMPLE_PASS,
    "safe_c_pass",
    OPTGROUP_NONE,
    TV_NONE,
    PROP_gimple_any | PROP_ssa,
    0,
    0,
    0,
    0
};

class pass_safe_c : public gimple_opt_pass {
public:
    explicit pass_safe_c(gcc::context *ctxt)
        : gimple_opt_pass(safe_c_pass_data, ctxt) {}

    bool gate(function *) override {
        return safe_c::plugin::g_config.enabled;
    }

    unsigned int execute(function *fun) override {
        return safe_c::plugin::execute_memcheck_pass(reinterpret_cast<void*>(fun));
    }

    opt_pass *clone() override {
        return this;
    }
};

} // anonymous namespace
#endif

namespace safe_c::plugin {

unsigned int execute_memcheck_pass(void* gcc_fun_ptr) {
    if (!g_config.enabled) {
        return 0;
    }

#ifdef HAVE_GCC_PLUGIN_HEADERS
    function *fun = reinterpret_cast<function*>(gcc_fun_ptr);
    if (!fun || !fun->cfg) return 0;

    std::unordered_map<int, safe_c::core::StateTable> bb_out_states;
    std::unordered_map<int, IterMap> bb_out_iters;
    std::unordered_map<uint64_t, LocInfo> cell_alloc_loc;
    std::unordered_map<uint64_t, LocInfo> cell_free_loc;
    std::unordered_map<std::string, LocInfo> sym_alloc_loc;
    std::unordered_map<std::string, std::string> realloc_src;
    std::unordered_set<uint64_t> reported_leaks;
    std::unordered_set<std::string> reported_iters;

    auto rpo_blocks = get_rpo_blocks(fun);

    for (basic_block bb : rpo_blocks) {
        if (!bb || bb->index == EXIT_BLOCK) continue;

        safe_c::core::StateTable state;
        IterMap iters;
        bool has_pred_state = false;
        edge e;
        edge_iterator ei;
        FOR_EACH_EDGE(e, ei, bb->preds) {
            if (!e->src) continue;
            auto it = bb_out_states.find(e->src->index);
            if (it == bb_out_states.end()) continue;

            safe_c::core::StateTable incoming = it->second;

            // Narrow pointer state along true/false edges of `if (p == 0B)` / `if (p != 0B)`.
            gimple_stmt_iterator last_gsi = gsi_last_nondebug_bb(e->src);
            gimple *last_stmt = !gsi_end_p(last_gsi) ? gsi_stmt(last_gsi) : nullptr;
            if (last_stmt && gimple_code(last_stmt) == GIMPLE_COND) {
                gcond *cond = as_a<gcond*>(last_stmt);
                enum tree_code ccode = gimple_cond_code(cond);
                tree lhs = gimple_cond_lhs(cond);
                tree rhs = gimple_cond_rhs(cond);
                if (lhs && TREE_CODE(lhs) == SSA_NAME && rhs && integer_zerop(rhs)) {
                    bool is_null_branch =
                        (ccode == EQ_EXPR && (e->flags & EDGE_TRUE_VALUE)) ||
                        (ccode == NE_EXPR && (e->flags & EDGE_FALSE_VALUE));
                    if (is_null_branch) {
                        std::string cond_sym = ssa_sym(lhs);
                        if (incoming.has_symbol(cond_sym)) {
                            incoming.mark_null(cond_sym);
                        }
                        auto r_it = realloc_src.find(cond_sym);
                        if (r_it != realloc_src.end()) {
                            incoming.restore_allocated(r_it->second);
                        }
                    }
                }
            }

            if (!has_pred_state) {
                state = std::move(incoming);
                auto iter_it = bb_out_iters.find(e->src->index);
                if (iter_it != bb_out_iters.end()) {
                    iters = iter_it->second;
                }
                has_pred_state = true;
            } else {
                state.merge_from(incoming);
                auto iter_it = bb_out_iters.find(e->src->index);
                if (iter_it != bb_out_iters.end()) {
                    for (const auto& [ik, iv] : iter_it->second) {
                        auto cur = iters.find(ik);
                        if (cur == iters.end()) {
                            iters.emplace(ik, iv);
                        } else if (iv.invalidated && !cur->second.invalidated) {
                            cur->second.invalidated = true;
                            cur->second.mutated_loc = iv.mutated_loc;
                        }
                    }
                }
            }
        }

        // Resolve SSA PHI nodes, preferring live heap bindings over freed or null paths.
        for (gphi_iterator gpi = gsi_start_phis(bb); !gsi_end_p(gpi); gsi_next(&gpi)) {
            gphi *phi = gpi.phi();
            tree res = gimple_phi_result(phi);
            if (!res || TREE_CODE(res) != SSA_NAME) continue;
            std::string res_sym = ssa_sym(res);

            unsigned n = gimple_phi_num_args(phi);
            std::string chosen_arg;
            int chosen_score = -1;
            for (unsigned idx = 0; idx < n; ++idx) {
                tree arg = gimple_phi_arg_def(phi, idx);
                if (!arg || TREE_CODE(arg) != SSA_NAME) continue;
                std::string arg_sym = ssa_sym(arg);
                const auto* b = state.get_binding(arg_sym);
                if (!b || !b->cell()) continue;
                int score = 0;
                if (b->cell()->state == safe_c::Typestate::Allocated) {
                    if (b->cell()->kind != safe_c::AllocKind::Null &&
                        b->cell()->kind != safe_c::AllocKind::Uninitialized) {
                        score = b->cell()->maybe_freed ? 2 : 3;
                    } else {
                        score = 1;
                    }
                }
                if (score > chosen_score) {
                    chosen_score = score;
                    chosen_arg = arg_sym;
                }
            }
            if (!chosen_arg.empty()) {
                state.register_alias(res_sym, chosen_arg, 0);
                auto a_it = sym_alloc_loc.find(chosen_arg);
                if (a_it != sym_alloc_loc.end()) {
                    const char* vname = ssa_var_name(res);
                    sym_alloc_loc[res_sym] = {
                        a_it->second.loc,
                        (vname && *vname) ? vname : a_it->second.name
                    };
                }
            }
        }

        for (gimple_stmt_iterator gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
            gimple *stmt = gsi_stmt(gsi);
            if (gimple_clobber_p(stmt)) {
                if (is_gimple_assign(stmt)) {
                    std::string tmp_name;
                    std::string clobber_key = canonical_obj_key(gimple_assign_lhs(stmt), tmp_name);
                    if (!clobber_key.empty()) {
                        iters.erase(clobber_key);
                    }
                }
                continue;
            }

            location_t loc = gimple_location(stmt);

            if (is_gimple_call(stmt)) {
                tree fndecl = gimple_call_fndecl(stmt);
                if (!fndecl) continue;
                const char *fn_name = DECL_NAME(fndecl) ? IDENTIFIER_POINTER(DECL_NAME(fndecl)) : nullptr;
                const char *asm_name = DECL_ASSEMBLER_NAME(fndecl) ? IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME(fndecl)) : nullptr;

                if (g_config.check_iterator && !is_destructor_call(fn_name, asm_name)) {
                    unsigned nargs = gimple_call_num_args(stmt);
                    for (unsigned ai = 0; ai < nargs; ++ai) {
                        std::string dummy;
                        report_iter_invalidation(iters, reported_iters,
                                                 canonical_obj_key(gimple_call_arg(stmt, ai), dummy), loc);
                    }

                    if (is_stl_container_call(fndecl, asm_name) && nargs >= 1) {
                        std::string cname;
                        std::string ckey = canonical_obj_key(gimple_call_arg(stmt, 0), cname);
                        if (!ckey.empty()) {
                            if (is_iter_begin_method(fn_name)) {
                                tree lhs = gimple_call_lhs(stmt);
                                if (lhs) {
                                    std::string iname;
                                    std::string ikey = canonical_obj_key(lhs, iname);
                                    if (!ikey.empty()) {
                                        iters[ikey] = {ckey, cname, loc, UNKNOWN_LOCATION, false};
                                    }
                                }
                            } else if (is_container_mutator(fn_name)) {
                                tree lhs = gimple_call_lhs(stmt);
                                std::string refreshed_key;
                                if (lhs && fn_name &&
                                    (strcmp(fn_name, "erase") == 0 || strcmp(fn_name, "insert") == 0 ||
                                     strcmp(fn_name, "emplace") == 0)) {
                                    std::string iname;
                                    refreshed_key = canonical_obj_key(lhs, iname);
                                }
                                for (auto& [ikey, entry] : iters) {
                                    if (entry.container_key == ckey) {
                                        if (ikey == refreshed_key) {
                                            entry.invalidated = false;
                                        } else {
                                            entry.invalidated = true;
                                            entry.mutated_loc = loc;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                safe_c::AllocKind dealloc_kind = safe_c::AllocKind::Unknown;
                safe_c::AllocKind alloc_kind = safe_c::AllocKind::Unknown;

                if (is_realloc_fn(fn_name, asm_name)) {
                    if (gimple_call_num_args(stmt) >= 2) {
                        tree old_arg = gimple_call_arg(stmt, 0);
                        if (old_arg && TREE_CODE(old_arg) == SSA_NAME) {
                            std::string old_sym = ssa_sym(old_arg);
                            if (state.has_symbol(old_sym)) {
                                std::shared_ptr<core::AllocationCell> cell;
                                int64_t offset = 0;
                                auto err = state.register_free(old_sym, safe_c::SourceLocation{}, cell, offset, safe_c::AllocKind::Malloc);
                                std::string dname = resolve_display_name(old_arg, old_sym, cell, sym_alloc_loc, cell_alloc_loc);
                                if (err.has_value()) {
                                    report_dealloc_error(loc, *err, dname, cell, offset, cell_alloc_loc, cell_free_loc);
                                } else if (cell) {
                                    cell_free_loc[cell->cell_id] = {loc, dname};
                                }
                            }
                        }
                        tree lhs = gimple_call_lhs(stmt);
                        if (lhs && TREE_CODE(lhs) == SSA_NAME) {
                            size_t sz = 64;
                            bool sz_known = cst_to_size(gimple_call_arg(stmt, 1), sz);
                            std::string lhs_sym = ssa_sym(lhs);
                            auto cell = state.register_allocation(lhs_sym, safe_c::SourceLocation{}, sz, false, 0, safe_c::AllocKind::Malloc, sz_known);
                            const char* vname = ssa_var_name(lhs);
                            sym_alloc_loc[lhs_sym] = {loc, vname ? vname : ""};
                            if (cell) cell_alloc_loc[cell->cell_id] = {loc, vname ? vname : ""};
                            if (old_arg && TREE_CODE(old_arg) == SSA_NAME) {
                                realloc_src[lhs_sym] = ssa_sym(old_arg);
                            }
                        }
                    }
                    continue;
                }

                if (is_dealloc_fn(fn_name, asm_name, dealloc_kind)) {
                    if (gimple_call_num_args(stmt) > 0) {
                        tree arg = gimple_call_arg(stmt, 0);
                        if (arg && integer_zerop(arg)) {
                            continue;
                        }
                        if (arg && TREE_CODE(arg) == SSA_NAME) {
                            std::string sym_name = ssa_sym(arg);
                            if (!state.has_symbol(sym_name)) {
                                continue;
                            }

                            std::shared_ptr<core::AllocationCell> cell;
                            int64_t offset = 0;
                            auto err = state.register_free(sym_name, safe_c::SourceLocation{}, cell, offset, dealloc_kind);
                            std::string display_name = resolve_display_name(arg, sym_name, cell, sym_alloc_loc, cell_alloc_loc);

                            if (err.has_value()) {
                                report_dealloc_error(loc, *err, display_name, cell, offset, cell_alloc_loc, cell_free_loc);
                            } else if (cell) {
                                cell_free_loc[cell->cell_id] = {loc, display_name};
                            }
                        }
                    }
                    continue;
                }

                if (is_alloc_fn(fn_name, asm_name, alloc_kind)) {
                    tree lhs = gimple_call_lhs(stmt);
                    if (lhs && TREE_CODE(lhs) == SSA_NAME) {
                        size_t sz = 64;
                        bool sz_known = false;
                        unsigned nargs = gimple_call_num_args(stmt);
                        if (fn_name && strcmp(fn_name, "calloc") == 0 && nargs >= 2) {
                            size_t c0 = 0, c1 = 0;
                            if (cst_to_size(gimple_call_arg(stmt, 0), c0) &&
                                cst_to_size(gimple_call_arg(stmt, 1), c1)) {
                                sz = c0 * c1;
                                sz_known = true;
                            }
                        } else if (fn_name && strcmp(fn_name, "aligned_alloc") == 0 && nargs >= 2) {
                            sz_known = cst_to_size(gimple_call_arg(stmt, 1), sz);
                        } else if (nargs >= 1) {
                            sz_known = cst_to_size(gimple_call_arg(stmt, 0), sz);
                        }

                        std::string sym_name = ssa_sym(lhs);
                        bool is_arr = (alloc_kind == safe_c::AllocKind::NewArray);
                        auto cell = state.register_allocation(sym_name, safe_c::SourceLocation{}, sz, is_arr, 0, alloc_kind, sz_known);
                        const char* vname = ssa_var_name(lhs);
                        sym_alloc_loc[sym_name] = {loc, vname ? vname : ""};
                        if (cell) {
                            cell_alloc_loc[cell->cell_id] = {loc, vname ? vname : ""};
                        }
                    }
                    continue;
                }

                int ptr_arg_idx = -1;
                int sz_arg_idx = -1;
                if (is_mem_write_fn(fn_name, ptr_arg_idx, sz_arg_idx) &&
                    gimple_call_num_args(stmt) > static_cast<unsigned>(sz_arg_idx)) {
                    tree dst = gimple_call_arg(stmt, ptr_arg_idx);
                    tree len = gimple_call_arg(stmt, sz_arg_idx);
                    if (dst && TREE_CODE(dst) == SSA_NAME) {
                        std::string dst_sym = ssa_sym(dst);
                        size_t write_sz = 0;
                        cst_to_size(len, write_sz);
                        std::shared_ptr<core::AllocationCell> cell;
                        auto err = state.validate_access(dst_sym, cell, 0, write_sz);
                        if (err.has_value()) {
                            std::string dname = resolve_display_name(dst, dst_sym, cell, sym_alloc_loc, cell_alloc_loc);
                            const auto* b = state.get_binding(dst_sym);
                            int64_t total_off = b ? b->offset_bytes() : 0;
                            report_access_error(loc, *err, dname, cell, total_off, cell_alloc_loc, cell_free_loc);
                        }
                    }
                }

                // Any pointer passed into an opaque function may outlive the current frame.
                if (!is_non_escaping_fn(fn_name)) {
                    unsigned nargs = gimple_call_num_args(stmt);
                    for (unsigned i = 0; i < nargs; ++i) {
                        tree arg = gimple_call_arg(stmt, i);
                        if (arg && TREE_CODE(arg) == SSA_NAME) {
                            std::string arg_sym = ssa_sym(arg);
                            if (state.has_symbol(arg_sym)) {
                                state.mark_escaped(arg_sym);
                            }
                        }
                    }
                }
            }

            if (is_gimple_assign(stmt)) {
                enum tree_code code = gimple_assign_rhs_code(stmt);
                tree lhs = gimple_assign_lhs(stmt);
                tree rhs1 = gimple_assign_rhs1(stmt);

                if (code == POINTER_PLUS_EXPR) {
                    tree rhs2 = gimple_assign_rhs2(stmt);
                    if (lhs && rhs1 && TREE_CODE(lhs) == SSA_NAME && TREE_CODE(rhs1) == SSA_NAME) {
                        std::string base_sym = ssa_sym(rhs1);
                        std::string lhs_sym = ssa_sym(lhs);
                        int64_t off_val = 0;
                        cst_to_shwi(rhs2, off_val);
                        state.register_alias(lhs_sym, base_sym, off_val);
                        auto b_it = sym_alloc_loc.find(base_sym);
                        if (b_it != sym_alloc_loc.end()) {
                            sym_alloc_loc[lhs_sym] = b_it->second;
                        }
                        auto iter_it = iters.find(base_sym);
                        if (iter_it != iters.end()) {
                            iters[lhs_sym] = iter_it->second;
                        }
                    }
                } else if ((code == SSA_NAME || code == NOP_EXPR || code == CONVERT_EXPR) &&
                           lhs && rhs1 && TREE_CODE(lhs) == SSA_NAME && TREE_CODE(rhs1) == SSA_NAME) {
                    std::string rhs_sym = ssa_sym(rhs1);
                    std::string lhs_sym = ssa_sym(lhs);
                    state.register_alias(lhs_sym, rhs_sym, 0);
                    auto iter_it = iters.find(rhs_sym);
                    if (iter_it != iters.end()) {
                        iters[lhs_sym] = iter_it->second;
                    }

                    const char* vname = ssa_var_name(lhs);
                    auto r_it = sym_alloc_loc.find(rhs_sym);
                    if (r_it != sym_alloc_loc.end()) {
                        sym_alloc_loc[lhs_sym] = {
                            r_it->second.loc,
                            (vname && *vname) ? vname : r_it->second.name
                        };
                    } else if (vname && *vname) {
                        sym_alloc_loc[lhs_sym] = {loc, vname};
                    }
                } else if (code == INTEGER_CST && lhs && TREE_CODE(lhs) == SSA_NAME &&
                           POINTER_TYPE_P(TREE_TYPE(lhs)) && rhs1 && integer_zerop(rhs1)) {
                    std::string lhs_sym = ssa_sym(lhs);
                    auto cell = state.register_special(lhs_sym, safe_c::SourceLocation{}, safe_c::AllocKind::Null);
                    const char* vname = ssa_var_name(lhs);
                    sym_alloc_loc[lhs_sym] = {loc, vname ? vname : ""};
                    if (cell) cell_alloc_loc[cell->cell_id] = {loc, vname ? vname : ""};
                } else if (code == ADDR_EXPR && lhs && TREE_CODE(lhs) == SSA_NAME && rhs1) {
                    tree target = TREE_OPERAND(rhs1, 0);
                    if (target && TREE_CODE(target) == VAR_DECL && !is_global_var(target)) {
                        std::string lhs_sym = ssa_sym(lhs);
                        auto cell = state.register_special(lhs_sym, safe_c::SourceLocation{}, safe_c::AllocKind::Stack);
                        const char* vname = DECL_NAME(target) ? IDENTIFIER_POINTER(DECL_NAME(target)) : "";
                        location_t decl_loc = DECL_SOURCE_LOCATION(target);
                        sym_alloc_loc[lhs_sym] = {decl_loc, vname};
                        if (cell) cell_alloc_loc[cell->cell_id] = {decl_loc, vname};
                    }
                } else if (rhs1 && TREE_CODE(rhs1) == SSA_NAME && lhs && TREE_CODE(lhs) != SSA_NAME) {
                    std::string rhs_sym = ssa_sym(rhs1);
                    if (state.has_symbol(rhs_sym)) {
                        state.mark_escaped(rhs_sym);
                    }
                }
            }

            if (gimple_code(stmt) == GIMPLE_RETURN) {
                greturn *ret = as_a<greturn*>(stmt);
                tree val = gimple_return_retval(ret);
                if (val && TREE_CODE(val) == SSA_NAME) {
                    state.mark_escaped(ssa_sym(val));
                }
            }

            tree deref_ptr = NULL_TREE;
            int64_t mem_off = 0;
            size_t access_sz = 0;
            if (is_gimple_assign(stmt)) {
                tree lhs = gimple_assign_lhs(stmt);
                tree rhs = gimple_assign_rhs1(stmt);
                if (!extract_mem_ref(lhs, deref_ptr, mem_off, access_sz)) {
                    extract_mem_ref(rhs, deref_ptr, mem_off, access_sz);
                }
            }

            if (deref_ptr && TREE_CODE(deref_ptr) == SSA_NAME) {
                std::string sym_name = ssa_sym(deref_ptr);
                if (g_config.check_iterator) {
                    std::string dummy;
                    report_iter_invalidation(iters, reported_iters, canonical_obj_key(deref_ptr, dummy), loc);
                }
                std::shared_ptr<core::AllocationCell> cell;
                auto err = state.validate_access(sym_name, cell, mem_off, access_sz);
                if (err.has_value()) {
                    std::string dname = resolve_display_name(deref_ptr, sym_name, cell, sym_alloc_loc, cell_alloc_loc);
                    const auto* b = state.get_binding(sym_name);
                    int64_t total_off = (b ? b->offset_bytes() : 0) + mem_off;
                    report_access_error(loc, *err, dname, cell, total_off, cell_alloc_loc, cell_free_loc);
                }
            }
        }

        bb_out_states[bb->index] = std::move(state);
        bb_out_iters[bb->index] = std::move(iters);
    }

    if (g_config.check_leaks) {
        basic_block exit_bb = EXIT_BLOCK_PTR_FOR_FN(fun);
        edge e;
        edge_iterator ei;
        FOR_EACH_EDGE(e, ei, exit_bb->preds) {
            if (!e->src) continue;
            auto it = bb_out_states.find(e->src->index);
            if (it == bb_out_states.end()) continue;

            auto leaks = it->second.find_leaks();
            for (const auto& [symbol, cell] : leaks) {
                if (!cell || !reported_leaks.insert(cell->cell_id).second) continue;

                location_t alloc_l = UNKNOWN_LOCATION;
                std::string display_name;
                auto c_it = cell_alloc_loc.find(cell->cell_id);
                if (c_it != cell_alloc_loc.end()) {
                    alloc_l = c_it->second.loc;
                    display_name = c_it->second.name;
                }
                if (display_name.empty()) {
                    auto a_it = sym_alloc_loc.find(symbol);
                    if (a_it != sym_alloc_loc.end()) {
                        if (alloc_l == UNKNOWN_LOCATION) alloc_l = a_it->second.loc;
                        display_name = a_it->second.name;
                    }
                }

                const char* fn_str = IDENTIFIER_POINTER(DECL_NAME(fun->decl));
                if (!display_name.empty()) {
                    SAFEC_DIAG(DECL_SOURCE_LOCATION(fun->decl),
                               "leak of %qs on exit from %qs [-Wsafec-leak]",
                               display_name.c_str(), fn_str);
                } else {
                    SAFEC_DIAG(DECL_SOURCE_LOCATION(fun->decl),
                               "leak of heap allocation on exit from %qs [-Wsafec-leak]",
                               fn_str);
                }
                if (alloc_l != UNKNOWN_LOCATION) {
                    inform(alloc_l, "allocated here");
                }
            }
        }
    }
#else
    (void)gcc_fun_ptr;
#endif

    return 0;
}

void register_safe_c_pass(const char* plugin_name) {
#ifdef HAVE_GCC_PLUGIN_HEADERS
    struct register_pass_info pass_info;
    pass_info.pass = new pass_safe_c(g);
    pass_info.reference_pass_name = "ssa";
    pass_info.ref_pass_instance_number = 1;
    pass_info.pos_op = PASS_POS_INSERT_AFTER;

    register_callback(plugin_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_info);
#else
    (void)plugin_name;
#endif
}

} // namespace safe_c::plugin
