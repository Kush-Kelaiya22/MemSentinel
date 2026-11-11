#include "analyzer.h"
#include "safe_c/common.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Safe-C static memory safety analyzer v" << SAFE_C_VERSION_STRING << "\n";
        std::cerr << "Usage: safe_c [options] <source-files...>\n";
        return 0;
    }

    const char* disable_env = std::getenv("SAFE_C_DISABLE");
    if (!disable_env) disable_env = std::getenv("SAFE_CPP_DISABLE");
    if (disable_env && (std::strcmp(disable_env, "1") == 0 || std::strcmp(disable_env, "true") == 0)) {
        return 0;
    }

    safe_c::core::ColorChoice color_choice = safe_c::core::ColorChoice::Auto;
    bool show_highlights = true;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-D" && i + 1 < argc) {
            std::string def = argv[++i];
            if (def == "SAFE_C_DISABLE" || def == "SAFE_CPP_DISABLE") {
                return 0;
            }
            continue;
        }
        if (arg == "-DSAFE_C_DISABLE" || arg == "-DSAFE_CPP_DISABLE" ||
            arg == "-fno-safe-c" || arg == "--no-safe-c" ||
            arg == "-fplugin-arg-safe_c-disable") {
            return 0;
        }
        if (arg.rfind("-D", 0) == 0 || arg.rfind("-U", 0) == 0 || arg.rfind("-A", 0) == 0) {
            continue;
        }
        if (arg == "-fdiagnostics-color=always" || arg == "-fdiagnostics-color") {
            color_choice = safe_c::core::ColorChoice::Always;
        } else if (arg == "-fdiagnostics-color=never" || arg == "-fno-diagnostics-color") {
            color_choice = safe_c::core::ColorChoice::Never;
        } else if (arg == "-fdiagnostics-color=auto") {
            color_choice = safe_c::core::ColorChoice::Auto;
        } else if (arg == "-fno-diagnostics-show-highlight-colors") {
            show_highlights = false;
        } else if (arg == "-fdiagnostics-show-highlight-colors") {
            show_highlights = true;
        } else if (!arg.empty() && arg[0] != '-') {
            files.push_back(arg);
        }
    }

    if (files.empty()) {
        return 0;
    }

    safe_c::core::Analyzer analyzer(safe_c::Mode::FatalError);
    analyzer.set_color_mode(color_choice);
    analyzer.set_highlight_colors(show_highlights);

    bool has_violations = false;
    for (const auto& file : files) {
        if (!analyzer.analyze_file(file) || analyzer.has_errors()) {
            has_violations = true;
        }
    }

    if (has_violations) {
        analyzer.print_diagnostics(std::cerr);
        return 1;
    }

    return 0;
}
