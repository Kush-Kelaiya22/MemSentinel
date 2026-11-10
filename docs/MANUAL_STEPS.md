# MemSentinel Manual Steps & Deployment Procedures

This guide provides an explicit checklist of manual operations required when deploying **MemSentinel** into custom MinGW or Linux environments.

---

## 1. MinGW-w64 Plugin Headers Configuration (Windows Only)

Standard pre-compiled distributions of MinGW-w64 often omit plugin headers to save disk space.

### Check if Plugin Headers are Present:
Run in PowerShell:
```powershell
gcc -print-file-name=plugin
```
* If a directory path containing `include/gcc-plugin.h` is returned, your MinGW installation is **ready** for native plugin building.
* If it returns `plugin` or the directory lacks `include/gcc-plugin.h`, install the development package via MSYS2:
  ```bash
  pacman -S mingw-w64-x86_64-gcc-plugin-devel
  ```

---

## 2. Deploying MemSentinel as Default in GCC / MinGW (Zero Wrappers)

To make `gcc` and `g++` automatically invoke MemSentinel without typing `-fplugin=...` manually:

### Step 2.1: Copy the Built Plugin
- **Windows (MinGW):**
  ```powershell
  Copy-Item "build/bin/memsentinel.dll" "$((gcc -print-file-name=plugin).Trim())\"
  ```
- **Linux:**
  ```bash
  sudo cp build/lib/memsentinel.so $(gcc -print-file-name=plugin)/
  ```

### Step 2.2: Register via Specs File
Copy `src/specs/memsentinel.specs` into your GCC installation directory as `specs`:
- **Windows (MinGW):**
  ```powershell
  Copy-Item "src/specs/memsentinel.specs" "$((gcc -print-file-name=specs).Trim())\specs"
  ```
- **Linux:**
  ```bash
  sudo cp src/specs/memsentinel.specs $(dirname $(gcc -print-libgcc-file-name))/specs
  ```

---

## 3. Disabling MemSentinel on Demand

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

---

## 4. Customizing Runtime Diagnostic Log Destination

By default, runtime guardrails write failure reports to `./memcheck_runtime.log`. To specify a central log path:
```bash
export MEMCHECK_LOG_PATH="/var/log/memsentinel.log"
# Windows PowerShell:
$env:MEMCHECK_LOG_PATH = "C:\logs\memsentinel.log"
```
