# MemSentinel Manual Steps, Diagnostics & Deployment Guide

This guide explains how GCC resolves plugins and specs, why some MinGW builds reject plugins, and how to configure your system properly.

---

## 1. Why Did Default Setup Fail On Some MinGW Distributions?

There are two distinct technical reasons why standard plugin setup can fail on certain MinGW installations:

### Reason A: Standalone MinGW-Builds Was Built with `--disable-plugin`
If you downloaded a standalone ZIP release of MinGW-Builds (e.g. from GitHub or SourceForge), check its configure line:
```bash
gcc -v
```
Look for `Configured with:`:
* If it contains `--enable-plugin`: Your GCC **fully supports native plugins**.
* If it lacks `--enable-plugin` (or contains `--disable-plugin`): GCC's internal compiler (`cc1.exe` / `cc1plus.exe`) was compiled without plugin hooks. Loading any plugin throws:
  ```text
  cc1plus.exe: error: plugin support is disabled; configure with '--enable-plugin'
  ```
  **Solution:** Use a MinGW distribution built with plugin support, such as:
  - **MSYS2 MinGW-w64 (Recommended):** `pacman -S mingw-w64-ucrt-x86_64-gcc`
  - **Brecht Sanders MinGW-w64 builds** (with plugin dev enabled).

### Reason B: Resolving the GCC Specs Directory
Running `gcc -print-file-name=specs` on a fresh system will literally output the word `"specs"` if no specs file currently exists.
* **The correct, universal GCC command** to find the compiler's private directory is:
  ```powershell
  # PowerShell:
  $gccLibDir = Split-Path (gcc -print-libgcc-file-name)
  ```
  ```bash
  # Linux / macOS Bash:
  dirname $(gcc -print-libgcc-file-name)
  ```

---

## 2. Automated Diagnostic & Setup Script

MemSentinel includes automated diagnostic and installation tools that detect your GCC build flags, verify compatibility, install the files, and run a self-test:

### On Windows (PowerShell):
```powershell
.\tools\memsentinel-setup.ps1
```
* Checks if `gcc.exe` has `--enable-plugin`.
* Finds the exact `libgcc` directory and copies `memsentinel.specs`.
* Compiles an inline verification snippet to confirm double-free detection is working.
* To uninstall: `.\tools\memsentinel-setup.ps1 -Uninstall`

### On Linux / macOS (Bash):
```bash
chmod +x ./tools/memsentinel-setup.sh
./tools/memsentinel-setup.sh
```

---

## 3. Manual Step-by-Step Installation (If Not Using Setup Script)

If you prefer to perform manual steps without the setup script:

### Step 3.1: Locate the GCC Specs Directory
```powershell
# PowerShell
$destDir = Split-Path -Parent (& gcc -print-libgcc-file-name).Trim()
Copy-Item "src/specs/memsentinel.specs" "$destDir\specs"
```
```bash
# Linux
sudo cp src/specs/memsentinel.specs $(dirname $(gcc -print-libgcc-file-name))/specs
```

### Step 3.2: Copy Plugin Binary
- **Windows (MinGW):**
  ```powershell
  Copy-Item "build/bin/memsentinel.dll" "$((& gcc -print-file-name=plugin).Trim())\"
  ```
- **Linux:**
  ```bash
  sudo cp build/lib/memsentinel.so $(gcc -print-file-name=plugin)/
  ```

---

## 4. Disabling MemSentinel on Demand

To bypass checking for third-party libraries:
```bash
gcc -fplugin-arg-memsentinel-disable main.c -o main.exe
```
Or set an environment variable:
```bash
export MEMSENTINEL_DISABLE=1
# Windows PowerShell:
$env:MEMSENTINEL_DISABLE="1"
```
