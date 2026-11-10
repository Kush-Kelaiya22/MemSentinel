#include "safe_c_plugin.h"
#include "gimple_pass.h"
#include "analyzer.h"
#include <cstring>
#include <iostream>

// Mandatory GPL compatibility symbol required by GCC plugin loader
int plugin_is_GPL_compatible = 1;

#ifdef HAVE_GCC_PLUGIN_HEADERS
extern "C" const char *main_input_filename;

static void on_plugin_all_passes_start(void* gcc_data, void* user_data) {
    (void)gcc_data;
    (void)user_data;
    if (!safe_c::plugin::g_config.enabled) return;

    if (main_input_filename && *main_input_filename) {
        safe_c::core::Analyzer analyzer(safe_c::plugin::g_config.mode);
        if (!analyzer.analyze_file(main_input_filename) || analyzer.has_errors()) {
            analyzer.print_diagnostics(std::cerr);
            if (safe_c::plugin::g_config.mode == safe_c::Mode::FatalError) {
                exit(1);
            }
        }
    }
}
#endif

namespace safe_c::plugin {

PluginConfig g_config;

void parse_plugin_arguments(int argc, struct plugin_argument *argv) {
    for (int i = 0; i < argc; ++i) {
        if (!argv[i].key) continue;

        if (std::strcmp(argv[i].key, "disable") == 0) {
            g_config.enabled = false;
        } else if (std::strcmp(argv[i].key, "mode") == 0 && argv[i].value) {
            if (std::strcmp(argv[i].value, "warn") == 0) {
                g_config.mode = Mode::WarningOnly;
            } else if (std::strcmp(argv[i].value, "fatal") == 0) {
                g_config.mode = Mode::FatalError;
            } else if (std::strcmp(argv[i].value, "off") == 0) {
                g_config.enabled = false;
            }
        } else if (std::strcmp(argv[i].key, "log") == 0 && argv[i].value) {
            g_config.log_file = argv[i].value;
        }
    }
}

} // namespace safe_c::plugin

extern "C" {

SAFE_C_API int plugin_init(struct plugin_name_args *plugin_info,
                           struct plugin_gcc_version *version) {
    if (!plugin_info) {
        return 1;
    }

#ifdef HAVE_GCC_PLUGIN_HEADERS
    // Validate GCC version: Must be >= GCC 4.5.0
    if (!plugin_default_version_check(version, &gcc_version)) {
        std::cerr << "safe-c: fatal error: GCC version mismatch. Minimum required version is GCC "
                  << SAFE_C_MIN_GCC_MAJOR << "." << SAFE_C_MIN_GCC_MINOR << ".\n";
        return 1;
    }
#else
    (void)version;
#endif

    // Parse options (e.g. -fplugin-arg-safe_c-disable)
    safe_c::plugin::parse_plugin_arguments(plugin_info->argc, plugin_info->argv);

    if (!safe_c::plugin::g_config.enabled) {
        // Plugin explicitly disabled by command line flag
        return 0;
    }

    // Register our GIMPLE pass with GCC
    safe_c::plugin::register_safe_c_pass(plugin_info->base_name);

#ifdef HAVE_GCC_PLUGIN_HEADERS
    register_callback(plugin_info->base_name, PLUGIN_ALL_PASSES_START, on_plugin_all_passes_start, NULL);
#endif

    return 0;
}

} // extern "C"
