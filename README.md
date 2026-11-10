# MemSentinel: Compile-Time Memory Safety Plugin for GCC / G++

[![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-blue.svg)](LICENSE)
[![GCC Compatibility](https://img.shields.io/badge/GCC-%3E%3D%204.5.0-success.svg)](https://gcc.gnu.org/)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Documentation](https://img.shields.io/badge/Docs-GitHub%20Pages-brightgreen.svg)](https://kush-kelaiya22.github.io/MemSentinel/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20MinGW%20%7C%20Linux%20%7C%20macOS-brightgreen.svg)]()

> **MemSentinel** is a pure native compile-time memory safety plugin and runtime guardrail extension for GCC and G++. It enforces memory safety by **halting compilation directly at compile time** before vulnerable binaries can ever be produced.
>
> **Pure Native Plugin Architecture:** Zero wrapper scripts or environment hacks. Automatically loaded by standard GCC specs. Full support for unrestricted raw pointers, pointer arithmetic, `malloc`/`free`, and `new`/`delete`.

---

## Online Documentation & Portal

📖 Live interactive documentation, architecture guides, and platform setup instructions are available on the [MemSentinel Documentation Portal](https://kush-kelaiya22.github.io/MemSentinel/).

---

## Safety Guarantees Enforced

MemSentinel performs intra-procedural GIMPLE SSA typestate analysis directly inside the compiler:

- 🛑 **Double Free:** Deallocation (`free`, `delete`, `delete[]`, `realloc`) on an already freed address halts compilation with `error_at`.
- 🛑 **Use-After-Free (UAF):** Dereferencing (`*ptr`, `ptr->field`, `ptr[i]`) or reading freed pointers terminates compilation immediately.
- 🛑 **Altered Base Free Guard:** Moving pointer addresses via arithmetic (`p += 4; free(p);`) and attempting to free an interior offset is caught at compile-time with exact shifted byte diagnostics.
- 🛑 **Memory Leaks (Unfreed Allocations):** Unescaped heap allocations that reach scope exit without being deallocated or returned are flagged with fatal compiler errors.
- 🛡️ **Runtime Guardrails:** For dynamic or opaque external calls, `libmemsentinel_rt` catches violations and logs detailed forensic records into `memcheck_runtime.log`.

---

## Cross-Platform & Cross-Architecture Support

MemSentinel is designed with architecture-adaptive word sizing (`sizeof(uintptr_t)`) and platform abstraction:

| OS / Environment | Supported Architectures | Native Plugin File | Runtime Library |
| :--- | :--- | :--- | :--- |
| **Windows (MinGW-w64 / MSYS2)** | x86_64, i686, ARM64 | `memsentinel.dll` | `libmemsentinel_rt.dll` |
| **Linux (glibc / musl)** | x86_64, AArch64, RISC-V 64 | `memsentinel.so` | `libmemsentinel_rt.so` |
| **macOS (Homebrew GCC)** | Apple Silicon (M-series), x86_64 | `memsentinel.dylib` | `libmemsentinel_rt.dylib` |

---

## Quickstart

### 1. Build from Source
```bash
# Clone the repository
git clone https://github.com/Kush-Kelaiya22/MemSentinel.git
cd MemSentinel

# Configure and build with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run verification test suite
ctest --test-dir build --output-on-failure
```

### 2. Auto-Loading As Default in MinGW / GCC (Zero Wrappers)
Copy the plugin and specs file into your GCC directory:
```powershell
# Windows (MinGW-w64)
Copy-Item "build/bin/memsentinel.dll" "$((gcc -print-file-name=plugin).Trim())\"
Copy-Item "src/specs/memsentinel.specs" "$((gcc -print-file-name=specs).Trim())\specs"
```

Once installed, **every single invocation of `gcc` and `g++` automatically loads MemSentinel**!

---

## Disabling MemSentinel on Demand

If you need to bypass MemSentinel for legacy third-party code:
```bash
gcc -fplugin-arg-memsentinel-disable main.c -o main.exe
```

---

## Example: Detection at Compile Time

```c
#include <stdlib.h>

void invalid_offset_free(void) {
    char* buf = (char*)malloc(32);
    buf += 4;   // Base address modified!
    free(buf);  // Compilation fails here!
}
```

**Compiler Diagnostic:**
```text
example.c:6:5: error: [MEMSENTINEL] Attempt to free altered/interior pointer 'buf'.
    6 |     free(buf);
      |     ^~~~~~~~~
example.c:4:17: note: Original allocation base was created here (size: 32 bytes):
    4 |     char* buf = (char*)malloc(32);
example.c:5:5: note: Pointer address was shifted by +4 bytes prior to deallocation.
compilation terminated due to memory safety violations.
```

---

## Manual Steps Reference

See [docs/MANUAL_STEPS.md](docs/MANUAL_STEPS.md) for detailed instructions on plugin directory layout, MSYS2 headers, and specs integration.

---

## License

This project is licensed under the **GNU Affero General Public License v3.0 (AGPLv3)**. See [LICENSE](LICENSE) for details.
