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

### Windows (MinGW-w64)
Copy `libwinpthread-1.dll` from `build/bin/addon/` into the GCC libexec directory:
```
<mingw-root>/libexec/gcc/x86_64-w64-mingw32/<version>/libwinpthread-1.dll
```
Subsequent invocations of `gcc` and `g++` run Safe-C static analysis automatically with zero wrapper scripts and zero custom compiler flags.

### Linux & macOS
Pass the plugin argument or place the spec file into your GCC specs directory:
```bash
gcc -fplugin=safe_c.so -c main.c -o main.o
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
