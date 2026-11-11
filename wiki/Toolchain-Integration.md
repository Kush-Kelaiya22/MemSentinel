# Toolchain Integration

Safe-C provides native integration for both Windows (MinGW-w64) and POSIX/Linux (GCC).

## Windows MinGW-w64 Companion Addon & Plugin

On Windows, Safe-C is built as a native companion shared library `safe_c_addon.dll` and plugin `safe_c.dll`.

### Usage
Run with GCC via the plugin flag or specs:
```cmd
# Load Safe-C plugin directly
gcc -fplugin=build/bin/safe_c.dll -c main.c -o main.o

# Using GCC auto-loading specs file
gcc -specs=build/specs/safe_c.specs -c main.c -o main.o
```

---

## Linux & macOS GCC Plugin

On Linux and macOS, Safe-C is built as a standard GCC shared plugin: `safe_c.so`.

### Usage
Pass `-fplugin=safe_c.so` on your compiler invocation:
```bash
gcc-13 -fplugin=/path/to/safe_c.so -c main.c -o main.o
g++-13 -fplugin=/path/to/safe_c.so -c app.cpp -o app.o
```

---

## Disabling On Demand

Safe-C can be disabled when compiling external third-party dependencies:

1. **Via Command Line Macro:**
   ```bash
   gcc -DSAFE_C_DISABLE -c third_party.c -o third_party.o
   ```
2. **Via Environment Variable:**
   ```bash
   export SAFE_C_DISABLE=1   # POSIX
   set SAFE_C_DISABLE=1      # Windows CMD
   $env:SAFE_C_DISABLE="1"   # PowerShell
   ```
