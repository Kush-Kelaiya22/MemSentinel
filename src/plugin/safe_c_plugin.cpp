#include "safe_c_plugin.h"
#include "gimple_pass.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

#ifdef HAVE_GCC_PLUGIN_HEADERS
  #include <diagnostic-core.h>
#endif

// Required symbol for GCC's plugin loader (AGPLv3 is GPLv3-compatible).
int plugin_is_GPL_compatible = 1;

namespace safe_c::plugin {

PluginConfig g_config;

static bool parse_bool(const char* val, bool fallback) {
    if (!val) return fallback;
    if (std::strcmp(val, "on") == 0 || std::strcmp(val, "1") == 0 ||
        std::strcmp(val, "true") == 0 || std::strcmp(val, "yes") == 0) {
        return true;
    }
    if (std::strcmp(val, "off") == 0 || std::strcmp(val, "0") == 0 ||
        std::strcmp(val, "false") == 0 || std::strcmp(val, "no") == 0) {
        return false;
    }
    return fallback;
}

static bool env_truthy(const char* name) {
    const char* val = std::getenv(name);
    if (!val || !*val) return false;
    return parse_bool(val, true);
}

void parse_plugin_arguments(int argc, struct plugin_argument *argv) {
    if (env_truthy("SAFE_C_DISABLE") || env_truthy("SAFE_CPP_DISABLE")) {
        g_config.enabled = false;
    }
    if (env_truthy("SAFE_C_ENABLE")) {
        g_config.enabled = true;
        g_config.force_enabled = true;
    }
    if (const char* mode_env = std::getenv("SAFE_C_MODE")) {
        if (std::strcmp(mode_env, "warn") == 0) {
            g_config.mode = Mode::WarningOnly;
        } else if (std::strcmp(mode_env, "fatal") == 0) {
            g_config.mode = Mode::FatalError;
        } else if (std::strcmp(mode_env, "off") == 0) {
            g_config.enabled = false;
        }
    }

    struct ToggleEntry {
        const char* name;
        const char* no_name;
        bool PluginConfig::*field;
    };
    static const ToggleEntry kToggles[] = {
        { "leaks",       "no-leaks",       &PluginConfig::check_leaks },
        { "double-free", "no-double-free", &PluginConfig::check_double_free },
        { "uaf",         "no-uaf",         &PluginConfig::check_uaf },
        { "bounds",      "no-bounds",      &PluginConfig::check_bounds },
        { "null",        "no-null",        &PluginConfig::check_null },
        { "uninit",      "no-uninit",      &PluginConfig::check_uninit },
        { "dangling",    "no-dangling",    &PluginConfig::check_dangling },
        { "mismatch",    "no-mismatch",    &PluginConfig::check_mismatch },
        { "arithmetic",  "no-arithmetic",  &PluginConfig::check_arithmetic },
        { "iterator",    "no-iterator",    &PluginConfig::check_iterator },
    };

    for (int i = 0; i < argc; ++i) {
        if (!argv[i].key) continue;
        const char* key = argv[i].key;
        const char* val = argv[i].value;

        if (std::strcmp(key, "disable") == 0 || std::strcmp(key, "off") == 0) {
            g_config.enabled = false;
            g_config.force_enabled = false;
            continue;
        }
        if (std::strcmp(key, "enable") == 0) {
            g_config.enabled = true;
            g_config.force_enabled = true;
            continue;
        }
        if (std::strcmp(key, "mode") == 0 && val) {
            if (std::strcmp(val, "warn") == 0) {
                g_config.mode = Mode::WarningOnly;
                g_config.enabled = true;
            } else if (std::strcmp(val, "fatal") == 0) {
                g_config.mode = Mode::FatalError;
                g_config.enabled = true;
            } else if (std::strcmp(val, "off") == 0) {
                g_config.enabled = false;
            }
            continue;
        }
        if (std::strcmp(key, "log") == 0 && val) {
            g_config.log_file = val;
            continue;
        }

        bool matched = false;
        for (const auto& t : kToggles) {
            if (std::strcmp(key, t.name) == 0) {
                g_config.*(t.field) = parse_bool(val, true);
                matched = true;
                break;
            }
            if (std::strcmp(key, t.no_name) == 0) {
                g_config.*(t.field) = false;
                matched = true;
                break;
            }
        }

        if (!matched) {
#ifdef HAVE_GCC_PLUGIN_HEADERS
            warning(0, "unrecognized safe_c plugin option %qs", key);
#endif
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
    if (!plugin_default_version_check(version, &gcc_version)) {
        std::cerr << "safe_c: fatal error: GCC version mismatch (minimum required: "
                  << SAFE_C_MIN_GCC_MAJOR << "." << SAFE_C_MIN_GCC_MINOR << ")\n";
        return 1;
    }
#else
    (void)version;
#endif

    safe_c::plugin::parse_plugin_arguments(plugin_info->argc, plugin_info->argv);

    if (!safe_c::plugin::g_config.enabled) {
        return 0;
    }

    safe_c::plugin::register_safe_c_pass(plugin_info->base_name);
    return 0;
}

} // extern "C"
