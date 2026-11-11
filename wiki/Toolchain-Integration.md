# Toolchain Integration

Safe-C provides native integration for both Windows (MinGW-w64) and POSIX/Linux (GCC).

## Windows MinGW-w64 Companion Addon & Plugin

MinGW-w64 distributions vary in plugin support (official `mingw-builds` distributions are built without `--enable-plugin`, while WinLibs and MSYS2 support plugins). Safe-C supports all MinGW-w64 distributions out of the box.

### Usage

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

4. **Permanent Zero-Flag Auto-Loading:**
   Copy `build/bin/safe_c.exe` into MinGW `bin/`, and copy `src/specs/safe_c.specs` as `specs` into your GCC install directory (`gcc -print-search-dirs` under `install:`). Now plain `gcc main.c` automatically runs Safe-C on every compilation.

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
