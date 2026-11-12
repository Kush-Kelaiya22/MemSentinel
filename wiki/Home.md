# Safe-C: Native Compile-Time Memory Safety Extension

Welcome to the **Safe-C** official documentation and technical wiki.

Safe-C is a pure compile-time memory safety plugin and toolchain companion addon for GNU GCC and G++. It statically tracks heap allocations, pointer aliases, byte offsets, and typestates—halting compilation directly before memory-unsafe binaries can be produced.

---

## Key Pages

- **[Memory Safety Invariants](Memory-Safety-Invariants)**: Full matrix of caught violations (leaks, UAF, OOB, null/uninit deref, dangling return, mismatched dealloc, STL iterator invalidation) and zero-false-positive valid patterns (v1.2.2).
- **[Address Scope Resolution](Address-Scope-Resolution)**: How Safe-C canonicalizes file paths (`.\x.c` vs `x.c`) and isolates translation units.
- **[Pointer Offsets & Invariants](Pointer-Offsets-and-Invariants)**: Rules governing pointer arithmetic and base address preservation.
- **[Lexical Scope Resolution](Lexical-Scoping)**: Block-level scoping (`{ ... }`), variable lifetime, and scope-exit leak detection.
- **[Toolchain Integration](Toolchain-Integration)**: Native GCC plugin and addon (`safe_c.dll` on Windows, `safe_c.so` on Linux).
- **[GNU/GCC Diagnostic Highlighting](GNU-GCC-Diagnostic-Highlighting)**: Gutter line numbers, token caret underlines, and terminal ANSI highlighting.
- **[Self-Compilation](Self-Compilation)**: Self-hosting clean compilation and dogfooding memory invariants.
- **[Binary Migration](Binary-Migration)**: Step-by-step adoption guide for existing legacy codebases with zero refactoring.
- **[Testing Suite](Testing)**: Exhaustive verification test harness and CI matrix details.

---

## Core Principles

1. **Unrestricted Idiomatic C/C++:** Raw pointers, pointer arithmetic, `malloc`/`free`, and `new`/`delete` are fully supported without language subsetting or forced wrapper classes.
2. **Zero Standalone Checker Binaries:** Safe-C is purely an extension of the compiler toolchain—it is never an external executable scanner.
3. **Zero Shell Wrapper Scripts:** Invocations use native `gcc` and `g++` commands directly.
4. **Deterministic Halting:** On safety violations, compilation halts with an authentic GCC diagnostic snippet and a non-zero exit code (`1`).
