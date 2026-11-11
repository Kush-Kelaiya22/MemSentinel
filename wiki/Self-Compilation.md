# Self-Compilation & Dogfooding

Safe-C is built using modern C++20 and C11 and is fully capable of compiling itself while adhering to its own memory safety contracts.

## Zero-Violation Architecture

Safe-C's internal codebase is designed to be free from memory hazards:
- **Lifetime Discipline:** RAII containers (`std::vector`, `std::unique_ptr`, `std::shared_ptr`, `std::string`) govern allocations deterministically.
- **Typestate Integrity:** Dynamic memory cells in `StateTable` and `AllocationCell` follow strict state transitions with zero use-after-free and zero double frees.
- **Leak Elimination:** All AST token representations and source line caches are destroyed upon translation unit completion.
- **Dogfooding:** Safe-C can be built with its own plugin enabled (`-fplugin=safe_c`), verifying that the codebase compiles cleanly with zero diagnostic violations.

## Prerequisites

- **GCC / G++:** Version 4.5.0 or newer (tested with GCC 13, 14, and 15).
- **CMake:** Version 3.16 or newer.
- **Build Generator:** Ninja or GNU Make.

## Compilation Commands

### 1. Configure Build Tree
```bash
git clone https://github.com/Kush-Kelaiya22/Safe-C.git
cd Safe-C

# Configure using CMake with Release optimizations
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
```

### 2. Compile Artifacts
```bash
cmake --build build --config Release
```

The build produces:
- `bin/safe_c.dll` (Windows) or `lib/safe_c.so` (Linux): The native Safe-C compiler plugin/addon.
- `lib/libsafe_c_core.a`: The static static-analysis core engine.
- `lib/libsafe_c_rt.a` / `bin/libsafe_c_rt.dll`: The runtime guardrails safety net.
- `bin/safe_c_tests.exe` (or `safe_c_tests`): Comprehensive verification suite.

### 3. Self-Test
Run the built-in test suite:
```bash
ctest --test-dir build --output-on-failure
```
