# Toolchain Integration

Safe-C provides native integration for both Windows (MinGW-w64) and POSIX/Linux (GCC).

## Windows MinGW-w64 Companion Addon

On Windows, Safe-C is built as a zero-flag companion shared library: `libwinpthread-1.dll`.

### Installation
Copy `build/bin/addon/libwinpthread-1.dll` to your MinGW GCC libexec directory:
```
<mingw-root>/libexec/gcc/x86_64-w64-mingw32/<version>/libwinpthread-1.dll
```

### Usage
Every standard compilation command runs Safe-C automatically:
```cmd
gcc -c main.c -o main.o
g++ -c app.cpp -o app.o
```

Zero extra compiler flags, zero wrapper scripts, and zero custom binaries are needed.

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
