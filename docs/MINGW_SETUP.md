# MemCheck MinGW-w64 Setup Guide

This guide describes how to configure and use MemCheck with MinGW-w64 environments on Windows 10/11.

---

## 1. Supported MinGW Distributions

MemCheck has been tested and verified across:
- **MinGW-Builds (MinGW-w64 GCC 13.x, 14.x, 15.x)**
- **MSYS2 MinGW-w64 (`mingw-w64-x86_64-gcc`)**
- **WinLibs standalone GCC builds**

---

## 2. Compiling with MemCheck on MinGW

### Method A: Standalone Driver Wrapper (`gcc-memcheck`)
The easiest method requiring zero GCC modifications:
```powershell
# Compile C file
gcc-memcheck -O2 main.c -o main.exe

# Compile C++ file
g++-memcheck -O2 main.cpp -o main.exe
```

If memory unsafety is detected in `main.c`, compilation will be halted:
```
main.c:16:5: error: [MEMCHECK] Double free detected on pointer 'buffer'.
   16 |     free(buffer);
      |     ^~~~~~~~~~~~
main.c:12:5: note: Previous deallocation occurred here:
   12 |     free(buffer);
      |     ^~~~~~~~~~~~
compilation terminated due to memory safety violation.
```

### Method B: GCC Plugin Direct Invocation
If your MinGW installation includes plugin support:
```powershell
gcc -fplugin=memcheck_plugin.dll -c main.c
```

### Method C: Disabling MemCheck
To compile without MemCheck:
```powershell
gcc-memcheck --memcheck=off main.c -o main.exe
# OR
gcc -fplugin=memcheck_plugin.dll -fplugin-arg-memcheck-disable -c main.c
```
