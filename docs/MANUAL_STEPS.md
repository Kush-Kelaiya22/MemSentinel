# Manual Steps & Operational Procedures

This document provides a comprehensive, step-by-step checklist of operations that must be performed manually by the developer or systems administrator when deploying **MemCheck** into custom MinGW or Linux environments.

---

## 1. MinGW-w64 Plugin Headers Configuration (Windows Only)

Standard pre-compiled distributions of MinGW-w64 (such as standalone zip releases) often ship with `plugin/` headers omitted to minimize distribution size.

### Verification:
Run the following in your terminal:
```bash
gcc -print-file-name=plugin
```
* If it prints a directory containing `include/gcc-plugin.h`, your MinGW installation is **ready** for native plugin compilation.
* If it returns just `plugin` or the directory lacks `include/gcc-plugin.h`, you must perform **one** of the following:

### Option A: Install via MSYS2 (Recommended for Windows)
```bash
pacman -S mingw-w64-x86_64-gcc-plugin-devel
```
This automatically provides the full GCC plugin SDK matching your compiler version.

### Option B: Using the Universal Driver Wrapper (No Headers Required)
If you do not want to install extra headers, build and use the standalone `gcc-memcheck` driver wrapper located in `bin/gcc-memcheck.exe`. It executes the identical static analysis engine independently before invoking your system MinGW GCC.

---

## 2. Installing the Plugin as Default in GCC / MinGW

To make GCC/G++ automatically load MemCheck for every build without explicitly adding `-fplugin=...`:

### Step 2.1: Copy the Built Plugin
Copy the compiled shared library:
- **Windows (MinGW):** Copy `build/lib/memcheck_plugin.dll` to:
  `<mingw-root>/lib/gcc/<target>/<version>/plugin/`
- **Linux:** Copy `build/lib/memcheck_plugin.so` to:
  `$(gcc -print-file-name=plugin)/`

### Step 2.2: Register via Specs File
Copy `src/driver/memcheck.specs` into your GCC specs directory:
- Run `gcc -dumpspecs > current.specs`
- Append the MemCheck spec line:
  ```
  *cc1_options:
  + %{!fplugin-arg-memcheck-disable:-fplugin=memcheck_plugin}
  ```
- Save as `%GCC_DIR%/specs` or load via `-specs=memcheck.specs`.

### Step 2.3: Alternative via Environment Variable / Aliases
Add to your PowerShell Profile (`$PROFILE`) or bash `.bashrc`:
```powershell
# PowerShell
function gcc { & "K:\mem_check\bin\gcc-memcheck.exe" $args }
function g++ { & "K:\mem_check\bin\g++-memcheck.exe" $args }
```
```bash
# Bash
alias gcc="gcc-memcheck"
alias g++="g++-memcheck"
```

---

## 3. Disabling MemCheck On Demand

If you need to bypass MemCheck for third-party libraries or legacy code:
- **Flag 1:** Pass `-fplugin-arg-memcheck-disable` directly to GCC.
- **Flag 2:** Set the environment variable:
  ```bash
  export MEMCHECK_DISABLE=1
  # or on Windows PowerShell:
  $env:MEMCHECK_DISABLE = "1"
  ```
- **Flag 3:** Pass `--memcheck=off` when using the `gcc-memcheck` driver.

---

## 4. Configuring Runtime Log Output

By default, runtime guardrails write failure reports to `./memcheck_runtime.log` in the current working directory. To redirect logs to a central directory:
```bash
export MEMCHECK_LOG_PATH="C:/Users/YourName/Logs/memcheck.log"
# Windows PowerShell:
$env:MEMCHECK_LOG_PATH = "C:\logs\memcheck.log"
```

---

## 5. GitHub Repository Management (One-Time Setup)

When administering the GitHub repository (`Kush-Kelaiya22/memcheck`):
1. **Branch Protection:**
   Navigate to `Settings -> Branches` on GitHub and add a rule for `main`:
   - Require a pull request before merging.
   - Require status checks to pass before merging (`MemCheck CI`).
2. **Release Artifacts:**
   When cutting new version tags (`v1.0.0`), attach the precompiled `memcheck-windows-x64.zip` and `memcheck-linux-x64.tar.gz` release bundles.
