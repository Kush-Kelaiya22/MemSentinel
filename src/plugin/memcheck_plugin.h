#ifndef MEMCHECK_PLUGIN_H
#define MEMCHECK_PLUGIN_H

// C++ standard library headers MUST precede GCC system.h / gcc-plugin.h
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <cstdint>
#include <cstring>

#include "memcheck/common.h"

#ifdef HAVE_GCC_PLUGIN_HEADERS
  #include <gcc-plugin.h>
  #include <plugin-version.h>
  #include <tree.h>
  #include <gimple.h>
  #include <gimple-iterator.h>
  #include <tree-pass.h>
  #include <context.h>
#else
  // Declarations matching GCC Plugin ABI (GCC >= 4.5.0)
  struct plugin_name_args {
      char *base_name;
      char *full_name;
      int argc;
      struct plugin_argument *argv;
      char *version;
      char *help;
  };

  struct plugin_argument {
      char *key;
      char *value;
  };

  struct plugin_gcc_version {
      const char *basever;
      const char *datestamp;
      const char *devphase;
      const char *revision;
      const char *configuration_arguments;
  };

  enum plugin_event {
      PLUGIN_PASS_MANAGER_SETUP,
      PLUGIN_FINISH_UNIT,
      PLUGIN_ALL_PASSES_START,
      PLUGIN_ALL_PASSES_END,
      PLUGIN_EVENT_FIRST_DYNAMIC
  };

  typedef void (*plugin_callback_func)(void *gcc_data, void *user_data);

  extern "C" {
      int plugin_default_version_check(struct plugin_gcc_version *gcc_version,
                                      struct plugin_gcc_version *plugin_version);
      void register_callback(const char *name, int event,
                             plugin_callback_func callback, void *user_data);
  }
#endif

// Mandatory GPL compatibility symbol for GCC plugins (AGPLv3 is GPLv3-compatible)
extern "C" {
    extern int plugin_is_GPL_compatible;
}

namespace memcheck::plugin {

struct PluginConfig {
    bool enabled{true};
    Mode mode{Mode::FatalError};
    std::string log_file{"memcheck_runtime.log"};
};

extern PluginConfig g_config;

void parse_plugin_arguments(int argc, struct plugin_argument *argv);

} // namespace memcheck::plugin

#endif // MEMCHECK_PLUGIN_H
