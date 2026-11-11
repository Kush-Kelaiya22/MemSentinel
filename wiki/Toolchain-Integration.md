# Toolchain Integration

Safe-C provides native integration for both Windows (MinGW-w64) and POSIX/Linux (GCC).

## Direct GCC & G++ Plugin Usage

Safe-C is built as a pure native shared library (`safe_c.dll` on Windows, `safe_c.so` on Linux):

### 1. Direct Plugin Flag
```cmd
# Windows (MinGW-w64):
gcc -fplugin=build/bin/safe_c.dll -c main.c -o main.o
g++ -fplugin=build/bin/safe_c.dll -c app.cpp -o app.o

# Linux & macOS:
gcc -fplugin=build/lib/safe_c.so -c main.c -o main.o
g++ -fplugin=build/lib/safe_c.so -c app.cpp -o app.o
```

### 2. Auto-Loading Specs File
```cmd
gcc -specs=build/specs/safe_c.specs -c main.c -o main.o
g++ -specs=build/specs/safe_c.specs -c app.cpp -o app.o
gcc -specs=build/specs/safe_c.specs main.c -o app.exe
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
