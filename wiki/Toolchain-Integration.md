# Toolchain Integration

Safe-C provides native integration for both Windows (MinGW-w64) and POSIX/Linux (GCC).

## Enable by Default (Zero-Flag Toolchain Activation)

Safe-C can be enabled by default across your entire toolchain so that plain `gcc` and `g++` invocations automatically enforce memory safety:

```cmd
# Enable Safe-C by default for host GCC/G++:
safe_c --enable-default

# Or via CMake build target:
cmake --build build --target enable-default

# Check current activation status:
safe_c --status

# Disable default auto-loading:
safe_c --disable-default
```

Once enabled, plain commands like `gcc -c main.c` and `g++ -c app.cpp` automatically enforce memory safety and halt compilation on violations.

---

## Per-Invocation Usage Options

1. **Universal Specs File (Works across ALL MinGW-w64 toolchains):**
   ```cmd
   gcc -specs=build/specs/safe_c.specs -c main.c -o main.o
   g++ -specs=build/specs/safe_c.specs -c app.cpp -o app.o
   ```

2. **Direct Compiler Plugin Invocation (for plugin-enabled toolchains):**
   ```cmd
   gcc -fplugin=build/bin/safe_c.dll -c main.c -o main.o
   ```

3. **Standalone Static Analysis:**
   ```cmd
   safe_c main.c
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
