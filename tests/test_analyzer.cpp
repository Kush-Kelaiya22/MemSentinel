#ifdef NDEBUG
#undef NDEBUG
#endif
#include "analyzer.h"
#include "safe_c/api.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <unordered_map>

#ifndef TEST_SOURCE_DIR
#define TEST_SOURCE_DIR "."
#endif

namespace fs = std::filesystem;

static std::string test_path(const std::string& rel) {
    return (fs::path(TEST_SOURCE_DIR) / rel).string();
}

void test_compile_fail_cases() {
    std::cout << "[TEST] Running Compile-Fail Static Analysis Suite...\n";
    safe_c::core::Analyzer analyzer(safe_c::Mode::FatalError);

    // 1. Double Free
    analyzer.clear();
    bool pass1 = analyzer.analyze_file(test_path("tests/compile_fail/double_free.c"));
    assert(!pass1 && "double_free.c must fail compilation");
    bool found_df = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == safe_c::ViolationType::DoubleFree) found_df = true;
    }
    assert(found_df && "Expected DoubleFree diagnostic");
    (void)pass1; (void)found_df;
    std::cout << "  ✓ double_free.c correctly halted compilation.\n";

    // 2. Use After Free
    analyzer.clear();
    bool pass2 = analyzer.analyze_file(test_path("tests/compile_fail/use_after_free.cpp"));
    assert(!pass2 && "use_after_free.cpp must fail compilation");
    bool found_uaf = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == safe_c::ViolationType::UseAfterFree) found_uaf = true;
    }
    assert(found_uaf && "Expected UseAfterFree diagnostic");
    (void)pass2; (void)found_uaf;
    std::cout << "  ✓ use_after_free.cpp correctly halted compilation.\n";

    // 3. Altered Pointer Free
    analyzer.clear();
    bool pass3 = analyzer.analyze_file(test_path("tests/compile_fail/altered_pointer_free.c"));
    assert(!pass3 && "altered_pointer_free.c must fail compilation");
    bool found_mbf = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == safe_c::ViolationType::ModifiedBaseFree) found_mbf = true;
    }
    assert(found_mbf && "Expected ModifiedBaseFree diagnostic");
    (void)pass3; (void)found_mbf;
    std::cout << "  ✓ altered_pointer_free.c correctly halted compilation.\n";

    // 4. Memory Leak
    analyzer.clear();
    bool pass4 = analyzer.analyze_file(test_path("tests/compile_fail/memory_leak.c"));
    assert(!pass4 && "memory_leak.c must fail compilation");
    bool found_leak = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == safe_c::ViolationType::MemoryLeakUnfreed) found_leak = true;
    }
    assert(found_leak && "Expected MemoryLeakUnfreed diagnostic");
    (void)pass4; (void)found_leak;
    std::cout << "  ✓ memory_leak.c correctly halted compilation.\n";
}

void test_compile_pass_cases() {
    std::cout << "[TEST] Running Compile-Pass Verification Suite...\n";
    safe_c::core::Analyzer analyzer(safe_c::Mode::FatalError);

    // 1. Valid Pointers
    analyzer.clear();
    bool pass1 = analyzer.analyze_file(test_path("tests/compile_pass/valid_pointers.c"));
    assert(pass1 && "valid_pointers.c must compile cleanly");
    assert(analyzer.diagnostics().empty() && "No diagnostics expected for valid code");
    (void)pass1;
    std::cout << "  ✓ valid_pointers.c compiled cleanly.\n";

    // 2. Valid Arithmetic
    analyzer.clear();
    bool pass2 = analyzer.analyze_file(test_path("tests/compile_pass/valid_arithmetic.c"));
    assert(pass2 && "valid_arithmetic.c must compile cleanly");
    assert(analyzer.diagnostics().empty() && "No diagnostics expected for valid arithmetic");
    (void)pass2;
    std::cout << "  ✓ valid_arithmetic.c compiled cleanly.\n";

    // 3. Valid C++ RAII
    analyzer.clear();
    bool pass3 = analyzer.analyze_file(test_path("tests/compile_pass/valid_cpp_raii.cpp"));
    assert(pass3 && "valid_cpp_raii.cpp must compile cleanly");
    (void)pass3;
    std::cout << "  ✓ valid_cpp_raii.cpp compiled cleanly.\n";
}

void test_pointer_offset_invariants() {
    std::cout << "[TEST] Running Pointer Offset & Base Invariant Suite...\n";
    safe_c::core::Analyzer analyzer(safe_c::Mode::FatalError);

    // Test A: Cursor offset changed while base variable retains offset 0 -> PASS
    const char* valid_cursor =
        "void test_cursor(void) {\n"
        "    char* base = (char*)malloc(64);\n"
        "    char* cursor = base;\n"
        "    cursor += 8;\n"
        "    cursor++;\n"
        "    free(base);\n"
        "}\n";
    analyzer.clear();
    bool ok_cursor = analyzer.analyze_source(valid_cursor, "test_cursor.c");
    assert(ok_cursor && "Cursor offset modification with base variable must pass");
    assert(analyzer.diagnostics().empty());
    (void)ok_cursor;
    std::cout << "  ✓ Changing cursor offset while base exists verified cleanly.\n";

    // Test B: Base variable shifted while backup variable retains base address -> PASS
    const char* valid_shift =
        "void test_shift(void) {\n"
        "    char* base = (char*)malloc(64);\n"
        "    char* orig = base;\n"
        "    base += 16;\n"
        "    free(orig);\n"
        "}\n";
    analyzer.clear();
    bool ok_shift = analyzer.analyze_source(valid_shift, "test_shift.c");
    assert(ok_shift && "Base offset modification with orig variable must pass");
    assert(analyzer.diagnostics().empty());
    (void)ok_shift;
    std::cout << "  ✓ Shifting base pointer when backup retains original address verified cleanly.\n";

    // Test C: Offset changed when NO variable retains base address -> FAIL
    const char* lost_base =
        "void test_lost(void) {\n"
        "    char* p = (char*)malloc(32);\n"
        "    p += 4;\n"
        "    free(p);\n"
        "}\n";
    analyzer.clear();
    bool fail_lost = analyzer.analyze_source(lost_base, "test_lost.c");
    assert(!fail_lost && "Offset modification without base variable must fail");
    bool found_mbf = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == safe_c::ViolationType::ModifiedBaseFree) found_mbf = true;
    }
    assert(found_mbf && "Expected ModifiedBaseFree diagnostic for lost base");
    (void)fail_lost; (void)found_mbf;
    std::cout << "  ✓ Offset alteration without base variable correctly rejected.\n";
}

void test_address_scope_resolution() {
    std::cout << "[TEST] Running Address Scope Resolution & Deduplication Suite...\n";

    // 1. Path canonicalization identity
    std::string rel1 = "tests/compile_fail/double_free.c";
    std::string rel2 = ".\\tests\\compile_fail\\double_free.c";
    std::string canon1 = safe_c::resolve_canonical_path(test_path(rel1));
    std::string canon2 = safe_c::resolve_canonical_path(test_path(rel2));
    assert(!canon1.empty() && "Canonical path 1 must not be empty");
    assert(!canon2.empty() && "Canonical path 2 must not be empty");
    assert(canon1 == canon2 && "Canonical paths must be identical across relative representations");

    safe_c::SourceLocation loc1(rel1, 6, 5);
    safe_c::SourceLocation loc2(rel2, 6, 5);
    assert(loc1.matches_file(loc2) && "SourceLocation must match across different relative notations");
    std::cout << "  ✓ Address scope resolution verified: .\\x and x resolve to exact same path.\n";

    // 2. Deduplication check: same error on same line recorded once only
    safe_c::core::Analyzer analyzer(safe_c::Mode::FatalError);
    analyzer.clear();

    const char* duplicate_error_code =
        "void test_dup() {\n"
        "    char* p = (char*)malloc(16);\n"
        "    free(p);\n"
        "    free(p);\n"
        "    free(p);\n"
        "}\n";

    analyzer.analyze_source(duplicate_error_code, "test_dup.c");
    std::unordered_map<uint32_t, int> df_per_line;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == safe_c::ViolationType::DoubleFree) {
            df_per_line[d.error_loc.line]++;
        }
    }
    for (const auto& [line, count] : df_per_line) {
        assert(count == 1 && "Each line must show at most 1 diagnostic of a given error type");
        (void)line; (void)count;
    }
    std::cout << "  ✓ Deduplication verified: exactly 1 diagnostic per line for a given error type.\n";
}

void test_lexical_block_scope_resolution() {
    std::cout << "[TEST] Running GCC-Style Lexical Block Scope Suite...\n";
    safe_c::core::Analyzer analyzer(safe_c::Mode::FatalError);

    // Case A: Inner block leaks heap pointer on block exit '}'
    const char* block_leak =
        "void test_block_leak() {\n"
        "    int x = 10;\n"
        "    {\n"
        "        char* inner_ptr = (char*)malloc(32);\n"
        "        inner_ptr[0] = 'a';\n"
        "    }\n"
        "}\n";

    analyzer.clear();
    bool pass_leak = analyzer.analyze_source(block_leak, "test_block.c");
    assert(!pass_leak && "Inner block unfreed allocation must fail compilation");
    bool found_leak = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == safe_c::ViolationType::MemoryLeakUnfreed && d.error_loc.line == 6) {
            found_leak = true;
        }
    }
    assert(found_leak && "Expected MemoryLeakUnfreed at line 6 block exit");
    (void)pass_leak; (void)found_leak;
    std::cout << "  ✓ Lexical block exit correctly caught unescaped block leak.\n";

    // Case B: Inner allocation escaped to outer scope variable -> Clean compilation
    const char* block_escape =
        "void test_block_escape() {\n"
        "    char* outer_ptr = 0;\n"
        "    {\n"
        "        char* inner_ptr = (char*)malloc(32);\n"
        "        outer_ptr = inner_ptr;\n"
        "    }\n"
        "    free(outer_ptr);\n"
        "}\n";

    analyzer.clear();
    bool pass_escape = analyzer.analyze_source(block_escape, "test_escape.c");
    assert(pass_escape && "Inner allocation escaped to outer variable must compile cleanly");
    assert(analyzer.diagnostics().empty());
    (void)pass_escape;
    std::cout << "  ✓ Scope escaping to outer variable correctly recognized and verified.\n";
}

void test_gnu_gcc_diagnostic_highlighting() {
    std::cout << "[TEST] Running GNU/GCC Error Highlighting & Gutter Suite...\n";
    safe_c::core::Analyzer analyzer(safe_c::Mode::FatalError);

    const char* code =
        "void test_colors() {\n"
        "    char* ptr = (char*)malloc(64);\n"
        "    free(ptr);\n"
        "    free(ptr);\n"
        "}\n";

    analyzer.clear();
    analyzer.analyze_source(code, "test_colors.c");
    assert(analyzer.has_errors());

    // 1. Plain text format check
    analyzer.set_color_mode(safe_c::core::ColorChoice::Never);
    std::ostringstream plain_oss;
    analyzer.print_diagnostics(plain_oss);
    std::string plain = plain_oss.str();

    assert(plain.find("test_colors.c:4:5: error:") != std::string::npos);
    assert(plain.find("    4 | ") != std::string::npos);
    assert(plain.find("      | ") != std::string::npos);
    assert(plain.find("^~~~") != std::string::npos);
    assert(plain.find("[-Wsafec-double-free]") != std::string::npos);
    assert(plain.find("note: previous deallocation here") != std::string::npos);
    assert(plain.find("compilation terminated due to memory safety violations.") != std::string::npos);
    std::cout << "  ✓ Plain text GCC gutter, caret underline, and note formatting verified.\n";

    // 2. ANSI Colorized format check
    analyzer.set_color_mode(safe_c::core::ColorChoice::Always);
    std::ostringstream color_oss;
    analyzer.print_diagnostics(color_oss);
    std::string colored = color_oss.str();

    assert(colored.find("\033[01;31merror:\033[0m") != std::string::npos && "Expected bold red error tag");
    assert(colored.find("\033[01;34m") != std::string::npos && "Expected bold blue gutter bar");
    assert(colored.find("\033[01;36mnote:\033[0m") != std::string::npos && "Expected bold cyan note tag");
    assert(colored.find("\033[01m'ptr'\033[0m") != std::string::npos && "Expected bold quoted identifier");
    std::cout << "  ✓ ANSI colorization (bold red, cyan, gutter blue, quoted symbols) verified.\n";

    // 3. Highlight toggle check (-fno-diagnostics-show-highlight-colors)
    analyzer.set_highlight_colors(false);
    std::ostringstream no_hl_oss;
    analyzer.print_diagnostics(no_hl_oss);
    std::string no_hl = no_hl_oss.str();
    assert(no_hl.find("\033[01;31merror:\033[0m") != std::string::npos);
    assert(no_hl.find("free(ptr);") != std::string::npos);
    std::cout << "  ✓ Diagnostic highlight toggling verified.\n";
}

void test_runtime_guardrails() {
    std::cout << "[TEST] Running Runtime Safety Net & Log Generation Suite...\n";
    const char* log_file = "test_safe_c_runtime.log";
    if (fs::exists(log_file)) {
        fs::remove(log_file);
    }

    safe_c_set_log_path(log_file);
    safe_c_set_mode(1); // WarningOnly mode for testing without abort()

    // 1. Test Allocation and Double Free Guardrail
    void* p = std::malloc(64);
    safe_c_record_alloc(p, 64, "runtime_test.c", 10);
    safe_c_record_free(p, "runtime_test.c", 12);
    // Trigger double free
    safe_c_record_free(p, "runtime_test.c", 14);

    // 2. Test Altered Pointer Deallocation Guardrail
    void* p2 = std::malloc(128);
    safe_c_record_alloc(p2, 128, "runtime_test.c", 20);
    char* interior_ptr = static_cast<char*>(p2) + 8;
    safe_c_record_free(interior_ptr, "runtime_test.c", 22);

    // 3. Test Leak Dump
    void* leaked = std::malloc(256);
    safe_c_record_alloc(leaked, 256, "runtime_test.c", 30);
    size_t leaks = safe_c_dump_unfreed();
    assert(leaks >= 1 && "Expected at least 1 unfreed leak");
    (void)leaks;

    // Verify log file was generated and populated
    assert(fs::exists(log_file) && "Log file must exist");
    std::string content;
    {
        std::ifstream in(log_file);
        content.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    assert(content.find("DOUBLE_FREE") != std::string::npos && "Log must mention DOUBLE_FREE");
    assert(content.find("MODIFIED_BASE_FREE") != std::string::npos && "Log must mention MODIFIED_BASE_FREE");
    assert(content.find("SAFE-C RUNTIME LEAK SUMMARY") != std::string::npos && "Log must have leak summary");

    std::cout << "  ✓ Runtime guardrails caught dynamic violations and generated detailed log file.\n";

    // Clean up test allocations
    std::free(p);
    std::free(p2);
    std::free(leaked);
    std::error_code ec;
    fs::remove(log_file, ec);
}

void test_v1_1_edge_cases() {
    using namespace safe_c;
    using namespace safe_c::core;
    std::cout << "[Test] v1.1.0 comprehensive edge cases (Core Analyzer / Windows Addon)...\n";
    Analyzer analyzer(Mode::FatalError);

    auto check = [](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "Assertion failed: " << msg << "\n";
            std::abort();
        }
    };

    // 1. Valid: free(NULL), realloc, scope escape, function return
    {
    auto check_pass = [&](const char* rel_path) {
        Analyzer pass_analyzer(Mode::FatalError);
        if (!pass_analyzer.analyze_file((fs::path(TEST_SOURCE_DIR) / rel_path).string())) {
            std::cerr << "Unexpected failure on " << rel_path << ":\n";
            pass_analyzer.print_diagnostics(std::cerr);
            std::abort();
        }
    };
        check_pass("tests/compile_pass/valid_edge_cases.c");
        check_pass("tests/compile_pass/pointer_passing.c");
        check_pass("tests/compile_pass/scope_changing.c");
        check_pass("tests/compile_pass/valid_vector_mutation.cpp");
    }

    // 2. Invalid: dangling stack return, OOB access, null deref, uninit deref, mismatched dealloc
    auto check_fail = [&](const char* rel_path, ViolationType expected) {
        Analyzer fail_analyzer(Mode::FatalError);
        bool ok = fail_analyzer.analyze_file((fs::path(TEST_SOURCE_DIR) / rel_path).string());
        check(!ok && !fail_analyzer.diagnostics().empty() && fail_analyzer.diagnostics()[0].type == expected, rel_path);
    };
    check_fail("tests/compile_fail/dangling_stack_return.c", ViolationType::DanglingStackReturn);
    check_fail("tests/compile_fail/out_of_bounds.c", ViolationType::OutOfBoundsAccess);
    check_fail("tests/compile_fail/null_deref.c", ViolationType::NullPointerDeref);
    check_fail("tests/compile_fail/uninit_deref.c", ViolationType::UninitializedPointer);
    check_fail("tests/compile_fail/mismatched_dealloc.cpp", ViolationType::MismatchedDealloc);

    std::cout << "  ✓ All v1.1.0 edge cases verified in Core Analyzer.\n";
}

int main() {
    std::cout << "===================================================\n";
    std::cout << "        Safe-C Comprehensive Verification Suite    \n";
    std::cout << "===================================================\n";

    test_compile_fail_cases();
    test_compile_pass_cases();
    test_pointer_offset_invariants();
    test_address_scope_resolution();
    test_lexical_block_scope_resolution();
    test_gnu_gcc_diagnostic_highlighting();
    test_runtime_guardrails();
    test_v1_1_edge_cases();

    std::cout << "===================================================\n";
    std::cout << "       ALL VERIFICATION TESTS PASSED (100% OK)     \n";
    std::cout << "===================================================\n";
    return 0;
}
