#ifndef MEMCHECK_PLUGIN_GIMPLE_PASS_H
#define MEMCHECK_PLUGIN_GIMPLE_PASS_H

#include "memcheck_plugin.h"
#include "pointer_state.h"

namespace memcheck::plugin {

// Core analysis function invoked for each function representation in GCC
unsigned int execute_memcheck_pass(void* gcc_fun_ptr);

// Registers the MemCheck GIMPLE SSA pass into the GCC Pass Manager
void register_memcheck_pass(const char* plugin_name);

} // namespace memcheck::plugin

#endif // MEMCHECK_PLUGIN_GIMPLE_PASS_H
