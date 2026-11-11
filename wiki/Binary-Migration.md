# Binary Migration & Project Adoption

Safe-C is engineered as a non-breaking, drop-in compile-time memory safety extension. It requires zero source code refactoring and supports standard C and C++ idioms without language subsetting.

---

## 1. Zero Refactoring Mandate

Traditional memory safety tools often force developers to:
- Abandon raw pointers and pointer arithmetic.
- Rewrite procedural code into smart-pointer object graphs.
- Adopt proprietary language dialects or custom wrapper macros.

Safe-C eliminates this friction:
- **Raw Pointers Supported:** Standard `T*` declarations compile without changes.
- **Pointer Arithmetic Preserved:** Expressions like `ptr++`, `ptr += offset`, and `ptr[i]` are permitted as long as a base address variable is retained in scope.
- **Allocation Functions Supported:** Standard `malloc`, `calloc`, `realloc`, `free`, `new`, and `delete` are tracked natively.

---

## 2. Migration Phases

### Phase 1: Audit & Discovery (`Mode::WarningOnly`)
To assess memory hazards across a large existing codebase without breaking production builds:

```bash
# Via GCC plugin argument
gcc -fplugin=safe_c -fplugin-arg-safe_c-mode=warn -c legacy_app.c -o legacy_app.o
```

In warning mode, Safe-C emits diagnostics to `stderr` with full source snippets, gutters, and caret underlines, but exits with code `0`. Review the audit logs to catalog:
- Scope-exit memory leaks
- Interior pointer deallocations
- Potential double frees or use-after-free paths

### Phase 2: Pull-Request Enforcement (`Mode::FatalError`)
Once critical violations are resolved, switch Safe-C to fatal enforcement mode:

```bash
# Fatal error mode (default)
gcc -fplugin=safe_c -c app.c -o app.o
```

Any memory safety violation immediately terminates compilation with exit code `1`, preventing unsafe binaries from being emitted.

### Phase 3: Selective Exemption for Legacy Submodules
Third-party libraries or vendor directories that cannot be modified can be exempted from Safe-C analysis:

```bash
# Via compiler flag
gcc -DSAFE_C_DISABLE -c vendor/sqlite3.c -o vendor/sqlite3.o

# Via environment variable for batch scripts
export SAFE_C_DISABLE=1
make -C legacy_submodule
unset SAFE_C_DISABLE
```

---

## 3. Build System Integration

### CMake Projects
Add Safe-C as a global or target-specific compile option:
```cmake
find_library(SAFE_C_LIB NAMES safe_c)
if(SAFE_C_LIB)
    add_compile_options(-fplugin=${SAFE_C_LIB})
endif()
```

### GNU Make Projects
```makefile
CFLAGS += -fplugin=/usr/local/lib/safe_c.so
CXXFLAGS += -fplugin=/usr/local/lib/safe_c.so
```

### Auto-Loading Specs File
Place `safe_c.specs` into your GCC specs directory or pass it during compilation:
```bash
gcc -specs=/path/to/safe_c.specs -c main.c -o main.o
```
