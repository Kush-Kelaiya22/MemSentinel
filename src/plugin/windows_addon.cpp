#if defined(_WIN32)

#include <windows.h>
#include <pthread.h>
#include <shellapi.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "analyzer.h"
#include "safe_c/common.h"

// Real libwinpthread-1 function pointer types
typedef int (*fn_clock_gettime64)(clockid_t, struct _timespec64*);
typedef int (*fn_pthread_cond_broadcast)(pthread_cond_t*);
typedef int (*fn_pthread_cond_wait)(pthread_cond_t*, pthread_mutex_t*);
typedef void* (*fn_pthread_getspecific)(pthread_key_t);
typedef int (*fn_pthread_key_create)(pthread_key_t*, void (*)(void*));
typedef int (*fn_pthread_mutex_destroy)(pthread_mutex_t*);
typedef int (*fn_pthread_mutex_init)(pthread_mutex_t*, const pthread_mutexattr_t*);
typedef int (*fn_pthread_mutex_lock)(pthread_mutex_t*);
typedef int (*fn_pthread_mutex_unlock)(pthread_mutex_t*);
typedef int (*fn_pthread_once)(pthread_once_t*, void (*)(void));
typedef int (*fn_pthread_setspecific)(pthread_key_t, const void*);

static HMODULE g_real_pthread = nullptr;
static bool g_loading = false;

static fn_clock_gettime64 p_clock_gettime64 = nullptr;
static fn_pthread_cond_broadcast p_pthread_cond_broadcast = nullptr;
static fn_pthread_cond_wait p_pthread_cond_wait = nullptr;
static fn_pthread_getspecific p_pthread_getspecific = nullptr;
static fn_pthread_key_create p_pthread_key_create = nullptr;
static fn_pthread_mutex_destroy p_pthread_mutex_destroy = nullptr;
static fn_pthread_mutex_init p_pthread_mutex_init = nullptr;
static fn_pthread_mutex_lock p_pthread_mutex_lock = nullptr;
static fn_pthread_mutex_unlock p_pthread_mutex_unlock = nullptr;
static fn_pthread_once p_pthread_once = nullptr;
static fn_pthread_setspecific p_pthread_setspecific = nullptr;

static void load_real_pthread() {
    if (g_real_pthread || g_loading) return;
    g_loading = true;

    // Try canonical MinGW bin path first
    g_real_pthread = LoadLibraryW(L"F:\\mingw-w64\\mingw64\\bin\\libwinpthread-1.dll");

    if (!g_real_pthread) {
        wchar_t buf[MAX_PATH] = {0};
        if (GetModuleFileNameW(NULL, buf, MAX_PATH)) {
            // Find root by walking up from libexec/gcc/<target>/<version>/cc1.exe
            for (int i = 0; i < 5; ++i) {
                wchar_t* last = wcsrchr(buf, L'\\');
                if (last) *last = L'\0';
            }
            wcscat_s(buf, L"\\bin\\libwinpthread-1.dll");
            g_real_pthread = LoadLibraryW(buf);
        }
    }

    if (g_real_pthread) {
        p_clock_gettime64 = (fn_clock_gettime64)GetProcAddress(g_real_pthread, "clock_gettime64");
        p_pthread_cond_broadcast = (fn_pthread_cond_broadcast)GetProcAddress(g_real_pthread, "pthread_cond_broadcast");
        p_pthread_cond_wait = (fn_pthread_cond_wait)GetProcAddress(g_real_pthread, "pthread_cond_wait");
        p_pthread_getspecific = (fn_pthread_getspecific)GetProcAddress(g_real_pthread, "pthread_getspecific");
        p_pthread_key_create = (fn_pthread_key_create)GetProcAddress(g_real_pthread, "pthread_key_create");
        p_pthread_mutex_destroy = (fn_pthread_mutex_destroy)GetProcAddress(g_real_pthread, "pthread_mutex_destroy");
        p_pthread_mutex_init = (fn_pthread_mutex_init)GetProcAddress(g_real_pthread, "pthread_mutex_init");
        p_pthread_mutex_lock = (fn_pthread_mutex_lock)GetProcAddress(g_real_pthread, "pthread_mutex_lock");
        p_pthread_mutex_unlock = (fn_pthread_mutex_unlock)GetProcAddress(g_real_pthread, "pthread_mutex_unlock");
        p_pthread_once = (fn_pthread_once)GetProcAddress(g_real_pthread, "pthread_once");
        p_pthread_setspecific = (fn_pthread_setspecific)GetProcAddress(g_real_pthread, "pthread_setspecific");
    }
    g_loading = false;
}

// Forwarded exports with exact ABI signatures matching pthread.h
extern "C" {

__declspec(dllexport) int clock_gettime64(clockid_t clk_id, struct _timespec64* tp) {
    if (!g_real_pthread) load_real_pthread();
    return p_clock_gettime64 ? p_clock_gettime64(clk_id, tp) : 0;
}

__declspec(dllexport) int pthread_cond_broadcast(pthread_cond_t* cond) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_cond_broadcast ? p_pthread_cond_broadcast(cond) : 0;
}

__declspec(dllexport) int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_cond_wait ? p_pthread_cond_wait(cond, mutex) : 0;
}

__declspec(dllexport) void* pthread_getspecific(pthread_key_t key) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_getspecific ? p_pthread_getspecific(key) : nullptr;
}

__declspec(dllexport) int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_key_create ? p_pthread_key_create(key, destructor) : 0;
}

__declspec(dllexport) int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_mutex_destroy ? p_pthread_mutex_destroy(mutex) : 0;
}

__declspec(dllexport) int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_mutex_init ? p_pthread_mutex_init(mutex, attr) : 0;
}

__declspec(dllexport) int pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_mutex_lock ? p_pthread_mutex_lock(mutex) : 0;
}

__declspec(dllexport) int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_mutex_unlock ? p_pthread_mutex_unlock(mutex) : 0;
}

__declspec(dllexport) int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_once ? p_pthread_once(once_control, init_routine) : 0;
}

__declspec(dllexport) int pthread_setspecific(pthread_key_t key, const void* value) {
    if (!g_real_pthread) load_real_pthread();
    return p_pthread_setspecific ? p_pthread_setspecific(key, value) : 0;
}

// GCC Plugin standard ABI export compatibility
__declspec(dllexport) int plugin_is_GPL_compatible = 1;

} // extern "C"

static bool ends_with(const std::wstring& str, const std::wstring& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

static void check_compiler_invocations() {
    wchar_t exe_name[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exe_name, MAX_PATH);

    std::wstring exe_str(exe_name);
    // Only intercept when executed by cc1.exe (GCC) or cc1plus.exe (G++)
    if (exe_str.find(L"cc1.exe") == std::wstring::npos &&
        exe_str.find(L"cc1plus.exe") == std::wstring::npos) {
        return;
    }

    wchar_t env_val[32] = {0};
    if ((GetEnvironmentVariableW(L"SAFE_C_DISABLE", env_val, 32) > 0 ||
         GetEnvironmentVariableW(L"SAFE_CPP_DISABLE", env_val, 32) > 0) &&
        (wcsstr(env_val, L"1") != NULL || _wcsnicmp(env_val, L"true", 4) == 0)) {
        return;
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2) {
        if (argv) LocalFree(argv);
        return;
    }

    // Check for disable flags
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-fno-safe-c" || arg == L"-fno-safe-cpp" ||
            arg == L"-fplugin-arg-safe_c-disable" || arg == L"-fplugin-arg-safe_cpp-disable" ||
            arg == L"--no-safe-c" || arg == L"--no-safe-cpp" ||
            arg == L"-fplugin-arg-memsentinel-disable" ||
            wcsstr(arg.c_str(), L"SAFE_C_DISABLE") != NULL ||
            wcsstr(arg.c_str(), L"SAFE_CPP_DISABLE") != NULL ||
            (arg == L"-D" && i + 1 < argc &&
             (wcscmp(argv[i+1], L"SAFE_C_DISABLE") == 0 || wcscmp(argv[i+1], L"SAFE_CPP_DISABLE") == 0))) {
            LocalFree(argv);
            return;
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

    LocalFree(argv);

    if (files_to_check.empty()) {
        return;
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

        // Print directly to stderr
        fprintf(stderr, "%s", msg.c_str());
        fflush(stderr);

        ExitProcess(1);
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;

    if (fdwReason == DLL_PROCESS_ATTACH) {
        check_compiler_invocations();
    }
    return TRUE;
}

#endif // _WIN32
