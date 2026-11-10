#include "gimple_pass.h"
#include "pointer_state.h"
#include <iostream>

#ifdef HAVE_GCC_PLUGIN_HEADERS
  #include <gcc-plugin.h>
  #include <tree.h>
  #include <gimple.h>
  #include <gimple-iterator.h>
  #include <tree-pass.h>
  #include <diagnostic.h>

namespace {

const pass_data safe_c_pass_data = {
    GIMPLE_PASS,        /* type */
    "safe_c_pass",      /* name */
    OPTGROUP_NONE,      /* optgroup_flags */
    TV_NONE,            /* tv_id */
    PROP_gimple_any | PROP_ssa, /* properties_required */
    0,                  /* properties_provided */
    0,                  /* properties_destroyed */
    0,                  /* todo_flags_start */
    0                   /* todo_flags_finish */
};

class pass_safe_c : public gimple_opt_pass {
public:
    pass_safe_c(gcc::context *ctxt)
        : gimple_opt_pass(safe_c_pass_data, ctxt) {}

    virtual bool gate(function *) override {
        return safe_c::plugin::g_config.enabled;
    }

    virtual unsigned int execute(function *fun) override {
        return safe_c::plugin::execute_memcheck_pass(reinterpret_cast<void*>(fun));
    }

    virtual opt_pass *clone() override {
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
    if (!fun) return 0;

    safe_c::core::StateTable state;
    basic_block bb;

    FOR_EACH_BB_FN(bb, fun) {
        for (gimple_stmt_iterator gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
            gimple *stmt = gsi_stmt(gsi);
            location_t loc = gimple_location(stmt);

            // 1. Analyze function call statements (malloc, free, etc.)
            if (is_gimple_call(stmt)) {
                tree fndecl = gimple_call_fndecl(stmt);
                if (!fndecl) continue;
                const char *fn_name = IDENTIFIER_POINTER(DECL_NAME(fndecl));
                if (!fn_name) continue;

                // Deallocation check: free(p), delete, etc.
                if (strcmp(fn_name, "free") == 0 || strcmp(fn_name, "_ZdlPv") == 0 || strcmp(fn_name, "_ZdaPv") == 0) {
                    if (gimple_call_num_args(stmt) > 0) {
                        tree arg = gimple_call_arg(stmt, 0);
                        if (TREE_CODE(arg) == SSA_NAME) {
                            std::string sym_name = "ptr_" + std::to_string(SSA_NAME_VERSION(arg));
                            std::shared_ptr<core::AllocationCell> cell;
                            int64_t offset = 0;
                            safe_c::SourceLocation sloc{"source", 0, 0};

                            auto err = state.register_free(sym_name, sloc, cell, offset);
                            if (err.has_value()) {
                                if (*err == ViolationType::DoubleFree) {
                                    error_at(loc, "[Safe-C] Double free detected on pointer");
                                } else if (*err == ViolationType::ModifiedBaseFree) {
                                    error_at(loc, "[Safe-C] Attempt to free interior/altered pointer (offset: %ld bytes)", (long)offset);
                                }
                            }
                        }
                    }
                }
                // Allocation check: malloc, calloc, operator new
                else if (strcmp(fn_name, "malloc") == 0 || strcmp(fn_name, "calloc") == 0 ||
                         strcmp(fn_name, "_Znwm") == 0 || strcmp(fn_name, "_Znam") == 0) {
                    tree lhs = gimple_call_lhs(stmt);
                    if (lhs && TREE_CODE(lhs) == SSA_NAME) {
                        std::string sym_name = "ptr_" + std::to_string(SSA_NAME_VERSION(lhs));
                        safe_c::SourceLocation sloc{"source", 0, 0};
                        state.register_allocation(sym_name, sloc, 64, false);
                    }
                }
            }

            // 2. Analyze Pointer Arithmetic: POINTER_PLUS_EXPR
            if (is_gimple_assign(stmt)) {
                enum tree_code code = gimple_assign_rhs_code(stmt);
                if (code == POINTER_PLUS_EXPR) {
                    tree lhs = gimple_assign_lhs(stmt);
                    tree base = gimple_assign_rhs1(stmt);
                    tree offset = gimple_assign_rhs2(stmt);

                    if (lhs && base && TREE_CODE(lhs) == SSA_NAME && TREE_CODE(base) == SSA_NAME) {
                        std::string base_sym = "ptr_" + std::to_string(SSA_NAME_VERSION(base));
                        std::string lhs_sym = "ptr_" + std::to_string(SSA_NAME_VERSION(lhs));
                        int64_t off_val = 4;
                        if (TREE_CODE(offset) == INTEGER_CST) {
                            off_val = tree_to_shwi(offset);
                        }
                        if (!state.can_alter_offset(base_sym, off_val)) {
                            error_at(loc, "[Safe-C] Cannot alter pointer offset: no variable retains the original allocation base address");
                        }
                        state.register_alias(lhs_sym, base_sym, off_val);
                    }
                }
            }

            // 3. Analyze Use-After-Free: Memory dereferences
            if (gimple_references_memory_p(stmt)) {
                tree vuse = gimple_vuse(stmt);
                if (vuse && TREE_CODE(vuse) == SSA_NAME) {
                    std::string sym_name = "ptr_" + std::to_string(SSA_NAME_VERSION(vuse));
                    std::shared_ptr<core::AllocationCell> cell;
                    auto err = state.validate_access(sym_name, cell);
                    if (err.has_value() && *err == ViolationType::UseAfterFree) {
                        error_at(loc, "[Safe-C] Use-After-Free detected: dereferencing freed pointer");
                    }
                }
            }
        }
    }

    // Check for memory leaks before function exit
    auto leaks = state.find_leaks();
    for (const auto& [symbol, cell] : leaks) {
        (void)symbol;
        (void)cell;
        warning_at(DECL_SOURCE_LOCATION(fun->decl), 0,
                   "[Safe-C] memory leak detected: dynamically allocated pointer was not freed before exiting %qs",
                   IDENTIFIER_POINTER(DECL_NAME(fun->decl)));
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
