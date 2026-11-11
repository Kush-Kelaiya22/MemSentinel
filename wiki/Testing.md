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

## Continuous Integration (CI)

Every commit and pull request runs automated CI pipelines on:
- **Ubuntu Latest:** Builds with GCC-13 plugin headers and verifies native plugin execution (`gcc-13 -fplugin=safe_c.so`).
- **Windows Latest:** Builds with MinGW-w64 toolchain and verifies native static analysis and runtime guardrails.
