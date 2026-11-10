# Safe_C++: Native Compile-Time Memory Safety Plugin for GCC / G++

[![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-blue.svg)](LICENSE)
[![GCC Compatibility](https://img.shields.io/badge/GCC-%3E%3D%204.5.0-success.svg)](https://gcc.gnu.org/)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Documentation](https://img.shields.io/badge/Docs-GitHub%20Pages-brightgreen.svg)](https://kush-kelaiya22.github.io/Safe_C-/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20MinGW%20%7C%20Linux%20%7C%20macOS-brightgreen.svg)]()

> **Safe_C++** is a native compile-time memory safety plugin and runtime guardrail extension for GCC and G++. It enforces memory safety by **halting compilation directly at compile time** before vulnerable binaries can ever be produced.
>
> **Pure Native Plugin Architecture:** Zero wrapper scripts and zero memory-checking executables. Operates strictly as a compiler plugin and native toolchain addon. Unrestricted raw pointers, pointer arithmetic, `malloc`/`free`, and `new`/`delete` are fully supported without any language subsetting.

---

## Online Documentation

Comprehensive single-page technical documentation and specifications are available at the [Safe_C++ Documentation Portal](https://kush-kelaiya22.github.io/Safe_C-/).

---

## Safety Invariants Enforced

Safe_C++ models pointer typestates directly during compiler execution:

- **Double Free:** Deallocation (`free`, `delete`, `delete[]`, `realloc`) on an already freed address halts compilation with fatal diagnostic errors.
- **Use-After-Free (UAF):** Dereferencing (`*ptr`, `ptr->field`, `ptr[i]`) or reading freed pointers terminates compilation immediately.
- **Altered Base Deallocation:** Shifting pointer addresses via arithmetic (`p += 4; free(p);`) and attempting to free an interior offset is caught at compile time with exact byte offset diagnostics.
- **Memory Leaks:** Unescaped dynamic heap allocations reaching scope exit without deallocation halt compilation with fatal errors.
- **Runtime Guardrails:** For dynamic or opaque external calls, `libsafe_cpp_rt` logs detailed diagnostic records into `safe_cpp_runtime.log`.

---

## Toolchain Integration

### Windows (MinGW-w64)
Place `libwinpthread-1.dll` in your GCC libexec directory (`libexec/gcc/<target>/<version>/`).
All subsequent invocations of `gcc` and `g++` automatically execute Safe_C++ static checks without any wrappers or manual flags!

### Linux & macOS
Pass `-fplugin=safe_cpp.so` or configure default GCC specs.

---

## Disabling On Demand

For compiling trusted legacy third-party source files:
```bash
# Via compiler flag
gcc -DSAFE_CPP_DISABLE -c legacy.c -o legacy.o

# Or via environment variable
export SAFE_CPP_DISABLE=1
gcc -c legacy.c -o legacy.o
```

---

## Building from Source

```bash
git clone https://github.com/Kush-Kelaiya22/Safe_C-.git
cd Safe_C-

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

ctest --test-dir build --output-on-failure
```

---

## License

Safe_C++ is licensed under the **GNU Affero General Public License v3.0 (AGPLv3)**. See [LICENSE](LICENSE) for details.
