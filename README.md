# Safe-C: Native Compile-Time Memory Safety Extension for GCC & G++

[![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-blue.svg)](LICENSE)
[![GCC Compatibility](https://img.shields.io/badge/GCC-%3E%3D%204.5.0-success.svg)](https://gcc.gnu.org/)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Documentation](https://img.shields.io/badge/Docs-GitHub%20Pages-brightgreen.svg)](https://kush-kelaiya22.github.io/Safe-C/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20MinGW%20%7C%20Linux%20%7C%20macOS-brightgreen.svg)]()

Safe-C is a compile-time memory safety plugin and toolchain addon for GCC and G++. It enforces memory safety by halting compilation directly at compile time before memory-unsafe binaries can be produced.

**Pure Native Toolchain Integration:** Operates strictly as a compiler plugin and companion toolchain addon—no executable checker binaries and no shell wrapper scripts. Standard raw pointers, pointer arithmetic, `malloc`/`free`, and `new`/`delete` are fully supported without language subsetting.

---

## Documentation

The authoritative single-page technical specification and reference manual is published on [GitHub Pages](https://kush-kelaiya22.github.io/Safe-C/).

---

## Safety Invariants Enforced

Safe-C verifies typestates and allocation bounds during compilation:

- **Double Free:** Deallocation (`free`, `delete`, `delete[]`, `realloc`) on an already freed pointer halts compilation immediately.
- **Use-After-Free:** Dereferencing (`*ptr`, `ptr->field`, `ptr[i]`) or reading freed pointers terminates compilation immediately with source locations of both the free site and access site.
- **Pointer Offset Preservation:** A pointer's offset can be modified through pointer arithmetic (`++`, `--`, `+=`, `-=`) as long as there is a variable that contains the original base address. Altering an offset when no variable retains the base address halts compilation to prevent permanent loss of the deallocation target.
- **Modified Base Deallocation:** Attempting to deallocate an interior or shifted pointer halts compilation with byte offset diagnostics.
- **Lexical & Block Scope Analysis:** Compound statement blocks (`{ ... }`) are analyzed with full lexical scope isolation. Unfreed allocations going out of scope without escaping to an outer variable trigger fatal compile-time memory leak errors at the closing brace.
- **Address Scope Resolution:** Path notations such as `.\x.c`, `./x.c`, `dir/../x.c`, and absolute paths resolve to identical canonical disk file identities, ensuring consistent diagnostic locus and translation unit isolation.
- **GNU/GCC Diagnostic Formatting & Highlighting:** Diagnostics are emitted with full GCC syntax and error highlighting, including gutter line numbers, vertical separator bars, source code snippets, caret underlines (`^~~~~`), bold red error tags, bold cyan notes, and bold quoted symbols.
- **Diagnostic Deduplication:** Each diagnostic line is emitted once per error type to ensure clear, uncluttered compiler diagnostics.
- **Runtime Guardrails:** For dynamic or opaque external library boundaries, `libsafe_c_rt` records allocation metadata and logs violations to `safe_c_runtime.log`.

---

## Toolchain Setup

Safe-C provides native integration for both Windows (MinGW-w64) and Linux/macOS (GCC):

### Enable by Default (Zero-Flag Enforcement)
Safe-C can be enabled by default across your entire toolchain, so that all standard `gcc` and `g++` invocations enforce memory safety without passing any extra flags:
```cmd
# Enable Safe-C by default for all host GCC/G++ compilers:
safe_c --enable-default

# Or enable via CMake:
cmake --build build --target enable-default

# Check current activation status:
safe_c --status

# Disable default auto-loading:
safe_c --disable-default
```

Once enabled, simply running standard commands like:
```cmd
gcc -c main.c -o main.o
g++ -c app.cpp -o app.o
```
will automatically inspect memory safety and halt on violations.

### Manual Toolchain Setup
If you prefer not to enable Safe-C globally, you can invoke it per build:

1. **Universal Specs Flag (Works across ALL MinGW-w64 toolchains):**
   ```cmd
   gcc -specs=build/specs/safe_c.specs -c main.c -o main.o
   g++ -specs=build/specs/safe_c.specs -c app.cpp -o app.o
   ```
2. **Direct Compiler Plugin Invocation (for plugin-enabled toolchains):**
   ```cmd
   gcc -fplugin=build/bin/safe_c.dll -c main.c -o main.o
   ```
3. **Direct Safe-C Inspection:**
   ```cmd
   safe_c main.c
   ```

### Linux & macOS
```bash
# Enable by default for host GCC:
./build/bin/safe_c --enable-default

# Or load directly per compilation:
gcc -fplugin=build/lib/safe_c.so -c main.c -o main.o
```

---

## Self-Compilation & Dogfooding

Safe-C is written in modern C++20 and C11 and dogfoods its own memory safety guarantees:
- **Zero Invariant Violations:** All data structures, token caches, and state tables in Safe-C obey its own memory safety contracts (zero leaks, zero use-after-free, zero double free).
- **Self-Hosting Compilation:** Safe-C compiles cleanly when analyzed by its own plugin (`-fplugin=safe_c`), demonstrating self-hosting verification without false positives.

```bash
# Clone and compile Safe-C from source
git clone https://github.com/Kush-Kelaiya22/Safe-C.git
cd Safe-C

cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## Binary Migration & Project Adoption

Safe-C is designed for incremental, non-breaking adoption in existing C and C++ codebases:

1. **Zero Code Refactoring:** No conversion of raw pointers to smart pointers is required. Standard `T*` declarations, pointer arithmetic (`ptr++`, `ptr += n`), `malloc`/`free`, and `new`/`delete` remain fully supported.
2. **Audit Phase (`-fplugin-arg-safe_c-mode=warn`):** Run Safe-C in warning mode across legacy codebases to catalogue potential leaks and hazards without breaking CI builds.
3. **Enforcement Phase (Default):** Halt compilation on pull requests whenever memory safety violations are introduced.
4. **Selective Exemption:** Exempt third-party vendor directories or legacy modules via `-DSAFE_C_DISABLE` or `export SAFE_C_DISABLE=1`.
5. **Runtime Transition Defense:** Link `libsafe_c_rt` to track allocations at dynamic binary boundaries and capture forensic logs in `safe_c_runtime.log`.

---

## Testing Details & Test Harness

Safe-C includes an automated verification test suite:

- **Compile-Fail Static Analysis Suite:** Verifies that Double Free, Use-After-Free, Altered Base Deallocation, and Memory Leaks halt compilation with fatal errors.
- **Compile-Pass Regression Suite:** Verifies that idiomatic raw pointers, cursor arithmetic, and C++ RAII lifetimes compile cleanly with zero false positives.
- **Pointer Offset & Base Invariant Suite:** Validates that cursor advancement is allowed while a base variable exists, and rejected when the base is lost.
- **Address Scope Resolution Suite:** Verifies that `.\x.c`, `./x.c`, and `x.c` resolve to identical physical file identities and deduplicate diagnostic lines.
- **Lexical Block Scoping Suite:** Validates that compound statement blocks (`{ ... }`) properly isolate variable lifetimes and catch unescaped leaks.
- **GNU/GCC Diagnostic Highlighting Suite:** Tests line number gutters, token carets, underline tildes, ANSI syntax colorization, and highlight toggling.
- **Runtime Guardrail Suite:** Validates runtime heap tracking, forensic logging, and memory leak summary generation.

To run the verification test suite:
```bash
ctest --test-dir build --output-on-failure
# Or execute directly:
./build/bin/safe_c_tests
```

---

## Disabling On Demand

Safe-C static analysis can be disabled per invocation or per environment:

```bash
# Via compiler macro
gcc -DSAFE_C_DISABLE -c legacy.c -o legacy.o

# Via environment variable
export SAFE_C_DISABLE=1
gcc -c legacy.c -o legacy.o
```

---

## Building from Source

```bash
git clone https://github.com/Kush-Kelaiya22/Safe-C.git
cd Safe-C

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

ctest --test-dir build --output-on-failure
```

---

## License

Safe-C is licensed under the **GNU Affero General Public License v3.0 (AGPLv3)**. See [LICENSE](LICENSE) for details.
