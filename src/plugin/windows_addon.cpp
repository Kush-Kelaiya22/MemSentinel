#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include "analyzer.h"
#include "safe_c/common.h"

namespace {

bool ends_with(const std::wstring& str, const std::wstring& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

int check_compiler_invocations() {
    wchar_t exe_name[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exe_name, MAX_PATH);

    std::wstring exe_str(exe_name);
    // Only intercept when executed by cc1.exe (GCC) or cc1plus.exe (G++)
    if (exe_str.find(L"cc1.exe") == std::wstring::npos &&
        exe_str.find(L"cc1plus.exe") == std::wstring::npos) {
        return 0;
    }

    wchar_t env_val[32] = {0};
    if ((GetEnvironmentVariableW(L"SAFE_C_DISABLE", env_val, 32) > 0 ||
         GetEnvironmentVariableW(L"SAFE_CPP_DISABLE", env_val, 32) > 0) &&
        (wcsstr(env_val, L"1") != NULL || _wcsnicmp(env_val, L"true", 4) == 0)) {
        return 0;
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2) {
        if (argv) LocalFree(argv);
        return 0;
    }

    // Check for disable flags
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-fno-safe-c" || arg == L"-fno-safe-cpp" ||
            arg == L"-fplugin-arg-safe_c-disable" || arg == L"-fplugin-arg-safe_cpp-disable" ||
            arg == L"--no-safe-c" || arg == L"--no-safe-cpp" ||
            wcsstr(arg.c_str(), L"SAFE_C_DISABLE") != NULL ||
            wcsstr(arg.c_str(), L"SAFE_CPP_DISABLE") != NULL ||
            (arg == L"-D" && i + 1 < argc &&
             (wcscmp(argv[i+1], L"SAFE_C_DISABLE") == 0 || wcscmp(argv[i+1], L"SAFE_CPP_DISABLE") == 0))) {
            LocalFree(argv);
            return 0;
        }
    }

    // Inspect files being compiled with address scope resolution
    std::vector<std::string> files_to_check;
    std::unordered_set<std::string> seen_canonical_paths;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg.empty() || arg[0] == L'-') continue;

        if (ends_with(arg, L".c") || ends_with(arg, L".cpp") ||
            ends_with(arg, L".cc") || ends_with(arg, L".cxx") ||
            ends_with(arg, L".C") || ends_with(arg, L".CPP")) {

            // Convert to UTF-8
            int len = WideCharToMultiByte(CP_UTF8, 0, arg.c_str(), -1, NULL, 0, NULL, NULL);
            if (len > 0) {
                std::string path(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, arg.c_str(), -1, &path[0], len, NULL, NULL);

                DWORD attr = GetFileAttributesA(path.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string canon = safe_c::resolve_canonical_path(path);
                    if (seen_canonical_paths.insert(canon).second) {
                        files_to_check.push_back(path);
                    }
                }
            }
        }
    }

    // Run Safe-C static memory safety analyzer
    safe_c::core::Analyzer analyzer(safe_c::Mode::FatalError);

    // Support standard GCC diagnostic color flags (-fdiagnostics-color, -fno-diagnostics-color)
    safe_c::core::ColorChoice color_choice = safe_c::core::ColorChoice::Auto;
    bool show_highlights = true;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-fdiagnostics-color=always" || arg == L"-fdiagnostics-color") {
            color_choice = safe_c::core::ColorChoice::Always;
        } else if (arg == L"-fdiagnostics-color=never" || arg == L"-fno-diagnostics-color") {
            color_choice = safe_c::core::ColorChoice::Never;
        } else if (arg == L"-fdiagnostics-color=auto") {
            color_choice = safe_c::core::ColorChoice::Auto;
        } else if (arg == L"-fno-diagnostics-show-highlight-colors") {
            show_highlights = false;
        } else if (arg == L"-fdiagnostics-show-highlight-colors") {
            show_highlights = true;
        }
    }
    analyzer.set_color_mode(color_choice);
    analyzer.set_highlight_colors(show_highlights);

    LocalFree(argv);

    if (files_to_check.empty()) {
        return 0;
    }

    bool has_violation = false;
    for (const auto& file : files_to_check) {
        bool ok = analyzer.analyze_file(file);
        if (!ok || analyzer.has_errors()) {
            has_violation = true;
        }
    }

    if (has_violation) {
        // Halt compilation directly at compile time
        std::ostringstream oss;
        analyzer.print_diagnostics(oss);
        std::string msg = oss.str();

        fprintf(stderr, "%s", msg.c_str());
        fflush(stderr);

        ExitProcess(1);
    }
    return 0;
}

} // anonymous namespace

// Pure Safe-C Addon Exports
extern "C" {

SAFE_C_API const char* safe_c_addon_version(void) {
    return SAFE_C_VERSION_STRING;
}

SAFE_C_API int safe_c_addon_inspect(void) {
    return check_compiler_invocations();
}

// GCC Plugin standard ABI export compatibility
SAFE_C_API int plugin_is_GPL_compatible = 1;

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;

    if (fdwReason == DLL_PROCESS_ATTACH) {
        check_compiler_invocations();
    }
    return TRUE;
}

#endif // _WIN32
