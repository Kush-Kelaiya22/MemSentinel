#include "analyzer.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

static bool is_source_file(const std::string& path_str) {
    fs::path p(path_str);
    std::string ext = p.extension().string();
    return ext == ".c" || ext == ".cpp" || ext == ".cc" || ext == ".cxx";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "MemSentinel v1.0.0 — Compile-Time Memory Safety Extension for GCC/G++\n";
        std::cerr << "Usage: memsentinel [gcc/g++ options] <source files...>\n";
        std::cerr << "       memsentinel --check <source-file>\n";
        std::cerr << "       memsentinel --version\n";
        return 1;
    }

    std::vector<std::string> args;
    std::vector<std::string> source_files;
    bool disable_checking = false;
    bool check_only = false;
    bool is_cpp = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "memsentinel version 1.0.0 (Native Memory Safety Engine)\n";
        }
        if (arg == "--memsentinel-disable" || arg == "-fplugin-arg-memsentinel-disable" ||
            arg == "--memcheck-disable") {
            disable_checking = true;
            continue;
        }
        if (arg == "--check") {
            check_only = true;
            continue;
        }
        if (is_source_file(arg)) {
            source_files.push_back(arg);
            if (arg.find(".cpp") != std::string::npos || arg.find(".cc") != std::string::npos ||
                arg.find(".cxx") != std::string::npos) {
                is_cpp = true;
            }
        }
        args.push_back(arg);
    }

    // 1. Run Static Memory Safety Analysis on all source files
    if (!disable_checking && !source_files.empty()) {
        memcheck::core::Analyzer analyzer(memcheck::Mode::FatalError);
        bool has_violation = false;

        for (const auto& file : source_files) {
            analyzer.clear();
            bool ok = analyzer.analyze_file(file);
            if (!ok || analyzer.has_errors()) {
                analyzer.print_diagnostics(std::cerr);
                has_violation = true;
            }
        }

        if (has_violation) {
            // Memory unsafety detected: Halt compilation immediately!
            return 1;
        }
    }

    if (check_only) {
        std::cout << "MemSentinel: All sources passed memory safety verification.\n";
        return 0;
    }

    // 2. Invoke the underlying compiler to generate binaries
    std::string compiler_cmd = is_cpp ? "g++" : "gcc";
    const char* env_gcc = std::getenv("MEMSENTINEL_CC");
    if (env_gcc && *env_gcc) {
        compiler_cmd = env_gcc;
    }

    std::string full_cmd = compiler_cmd;
    for (const auto& a : args) {
        full_cmd += " \"" + a + "\"";
    }

    return std::system(full_cmd.c_str());
}
