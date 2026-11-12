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

    bool enabled = true;
    wchar_t env_val[32] = {0};
    if ((GetEnvironmentVariableW(L"SAFE_C_DISABLE", env_val, 32) > 0 ||
         GetEnvironmentVariableW(L"SAFE_CPP_DISABLE", env_val, 32) > 0) &&
        (wcsstr(env_val, L"1") != NULL || _wcsnicmp(env_val, L"true", 4) == 0)) {
        enabled = false;
    }
    if (GetEnvironmentVariableW(L"SAFE_C_ENABLE", env_val, 32) > 0 &&
        (wcsstr(env_val, L"1") != NULL || _wcsnicmp(env_val, L"true", 4) == 0)) {
        enabled = true;
    }

    safe_c::Mode mode = safe_c::Mode::FatalError;
    if (GetEnvironmentVariableW(L"SAFE_C_MODE", env_val, 32) > 0) {
        if (_wcsicmp(env_val, L"warn") == 0) {
            mode = safe_c::Mode::WarningOnly;
        } else if (_wcsicmp(env_val, L"off") == 0) {
            enabled = false;
        }
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2) {
        if (argv) LocalFree(argv);
        return 0;
    }

    safe_c::core::CheckOptions checks;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-DSAFE_C_ENABLE" || arg == L"-fplugin-arg-safe_c-enable" ||
            (arg == L"-D" && i + 1 < argc && wcscmp(argv[i + 1], L"SAFE_C_ENABLE") == 0)) {
            enabled = true;
        } else if (arg == L"-fno-safe-c" || arg == L"-fno-safe-cpp" ||
                   arg == L"-fplugin-arg-safe_c-disable" || arg == L"-fplugin-arg-safe_cpp-disable" ||
                   arg == L"-fplugin-arg-safe_c-mode=off" ||
                   arg == L"--no-safe-c" || arg == L"--no-safe-cpp" ||
                   arg == L"-DSAFE_C_DISABLE" || arg == L"-DSAFE_CPP_DISABLE" ||
                   (arg == L"-D" && i + 1 < argc &&
                    (wcscmp(argv[i + 1], L"SAFE_C_DISABLE") == 0 || wcscmp(argv[i + 1], L"SAFE_CPP_DISABLE") == 0))) {
            enabled = false;
        } else if (arg == L"-DSAFE_C_WARN_ONLY" || arg == L"-fplugin-arg-safe_c-mode=warn") {
            mode = safe_c::Mode::WarningOnly;
        } else if (arg == L"-DSAFE_C_FATAL" || arg == L"-fplugin-arg-safe_c-mode=fatal") {
            mode = safe_c::Mode::FatalError;
        } else if (arg == L"-DSAFE_C_NO_LEAKS" || arg == L"-fplugin-arg-safe_c-leaks=off" || arg == L"-fplugin-arg-safe_c-no-leaks") {
            checks.leaks = false;
        } else if (arg == L"-DSAFE_C_NO_DOUBLE_FREE" || arg == L"-fplugin-arg-safe_c-double-free=off" || arg == L"-fplugin-arg-safe_c-no-double-free") {
            checks.double_free = false;
        } else if (arg == L"-DSAFE_C_NO_UAF" || arg == L"-fplugin-arg-safe_c-uaf=off" || arg == L"-fplugin-arg-safe_c-no-uaf") {
            checks.uaf = false;
        } else if (arg == L"-DSAFE_C_NO_BOUNDS" || arg == L"-fplugin-arg-safe_c-bounds=off" || arg == L"-fplugin-arg-safe_c-no-bounds") {
            checks.bounds = false;
        } else if (arg == L"-DSAFE_C_NO_NULL" || arg == L"-fplugin-arg-safe_c-null=off" || arg == L"-fplugin-arg-safe_c-no-null") {
            checks.null_deref = false;
        } else if (arg == L"-DSAFE_C_NO_UNINIT" || arg == L"-fplugin-arg-safe_c-uninit=off" || arg == L"-fplugin-arg-safe_c-no-uninit") {
            checks.uninit = false;
        } else if (arg == L"-DSAFE_C_NO_DANGLING" || arg == L"-fplugin-arg-safe_c-dangling=off" || arg == L"-fplugin-arg-safe_c-no-dangling") {
            checks.dangling = false;
        } else if (arg == L"-DSAFE_C_NO_MISMATCH" || arg == L"-fplugin-arg-safe_c-mismatch=off" || arg == L"-fplugin-arg-safe_c-no-mismatch") {
            checks.mismatch = false;
        } else if (arg == L"-DSAFE_C_NO_ARITHMETIC" || arg == L"-fplugin-arg-safe_c-arithmetic=off" || arg == L"-fplugin-arg-safe_c-no-arithmetic") {
            checks.arithmetic = false;
        } else if (arg == L"-DSAFE_C_NO_ITERATOR" || arg == L"-fplugin-arg-safe_c-iterator=off" || arg == L"-fplugin-arg-safe_c-no-iterator") {
            checks.iterator = false;
        }
    }

    if (!enabled) {
        LocalFree(argv);
        return 0;
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

    safe_c::core::Analyzer analyzer(mode);
    analyzer.set_check_options(checks);

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
        std::ostringstream oss;
        analyzer.print_diagnostics(oss);
        std::string msg = oss.str();

        fprintf(stderr, "%s", msg.c_str());
        fflush(stderr);

        if (mode == safe_c::Mode::FatalError) {
            ExitProcess(1);
        }
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
