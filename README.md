# MemCheck: Compile-Time Memory Safety Checker for GCC / G++

[![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-blue.svg)](LICENSE)
[![GCC Compatibility](https://img.shields.io/badge/GCC-%3E%3D%204.5.0-success.svg)](https://gcc.gnu.org/)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/Platform-Windows%20MinGW%20%7C%20Linux-brightgreen.svg)]()

> **MemCheck** is a pure compile-time memory safety checker and native GCC/G++ plugin equipped with runtime guardrails. It prevents memory unsafety bugs in standard C and C++ by **halting compilation directly at compile time** before vulnerable binaries can ever be produced.
>
> **Pure Native Plugin:** Zero wrapper scripts or environment hacks. Works automatically with existing GCC toolchains as soon as it is added to their path or specs.

---

## Key Highlights

- **Unrestricted C/C++ Syntax:** Write standard idiomatic C and C++. Raw pointers, pointer arithmetic, `malloc`/`free`, and `new`/`delete` are completely permitted.
- **Strict Compile-Time Rejection:** If memory unsafety is detected by static typestate analysis, the compiler halts immediately with a fatal diagnostic error (`error_at`).
- **Comprehensive Bug Detection:**
  - 🛑 **Double Free:** Calling `free` or `delete` on an already deallocated pointer.
  - 🛑 **Use-After-Free (UAF):** Reading, writing, or dereferencing pointers after deallocation.
  - 🛑 **Memory Leaks (Not Freed):** Forgetting to deallocate dynamic heap blocks before scope exit or loss of aliases.
  - 🛑 **Altered Address Before Free:** Performing pointer arithmetic (e.g. `p++; free(p);`) and attempting to deallocate an interior offset instead of the original base.
  - 🛑 **Unallocated / Wild Free:** Deallocating stack memory or arbitrary addresses.
- **Runtime Guardrails:** A secondary lightweight runtime safety net (`libmemcheck_rt`) catches non-deterministic dynamic bugs and logs comprehensive diagnostics to `memcheck_runtime.log`.
- **Compiler Compatibility:** Built for any GCC-supporting environment from **GCC 4.5.0** (the initial GCC release supporting plugins) to modern GCC 13, 14, and 15+.
- **MinGW & Cross-Platform:** Engineered for Windows MinGW-w64 and Linux.
- **Fully Controllable:** Can be completely disabled at any time via `-fplugin-arg-memcheck-disable` or `--memcheck=off`.
- **100% Memory-Safe Checker Codebase:** The checker itself is implemented in modern safe C++20 using strict RAII, smart pointers, bounds-checked views (`std::span`), and zero manual raw pointer ownership.

---

## Architecture Overview

```
                         C/C++ Source Code
                                 │
                                 ▼
      ┌──────────────────────────────────────────────────────┐
      │ GCC Frontend / Plugin GIMPLE Typestate Pass          │
      │ • Reconstructs pointer aliases & ownership chains    │
      │ • Evaluates Control Flow Graph (CFG) basic blocks    │
      │ • Detects: Double Free, UAF, Leaks, Offset Frees     │
      └──────────────────────────┬───────────────────────────┘
                                 │
                   Memory Safe at Compile Time?
                      /                     \
                    NO                       YES
                    │                         │
                    ▼                         ▼
        COMPILATION TERMINATED!        Binary Generated
        (Detailed error_at note)       (Optional: Runtime Guardrails)
                                              │
                                              ▼
                             ┌─────────────────────────────────┐
                             │ libmemcheck_rt (Runtime Net)    │
                             │ • Allocation table & redzones   │
                             │ • Writes memcheck_runtime.log   │
                             └─────────────────────────────────┘
```

---

## Quickstart

### 1. Building MemCheck
```bash
# Clone the repository
git clone https://github.com/Kush-Kelaiya22/memcheck.git
cd memcheck

# Configure and build with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### 2. Compiling with MemCheck (MinGW / Windows)
```powershell
# Using the driver wrapper
./build/bin/gcc-memcheck.exe -O2 example.c -o example.exe
```

### 3. Using as a GCC Plugin (Linux / MinGW with plugin headers)
```bash
gcc -fplugin=build/lib/memcheck_plugin.so -c example.c
```

---

## Detection In Action

### Example 1: Double Free (Compile-Time Rejection)
```c
#include <stdlib.h>

void bad_function() {
    int* ptr = (int*)malloc(sizeof(int) * 10);
    free(ptr);
    free(ptr); // ERROR: Double free!
}
```
**Compiler Output:**
```text
bad_function.c:7:5: error: [MEMCHECK] Double free detected on pointer 'ptr'.
    7 |     free(ptr);
      |     ^~~~~~~~~
bad_function.c:6:5: note: Previous deallocation occurred here:
    6 |     free(ptr);
      |     ^~~~~~~~~
compilation terminated due to memory safety violation.
```

### Example 2: Altered Address Before Free
```c
#include <stdlib.h>

void invalid_offset_free() {
    char* buf = (char*)malloc(32);
    buf += 4;   // Base address modified!
    free(buf);  // ERROR: Attempting to free interior pointer!
}
```
**Compiler Output:**
```text
test.c:6:5: error: [MEMCHECK] Attempt to free altered/interior pointer 'buf'.
    6 |     free(buf);
      |     ^~~~~~~~~
test.c:4:17: note: Allocation base originates here with size 32 bytes:
    4 |     char* buf = (char*)malloc(32);
test.c:5:5: note: Pointer address was shifted by +4 bytes prior to deallocation.
```

### Example 3: Use After Free (UAF)
```cpp
void uaf_example() {
    int* data = new int(42);
    delete data;
    *data = 100; // ERROR: Dereferencing freed pointer!
}
```
**Compiler Output:**
```text
uaf.cpp:4:5: error: [MEMCHECK] Use-After-Free: Pointer 'data' was dereferenced after being deleted.
    4 |     *data = 100;
      |     ^~~~~
uaf.cpp:3:5: note: Pointer was deleted here.
```

---

## Disabling the Plugin

To disable checking on demand:
- **CLI Flag:** `-fplugin-arg-memcheck-disable`
- **Driver Flag:** `--memcheck=off`
- **Environment Variable:** `export MEMCHECK_DISABLE=1` (or `$env:MEMCHECK_DISABLE="1"`)

---

## Runtime Diagnostic Log Format (`memcheck_runtime.log`)

When runtime guardrails catch dynamic non-deterministic safety issues, detailed logs are saved:

```text
======================================================================
MEMCHECK RUNTIME SAFETY VIOLATION REPORT
Timestamp    : 2026-11-11 00:25:32
Violation    : DOUBLE_FREE
Target Addr  : 0x000001FA291B4C80
Original Base: 0x000001FA291B4C80 (Allocated at test.c:12, size 128 bytes)
Previous Free: test.c:18
Call Stack   :
  [0] memcheck_rt!memcheck_free_hook + 0x3f
  [1] app.exe!cleanup_buffers (test.c:22)
  [2] app.exe!main (test.c:45)
Action Taken : Execution halted safely before heap corruption could occur.
======================================================================
```

---

## Manual Steps Checklist

For manual integration steps, MinGW headers installation, and specs setup, see:
📖 [docs/MANUAL_STEPS.md](docs/MANUAL_STEPS.md)

---

## License

This project is licensed under the **GNU Affero General Public License v3.0 (AGPLv3)**. See [LICENSE](LICENSE) for details. GCC plugin interfaces comply with GCC runtime licensing requirements (`plugin_is_GPL_compatible`).
