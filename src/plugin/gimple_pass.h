#ifndef SAFE_C_PLUGIN_GIMPLE_PASS_H
#define SAFE_C_PLUGIN_GIMPLE_PASS_H

#include "safe_c_plugin.h"
#include "pointer_state.h"

namespace safe_c::plugin {

// Core analysis function invoked for each function representation in GCC
unsigned int execute_memcheck_pass(void* gcc_fun_ptr);

// Registers the Safe-C GIMPLE SSA pass into the GCC Pass Manager
void register_safe_c_pass(const char* plugin_name);
inline void register_memcheck_pass(const char* plugin_name) { register_safe_c_pass(plugin_name); }

} // namespace safe_c::plugin

namespace memcheck::plugin {
    using namespace safe_c::plugin;
}

namespace safe_cpp::plugin {
    using namespace safe_c::plugin;
}

#endif // SAFE_C_PLUGIN_GIMPLE_PASS_H
