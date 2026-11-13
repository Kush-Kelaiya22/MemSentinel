# Toolchain Integration

Safe-C provides native integration for both Windows (MinGW-w64) and POSIX/Linux (GCC).

## Interactive Installer & Uninstaller (`install.sh` / `install.ps1`)

Safe-C includes interactive shell scripts for Linux/POSIX/MSYS2 (`install.sh`) and Windows PowerShell (`install.ps1`). Both scripts:
- Auto-detect installed `gcc` and `g++` compilers, display the discovered toolchain/plugin/specs directories, and ask you to confirm or manually enter a custom `gcc`/`g++` path.
- Copy the plugin (`safe_c.so` / `safe_c.dll`), generate a merged `specs` file from `gcc -dumpspecs` (backing up any existing non-Safe-C `specs` file first), and optionally install `libsafe_c_rt` runtime libraries and headers.
- Track installed files in a local `safe_c.manifest` so `uninstall` removes only Safe-C files and restores backups cleanly without disturbing other system packages.

```bash
# Linux / MSYS2 / Git Bash:
./install.sh              # interactive menu (Install / Uninstall)
./install.sh uninstall    # clean removal without touching unrelated system files
```

```powershell
# Windows PowerShell:
powershell -ExecutionPolicy Bypass -File .\install.ps1
powershell -ExecutionPolicy Bypass -File .\install.ps1 uninstall
```

---

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
gcc -specs=src/specs/safe_c.specs -c main.c -o main.o
g++ -specs=src/specs/safe_c.specs -c app.cpp -o app.o
gcc -specs=src/specs/safe_c.specs main.c -o app.exe
```

> Never copy bare `src/specs/safe_c.specs` over GCC's system `specs` path alone—doing so removes GCC's default `*libgcc:` and `--eh-frame-hdr` entries needed for C++ linking (`_Unwind_Resume`) and exception handling. Always use `tools/integrate_toolchain.py` or merge `*self_spec:` into full `gcc -dumpspecs` output, and export `SAFE_C_DISABLE=1` when building external system packages.

---

## Compiler Flags & Partial Check Controls

Safe-C can be globally disabled/enabled, switched to warning mode, or partially configured per translation unit:

| Flag / Macro | Plugin Argument | Behavior |
| :--- | :--- | :--- |
| `-DSAFE_C_DISABLE` | `-fplugin-arg-safe_c-disable` | Disable Safe-C for this compilation |
| `-DSAFE_C_ENABLE` | `-fplugin-arg-safe_c-enable` | Force-enable Safe-C (overrides `SAFE_C_DISABLE`) |
| `-DSAFE_C_WARN_ONLY` | `-fplugin-arg-safe_c-mode=warn` | Report violations as GCC warnings instead of errors |
| `-DSAFE_C_FATAL` | `-fplugin-arg-safe_c-mode=fatal` | Halt compilation on violations (default) |
| `-DSAFE_C_NO_LEAKS` | `-fplugin-arg-safe_c-leaks=off` | Turn off heap leak checks |
| `-DSAFE_C_NO_DOUBLE_FREE` | `-fplugin-arg-safe_c-double-free=off` | Turn off double-free checks |
| `-DSAFE_C_NO_UAF` | `-fplugin-arg-safe_c-uaf=off` | Turn off use-after-free checks |
| `-DSAFE_C_NO_BOUNDS` | `-fplugin-arg-safe_c-bounds=off` | Turn off static bounds checks |
| `-DSAFE_C_NO_NULL` | `-fplugin-arg-safe_c-null=off` | Turn off null pointer dereference checks |
| `-DSAFE_C_NO_UNINIT` | `-fplugin-arg-safe_c-uninit=off` | Turn off uninitialized pointer checks |
| `-DSAFE_C_NO_MISMATCH` | `-fplugin-arg-safe_c-mismatch=off` | Turn off mismatched deallocator checks |
| `-DSAFE_C_NO_DANGLING` | `-fplugin-arg-safe_c-dangling=off` | Turn off dangling local-address return errors |
| `-DSAFE_C_NO_ARITHMETIC` | `-fplugin-arg-safe_c-arithmetic=off` | Turn off pointer-arithmetic base checks |
| `-DSAFE_C_NO_ITERATOR` | `-fplugin-arg-safe_c-iterator=off` | Turn off C++ container iterator invalidation checks |

Environment variables: `SAFE_C_DISABLE=1`, `SAFE_C_ENABLE=1`, `SAFE_C_MODE=warn|fatal|off`.
