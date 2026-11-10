#include "analyzer.h"
#include "memcheck/api.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>

#ifndef TEST_SOURCE_DIR
#define TEST_SOURCE_DIR "."
#endif

namespace fs = std::filesystem;

static std::string test_path(const std::string& rel) {
    return (fs::path(TEST_SOURCE_DIR) / rel).string();
}

void test_compile_fail_cases() {
    std::cout << "[TEST] Running Compile-Fail Static Analysis Suite...\n";
    memcheck::core::Analyzer analyzer(memcheck::Mode::FatalError);

    // 1. Double Free
    analyzer.clear();
    bool pass1 = analyzer.analyze_file(test_path("tests/compile_fail/double_free.c"));
    assert(!pass1 && "double_free.c must fail compilation");
    bool found_df = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == memcheck::ViolationType::DoubleFree) found_df = true;
    }
    assert(found_df && "Expected DoubleFree diagnostic");
    std::cout << "  ✓ double_free.c correctly halted compilation.\n";

    // 2. Use After Free
    analyzer.clear();
    bool pass2 = analyzer.analyze_file(test_path("tests/compile_fail/use_after_free.cpp"));
    assert(!pass2 && "use_after_free.cpp must fail compilation");
    bool found_uaf = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == memcheck::ViolationType::UseAfterFree) found_uaf = true;
    }
    assert(found_uaf && "Expected UseAfterFree diagnostic");
    std::cout << "  ✓ use_after_free.cpp correctly halted compilation.\n";

    // 3. Altered Pointer Free
    analyzer.clear();
    bool pass3 = analyzer.analyze_file(test_path("tests/compile_fail/altered_pointer_free.c"));
    assert(!pass3 && "altered_pointer_free.c must fail compilation");
    bool found_mbf = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == memcheck::ViolationType::ModifiedBaseFree) found_mbf = true;
    }
    assert(found_mbf && "Expected ModifiedBaseFree diagnostic");
    std::cout << "  ✓ altered_pointer_free.c correctly halted compilation.\n";

    // 4. Memory Leak
    analyzer.clear();
    bool pass4 = analyzer.analyze_file(test_path("tests/compile_fail/memory_leak.c"));
    assert(!pass4 && "memory_leak.c must fail compilation");
    bool found_leak = false;
    for (const auto& d : analyzer.diagnostics()) {
        if (d.type == memcheck::ViolationType::MemoryLeakUnfreed) found_leak = true;
    }
    assert(found_leak && "Expected MemoryLeakUnfreed diagnostic");
    std::cout << "  ✓ memory_leak.c correctly halted compilation.\n";
}

void test_compile_pass_cases() {
    std::cout << "[TEST] Running Compile-Pass Verification Suite...\n";
    memcheck::core::Analyzer analyzer(memcheck::Mode::FatalError);

    // 1. Valid Pointers
    analyzer.clear();
    bool pass1 = analyzer.analyze_file(test_path("tests/compile_pass/valid_pointers.c"));
    assert(pass1 && "valid_pointers.c must compile cleanly");
    assert(analyzer.diagnostics().empty() && "No diagnostics expected for valid code");
    std::cout << "  ✓ valid_pointers.c compiled cleanly.\n";

    // 2. Valid Arithmetic
    analyzer.clear();
    bool pass2 = analyzer.analyze_file(test_path("tests/compile_pass/valid_arithmetic.c"));
    assert(pass2 && "valid_arithmetic.c must compile cleanly");
    std::cout << "  ✓ valid_arithmetic.c compiled cleanly.\n";

    // 3. Valid C++ RAII
    analyzer.clear();
    bool pass3 = analyzer.analyze_file(test_path("tests/compile_pass/valid_cpp_raii.cpp"));
    assert(pass3 && "valid_cpp_raii.cpp must compile cleanly");
    std::cout << "  ✓ valid_cpp_raii.cpp compiled cleanly.\n";
}

void test_runtime_guardrails() {
    std::cout << "[TEST] Running Runtime Safety Net & Log Generation Suite...\n";
    const char* log_file = "test_memcheck_runtime.log";
    if (fs::exists(log_file)) {
        fs::remove(log_file);
    }

    memcheck_set_log_path(log_file);
    memcheck_set_mode(1); // WarningOnly mode for testing without abort()

    // 1. Test Allocation and Double Free Guardrail
    void* p = std::malloc(64);
    memcheck_record_alloc(p, 64, "runtime_test.c", 10);
    memcheck_record_free(p, "runtime_test.c", 12);
    // Trigger double free
    memcheck_record_free(p, "runtime_test.c", 14);

    // 2. Test Altered Pointer Deallocation Guardrail
    void* p2 = std::malloc(128);
    memcheck_record_alloc(p2, 128, "runtime_test.c", 20);
    char* interior_ptr = static_cast<char*>(p2) + 8;
    memcheck_record_free(interior_ptr, "runtime_test.c", 22);

    // 3. Test Leak Dump
    void* leaked = std::malloc(256);
    memcheck_record_alloc(leaked, 256, "runtime_test.c", 30);
    size_t leaks = memcheck_dump_unfreed();
    assert(leaks >= 1 && "Expected at least 1 unfreed leak");

    // Verify log file was generated and populated
    assert(fs::exists(log_file) && "Log file must exist");
    std::string content;
    {
        std::ifstream in(log_file);
        content.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    assert(content.find("DOUBLE_FREE") != std::string::npos && "Log must mention DOUBLE_FREE");
    assert(content.find("MODIFIED_BASE_FREE") != std::string::npos && "Log must mention MODIFIED_BASE_FREE");
    assert(content.find("MEMCHECK RUNTIME LEAK SUMMARY") != std::string::npos && "Log must have leak summary");

    std::cout << "  ✓ Runtime guardrails caught dynamic violations and generated detailed log file.\n";

    // Clean up test allocations
    std::free(p);
    std::free(p2);
    std::free(leaked);
    std::error_code ec;
    fs::remove(log_file, ec);
}

int main() {
    std::cout << "===================================================\n";
    std::cout << "       MemCheck Comprehensive Verification Suite    \n";
    std::cout << "===================================================\n";

    test_compile_fail_cases();
    test_compile_pass_cases();
    test_runtime_guardrails();

    std::cout << "===================================================\n";
    std::cout << "       ALL VERIFICATION TESTS PASSED (100% OK)     \n";
    std::cout << "===================================================\n";
    return 0;
}
