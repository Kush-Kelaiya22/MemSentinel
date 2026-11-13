# Testing & Verification Suite

Safe-C includes an automated test harness covering static compile-fail suites, compile-pass regressions, pointer arithmetic invariants, lexical scoping, address scope resolution, diagnostic formatting, and runtime guardrails.

---

## Suite Organization

| Test Category | Function | Objective |
| :--- | :--- | :--- |
| **Compile-Fail Suite** | `test_compile_fail_cases()` | Verifies that memory hazards (`double_free.c`, `use_after_free.cpp`, `altered_pointer_free.c`, `memory_leak.c`) halt compilation with non-zero exit codes. |
| **Compile-Pass Suite** | `test_compile_pass_cases()` | Verifies that valid pointer idioms (`valid_pointers.c`, `valid_arithmetic.c`, `valid_cpp_raii.cpp`) compile cleanly with zero false positives. |
| **Pointer Offset Invariants** | `test_pointer_offset_invariants()` | Verifies that cursor advancement is permitted when a base variable exists, but halted when no variable retains the base address. |
| **Address Scope Resolution** | `test_address_scope_resolution()` | Verifies canonicalization of `.\x.c`, `./x.c`, and `x.c` to the identical physical disk identity and ensures single-line diagnostic deduplication. |
| **Lexical Block Scoping** | `test_lexical_block_scope_resolution()` | Verifies compound statement blocks (`{ ... }`), ensuring unescaped inner allocations leak at `}`, while escaped variables compile cleanly. |
| **GNU/GCC Diagnostic Formatting** | `test_gnu_gcc_diagnostic_highlighting()` | Verifies line number gutters, token carets, underline tildes, ANSI colorization, and `-fno-diagnostics-show-highlight-colors` toggle. |
| **Runtime Guardrail Suite** | `test_runtime_guardrails()` | Verifies `libsafe_c_rt` dynamic heap tracking and `safe_c_runtime.log` emission. |
| **Edge Cases & STL Invalidation** | `test_v1_1_edge_cases()` | Verifies OOB/null/uninit deref, dangling stack returns, mismatched deallocs, and STL container iterator/`.data()`/`.c_str()` invalidation. |
| **Legacy C89 & C++98 Suite** | `test_legacy_standards()` + CTest `legacy_c89_compile` / `legacy_cpp98_compile` | Verifies strict `-std=c89 -pedantic -Wall -Wextra -Werror` (K&R/ANSI + `safe_c/api.h`) and `-std=c++98 -pedantic -Wall -Wextra -Werror` (`new int(42)`, `new Box98[2]`, `delete`/`delete[]`). |

---

## Running the Test Suite Locally

```bash
# Build the test executable
cmake --build build --config Release

# Run via CTest
ctest --test-dir build --output-on-failure

# Or execute directly for detailed test logs
./build/bin/safe_c_tests
```

### Expected Output
```
===================================================
        Safe-C Comprehensive Verification Suite    
===================================================
[TEST] Running Compile-Fail Static Analysis Suite...
  ✓ double_free.c correctly halted compilation.
  ✓ use_after_free.cpp correctly halted compilation.
  ✓ altered_pointer_free.c correctly halted compilation.
  ✓ memory_leak.c correctly halted compilation.
[TEST] Running Compile-Pass Verification Suite...
  ✓ valid_pointers.c compiled cleanly.
  ✓ valid_arithmetic.c compiled cleanly.
  ✓ valid_cpp_raii.cpp compiled cleanly.
[TEST] Running Pointer Offset & Base Invariant Suite...
  ✓ Changing cursor offset while base exists verified cleanly.
  ✓ Shifting base pointer when backup retains original address verified cleanly.
  ✓ Offset alteration without base variable correctly rejected.
[TEST] Running Address Scope Resolution & Deduplication Suite...
  ✓ Address scope resolution verified: .\x and x resolve to exact same path.
  ✓ Deduplication verified: exactly 1 diagnostic per line for a given error type.
[TEST] Running GCC-Style Lexical Block Scope Suite...
  ✓ Lexical block exit correctly caught unescaped block leak.
  ✓ Scope escaping to outer variable correctly recognized and verified.
[TEST] Running GNU/GCC Error Highlighting & Gutter Suite...
  ✓ Plain text GCC gutter, caret underline, and note formatting verified.
  ✓ ANSI colorization (bold red, cyan, gutter blue, quoted symbols) verified.
  ✓ Diagnostic highlight toggling verified.
[TEST] Running Runtime Safety Net & Log Generation Suite...
  ✓ Runtime guardrails caught dynamic violations and generated detailed log file.
===================================================
       ALL VERIFICATION TESTS PASSED (100% OK)     
===================================================
```

---

## Compilation Time Benchmarks

You can benchmark stock `gcc`/`g++` against Safe-C using `tools/benchmark_compile.py` (default: warmup + 7 runs; example below uses 1 warmup + 5 measured runs, median reported).

Measured with `g++ (GCC) 16.2.1 20260810` on Intel Core i5-10210U (8 logical cores, Linux 7.2.11 x86\_64):

| Scenario | Stock (ms) | With Safe-C (ms) | Delta |
| :--- | ---: | ---: | ---: |
| C small (`valid_edge_cases.c`, `-O0`) | 25.2 | 22.2 | -3.0 ms (-11.8%) |
| C 120-func heap/loop (`-O0`) | 190.3 | 214.4 | +24.1 ms (+12.6%) |
| C 120-func heap/loop (`-O2`) | 1375.4 | 1375.7 | +0.3 ms (+0.0%) |
| C++ STL patterns (`-O0 -std=c++20`) | 371.7 | 378.0 | +6.2 ms (+1.7%) |
| C++ STL patterns (`-O2 -std=c++20`) | 400.3 | 387.8 | -12.5 ms (-3.1%) |
| C++ multi-container STL (`-O2 -std=c++20`) | 1501.8 | 1539.7 | +37.9 ms (+2.5%) |
| C++20 real TU `analyzer.cpp` (`-O0`) | 1734.0 | 1752.2 | +18.2 ms (+1.0%) |
| C++20 real TU `analyzer.cpp` (`-O2`) | 3892.9 | 3907.3 | +14.4 ms (+0.4%) |

```bash
python3 tools/benchmark_compile.py --runs 5 --warmup 1
```

---

## Continuous Integration (CI)

Every commit and pull request runs automated CI pipelines on:
- **Ubuntu Latest:** Builds with GCC-13 plugin headers and verifies native plugin execution (`gcc-13 -fplugin=safe_c.so`).
- **Windows Latest:** Builds with MinGW-w64 toolchain and verifies native static analysis and runtime guardrails.
