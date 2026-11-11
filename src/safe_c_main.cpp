#include "analyzer.h"
#include "safe_c/common.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

static const char* EMBEDDED_SPECS =
"# Safe-C Toolchain Specs File for GCC / G++ (MinGW-w64 and Linux)\n"
"# Instructs GCC driver to invoke Safe-C static analysis before running cc1/cc1plus.\n"
"# To disable Safe-C: pass -DSAFE_C_DISABLE, -fno-safe-c, or set SAFE_C_DISABLE=1.\n"
"\n"
"@c:\n"
"safe_c --check-only %{D*&U*&A*} %{fdiagnostics-*} %i\n"
"cc1 %(cpp_unique_options) %(cc1_options) %(invoke_as)\n"
"\n"
"@c++:\n"
"safe_c --check-only %{D*&U*&A*} %{fdiagnostics-*} %i\n"
"cc1plus %(cpp_unique_options) %(cc1_options) %(invoke_as)\n";

bool query_compiler_install_dir(const std::string& compiler_cmd, std::string& out_install_dir, std::string& out_bin_dir) {
    std::string cmd = compiler_cmd + " -print-search-dirs";
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        return false;
    }

    char buf[1024];
    std::string output;
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
#if defined(_WIN32)
    int ret = _pclose(pipe);
#else
    int ret = pclose(pipe);
#endif

    if (ret != 0 || output.empty()) {
        return false;
    }

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("install: ", 0) == 0) {
            std::string dir = line.substr(9);
            while (!dir.empty() && (dir.back() == ' ' || dir.back() == '\t')) {
                dir.pop_back();
            }
            out_install_dir = dir;
            break;
        }
    }

    if (out_install_dir.empty()) {
        return false;
    }

    fs::path inst_path(out_install_dir);
    fs::path bin_candidate = (inst_path / "../../../../bin").lexically_normal();
    if (fs::exists(bin_candidate) && fs::is_directory(bin_candidate)) {
        out_bin_dir = bin_candidate.string();
    } else {
        bin_candidate = (inst_path / "../../bin").lexically_normal();
        if (fs::exists(bin_candidate) && fs::is_directory(bin_candidate)) {
            out_bin_dir = bin_candidate.string();
        }
    }

    return true;
}

fs::path get_current_executable_path() {
#if defined(_WIN32)
    wchar_t exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    return fs::path(exe_path);
#else
    char result[1024] = {0};
    ssize_t count = readlink("/proc/self/exe", result, sizeof(result) - 1);
    if (count > 0) {
        return fs::path(std::string(result, static_cast<size_t>(count)));
    }
    return fs::path("safe_c");
#endif
}

bool copy_driver_binary_variants(const fs::path& cur_exe, const fs::path& target_dir) {
    std::error_code ec;
    // Names to provide: safe_c, safe-c, safe-gcc, safe-g++
    std::vector<std::string> names = {"safe_c", "safe-c", "safe-gcc", "safe-g++"};
    std::string ext = cur_exe.extension().string();

    for (const auto& name : names) {
        fs::path target_exe = target_dir / (name + ext);
        if (cur_exe != target_exe) {
            fs::copy_file(cur_exe, target_exe, fs::copy_options::overwrite_existing, ec);
        }
    }
    return true;
}

bool enable_default_for_compiler(const std::string& compiler_cmd) {
    std::string install_dir, bin_dir;
    if (!query_compiler_install_dir(compiler_cmd, install_dir, bin_dir)) {
        std::cerr << "safe-c: warning: could not determine install directory for '" << compiler_cmd << "'.\n";
        return false;
    }

    fs::path install_path(install_dir);
    fs::path specs_file = install_path / "specs";

    std::ofstream ofs(specs_file, std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "safe-c: error: cannot write specs file to " << specs_file.string() << "\n";
        return false;
    }
    ofs << EMBEDDED_SPECS;
    ofs.close();

    std::cout << "[Safe-C] Enabled by default for compiler: " << compiler_cmd << "\n";
    std::cout << "         Installed specs: " << specs_file.string() << "\n";

    if (!bin_dir.empty()) {
        fs::path cur_exe = get_current_executable_path();
        if (fs::exists(cur_exe)) {
            copy_driver_binary_variants(cur_exe, fs::path(bin_dir));
            std::cout << "         Installed compiler binaries (safe-c, safe_c, safe-gcc, safe-g++) into: " << bin_dir << "\n";
        }
    }

    std::cout << "         All invocations of '" << compiler_cmd << "' will now enforce Safe-C memory safety by default.\n";
    return true;
}

bool disable_default_for_compiler(const std::string& compiler_cmd) {
    std::string install_dir, bin_dir;
    if (!query_compiler_install_dir(compiler_cmd, install_dir, bin_dir)) {
        return false;
    }

    fs::path specs_file = fs::path(install_dir) / "specs";
    std::error_code ec;
    if (fs::exists(specs_file)) {
        fs::remove(specs_file, ec);
        std::cout << "[Safe-C] Disabled default auto-loading for: " << compiler_cmd << "\n";
        std::cout << "         Removed specs: " << specs_file.string() << "\n";
        return true;
    }
    std::cout << "[Safe-C] Default auto-loading was not active for " << compiler_cmd << ".\n";
    return true;
}

void print_status_for_compiler(const std::string& compiler_cmd) {
    std::string install_dir, bin_dir;
    if (!query_compiler_install_dir(compiler_cmd, install_dir, bin_dir)) {
        std::cout << "[Safe-C] Compiler '" << compiler_cmd << "': Not found in PATH or not executable.\n";
        return;
    }
    fs::path specs_file = fs::path(install_dir) / "specs";
    if (fs::exists(specs_file)) {
        std::cout << "[Safe-C] Compiler '" << compiler_cmd << "': ACTIVE BY DEFAULT (specs: " << specs_file.string() << ")\n";
    } else {
        std::cout << "[Safe-C] Compiler '" << compiler_cmd << "': INACTIVE by default (run 'safe-c --enable-default' to enable).\n";
    }
}

bool ends_with_ci(std::string_view str, std::string_view suffix) {
    if (str.length() < suffix.length()) return false;
    size_t offset = str.length() - suffix.length();
    for (size_t i = 0; i < suffix.length(); ++i) {
        if (std::tolower(static_cast<unsigned char>(str[offset + i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

bool is_cpp_file(std::string_view path) {
    return ends_with_ci(path, ".cpp") || ends_with_ci(path, ".cxx") ||
           ends_with_ci(path, ".cc")  || ends_with_ci(path, ".c++") ||
           ends_with_ci(path, ".CPP");
}

bool is_c_or_cpp_file(std::string_view path) {
    return ends_with_ci(path, ".c") || is_cpp_file(path);
}

int forward_to_compiler(bool is_cpp, const std::vector<std::string>& raw_args) {
    std::string compiler;
    if (is_cpp) {
        const char* cxx = std::getenv("CXX");
        compiler = (cxx && cxx[0] != '\0') ? cxx : "g++";
    } else {
        const char* cc = std::getenv("CC");
        compiler = (cc && cc[0] != '\0') ? cc : "gcc";
    }

    std::string cmd = compiler;
    for (size_t i = 1; i < raw_args.size(); ++i) {
        const std::string& a = raw_args[i];
        if (a == "--check-only" || a == "-check-only") continue;

        cmd += " ";
        if (a.find(' ') != std::string::npos || a.find('"') != std::string::npos) {
            cmd += "\"";
            for (char ch : a) {
                if (ch == '"') cmd += "\\\"";
                else cmd += ch;
            }
            cmd += "\"";
        } else {
            cmd += a;
        }
    }

#if defined(_WIN32)
    _putenv("SAFE_C_RECURSION_GUARD=1");
#else
    setenv("SAFE_C_RECURSION_GUARD", "1", 1);
#endif

    int ret = std::system(cmd.c_str());

#if defined(_WIN32)
    _putenv("SAFE_C_RECURSION_GUARD=0");
#else
    unsetenv("SAFE_C_RECURSION_GUARD");
#endif

    return ret;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    const char* recurse_env = std::getenv("SAFE_C_RECURSION_GUARD");
    if (recurse_env && std::strcmp(recurse_env, "1") == 0) {
        return 0;
    }

    std::vector<std::string> raw_args;
    for (int i = 0; i < argc; ++i) {
        raw_args.emplace_back(argv[i]);
    }

    // Determine invocation role from executable name
    std::string prog_name = fs::path(raw_args[0]).stem().string();
    bool invoked_as_cpp = (prog_name.find("++") != std::string::npos || prog_name.find("gxx") != std::string::npos);

    if (argc < 2) {
        std::cerr << "Safe-C: Memory-Safe Compiler Driver v" << SAFE_C_VERSION_STRING << "\n";
        std::cerr << "Usage: " << prog_name << " [options] <source-files...>\n\n";
        std::cerr << "Compiler Invocations:\n";
        std::cerr << "  " << prog_name << " main.c -o main.exe          Compile C with compile-time memory safety\n";
        std::cerr << "  safe-g++ app.cpp -o app.exe         Compile C++ with compile-time memory safety\n";
        std::cerr << "  " << prog_name << " --check-only main.c        Perform memory safety analysis only (no compilation)\n\n";
        std::cerr << "Default Toolchain Activation:\n";
        std::cerr << "  " << prog_name << " --enable-default [gcc]     Activate Safe-C by default for host GCC/G++\n";
        std::cerr << "  " << prog_name << " --disable-default [gcc]    Deactivate Safe-C default auto-loading\n";
        std::cerr << "  " << prog_name << " --status [gcc]             Check current default auto-loading status\n";
        return 0;
    }

    // Management subcommands
    std::string first_arg = raw_args[1];
    if (first_arg == "--enable-default" || first_arg == "--install" || first_arg == "-enable-default") {
        std::vector<std::string> compilers;
        if (argc >= 3) {
            for (int i = 2; i < argc; ++i) compilers.push_back(raw_args[i]);
        } else {
            compilers.push_back("gcc");
#if defined(_WIN32)
            if (fs::exists("F:\\mingw-w64\\mingw64\\bin\\gcc.exe")) {
                compilers.push_back("F:\\mingw-w64\\mingw64\\bin\\gcc.exe");
            }
#endif
        }
        bool ok = true;
        for (const auto& c : compilers) {
            if (!enable_default_for_compiler(c)) ok = false;
        }
        return ok ? 0 : 1;
    }

    if (first_arg == "--disable-default" || first_arg == "--uninstall" || first_arg == "-disable-default") {
        std::vector<std::string> compilers;
        if (argc >= 3) {
            for (int i = 2; i < argc; ++i) compilers.push_back(raw_args[i]);
        } else {
            compilers.push_back("gcc");
#if defined(_WIN32)
            if (fs::exists("F:\\mingw-w64\\mingw64\\bin\\gcc.exe")) {
                compilers.push_back("F:\\mingw-w64\\mingw64\\bin\\gcc.exe");
            }
#endif
        }
        for (const auto& c : compilers) {
            disable_default_for_compiler(c);
        }
        return 0;
    }

    if (first_arg == "--status" || first_arg == "-status") {
        std::vector<std::string> compilers;
        if (argc >= 3) {
            for (int i = 2; i < argc; ++i) compilers.push_back(raw_args[i]);
        } else {
            compilers.push_back("gcc");
#if defined(_WIN32)
            if (fs::exists("F:\\mingw-w64\\mingw64\\bin\\gcc.exe")) {
                compilers.push_back("F:\\mingw-w64\\mingw64\\bin\\gcc.exe");
            }
#endif
        }
        for (const auto& c : compilers) {
            print_status_for_compiler(c);
        }
        return 0;
    }

    // Check disable environment variables
    bool is_disabled = false;
    const char* disable_env = std::getenv("SAFE_C_DISABLE");
    if (!disable_env) disable_env = std::getenv("SAFE_CPP_DISABLE");
    if (disable_env && (std::strcmp(disable_env, "1") == 0 || std::strcmp(disable_env, "true") == 0)) {
        is_disabled = true;
    }

    safe_c::core::ColorChoice color_choice = safe_c::core::ColorChoice::Auto;
    bool show_highlights = true;
    bool check_only = false;
    std::vector<std::string> source_files;

    for (int i = 1; i < argc; ++i) {
        std::string arg = raw_args[i];
        if (arg == "--check-only" || arg == "-check-only") {
            check_only = true;
            continue;
        }
        if (arg == "-D" && i + 1 < argc) {
            std::string def = raw_args[++i];
            if (def == "SAFE_C_DISABLE" || def == "SAFE_CPP_DISABLE") {
                is_disabled = true;
            }
            continue;
        }
        if (arg == "-DSAFE_C_DISABLE" || arg == "-DSAFE_CPP_DISABLE" ||
            arg == "-fno-safe-c" || arg == "--no-safe-c" ||
            arg == "-fplugin-arg-safe_c-disable") {
            is_disabled = true;
            continue;
        }
        if (arg.rfind("-D", 0) == 0 || arg.rfind("-U", 0) == 0 || arg.rfind("-A", 0) == 0) {
            continue;
        }
        if (arg == "-fdiagnostics-color=always" || arg == "-fdiagnostics-color") {
            color_choice = safe_c::core::ColorChoice::Always;
            continue;
        } else if (arg == "-fdiagnostics-color=never" || arg == "-fno-diagnostics-color") {
            color_choice = safe_c::core::ColorChoice::Never;
            continue;
        } else if (arg == "-fdiagnostics-color=auto") {
            color_choice = safe_c::core::ColorChoice::Auto;
            continue;
        } else if (arg == "-fno-diagnostics-show-highlight-colors") {
            show_highlights = false;
            continue;
        } else if (arg == "-fdiagnostics-show-highlight-colors") {
            show_highlights = true;
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            continue;
        }

        if (is_c_or_cpp_file(arg)) {
            source_files.push_back(arg);
            if (is_cpp_file(arg)) {
                invoked_as_cpp = true;
            }
        }
    }

    // If Safe-C is disabled, forward directly to underlying compiler
    if (is_disabled) {
        if (check_only) return 0;
        return forward_to_compiler(invoked_as_cpp, raw_args);
    }

    // Run static memory safety analyzer on source files
    if (!source_files.empty()) {
        safe_c::core::Analyzer analyzer(safe_c::Mode::FatalError);
        analyzer.set_color_mode(color_choice);
        analyzer.set_highlight_colors(show_highlights);

        bool has_violations = false;
        for (const auto& file : source_files) {
            if (!analyzer.analyze_file(file) || analyzer.has_errors()) {
                has_violations = true;
            }
        }

        if (has_violations) {
            analyzer.print_diagnostics(std::cerr);
            return 1;
        }
    }

    // If check-only, exit here without invoking host compiler
    if (check_only) {
        return 0;
    }

    // If no compiler flags or files, or if invoked as compiler driver, forward to host gcc/g++
    return forward_to_compiler(invoked_as_cpp, raw_args);
}
