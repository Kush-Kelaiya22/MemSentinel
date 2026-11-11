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
"safe_c %{D*&U*&A*} %{fdiagnostics-*} %i\n"
"cc1 %(cpp_unique_options) %(cc1_options) %(invoke_as)\n"
"\n"
"@c++:\n"
"safe_c %{D*&U*&A*} %{fdiagnostics-*} %i\n"
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
            fs::path target_exe = fs::path(bin_dir) / cur_exe.filename();
            if (cur_exe != target_exe) {
                std::error_code ec;
                fs::copy_file(cur_exe, target_exe, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    std::cout << "         Copied driver binary to: " << target_exe.string() << "\n";
                }
            }
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
        std::cout << "[Safe-C] Compiler '" << compiler_cmd << "': INACTIVE by default (run 'safe_c --enable-default' to enable).\n";
    }
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Safe-C static memory safety analyzer v" << SAFE_C_VERSION_STRING << "\n";
        std::cerr << "Usage: safe_c [options] <source-files...>\n\n";
        std::cerr << "Default Auto-Loading Management:\n";
        std::cerr << "  --enable-default [compiler]    Enable Safe-C by default for host GCC/G++\n";
        std::cerr << "  --disable-default [compiler]   Disable Safe-C default auto-loading\n";
        std::cerr << "  --status [compiler]            Check whether Safe-C is active by default\n";
        return 0;
    }

    // Management subcommands
    std::string first_arg = argv[1];
    if (first_arg == "--enable-default" || first_arg == "--install" || first_arg == "-enable-default") {
        std::vector<std::string> compilers;
        if (argc >= 3) {
            for (int i = 2; i < argc; ++i) compilers.push_back(argv[i]);
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
            for (int i = 2; i < argc; ++i) compilers.push_back(argv[i]);
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
            for (int i = 2; i < argc; ++i) compilers.push_back(argv[i]);
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
