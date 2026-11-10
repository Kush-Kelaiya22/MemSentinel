# MemSentinel Toolchain Diagnostics & Automated Setup Script (Windows / MinGW-w64)
param (
    [switch]$Uninstall,
    [switch]$VerifyOnly
)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "       MemSentinel Toolchain Diagnostics & Setup          " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Locate GCC executable
$gccCmd = Get-Command "gcc.exe" -ErrorAction SilentlyContinue
if (-not $gccCmd) {
    Write-Host "[ERROR] gcc.exe not found in PATH." -ForegroundColor Red
    Write-Host "Please add your MinGW-w64 bin directory to PATH and try again." -ForegroundColor Yellow
    exit 1
}

$gccPath = $gccCmd.Source
$gccVersion = (gcc --version | Select-Object -First 1)
Write-Host "[OK] Detected GCC: $gccPath" -ForegroundColor Green
Write-Host "     Version     : $gccVersion" -ForegroundColor Gray

# 2. Inspect GCC configuration flags for plugin support
$verboseOutput = & gcc -v 2>&1 | Out-String
$hasPluginSupport = $verboseOutput -match "--enable-plugin"
$hasExplicitDisable = $verboseOutput -match "--disable-plugin"

# 3. Locate exact GCC library directory (where specs belongs)
$libgccPath = (& gcc -print-libgcc-file-name).Trim()
if (-not $libgccPath -or -not (Test-Path $libgccPath)) {
    Write-Host "[ERROR] Could not resolve GCC library directory." -ForegroundColor Red
    exit 1
}
$gccLibDir = Split-Path -Parent $libgccPath
$specsDest = Join-Path $gccLibDir "specs"
$pluginDirRaw = (& gcc -print-file-name=plugin).Trim()

Write-Host "[INFO] GCC Specs Directory: $gccLibDir" -ForegroundColor Gray

if ($Uninstall) {
    Write-Host "`n[UNINSTALL] Removing MemSentinel from toolchain..." -ForegroundColor Yellow
    if (Test-Path $specsDest) {
        Remove-Item -Force $specsDest
        Write-Host "  ✓ Removed specs file from $specsDest" -ForegroundColor Green
    }
    Write-Host "[UNINSTALL COMPLETE] Toolchain restored to default." -ForegroundColor Green
    exit 0
}

# 4. Diagnose Plugin Capability
if (-not $hasPluginSupport) {
    Write-Host "`n[DIAGNOSTIC NOTICE] GCC Plugin Interface Disabled at Build Time" -ForegroundColor Yellow
    Write-Host "------------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "The currently active GCC ($gccPath) was distributed by"
    Write-Host "MinGW-Builds without the '--enable-plugin' configuration flag."
    Write-Host ""
    Write-Host "Why does this happen?" -ForegroundColor White
    Write-Host "Standalone ZIP releases of MinGW-Builds compile GCC with '--disable-plugin'"
    Write-Host "to reduce bundle size. When a specs file attempts to load a plugin into a"
    Write-Host "disabled GCC binary, cc1.exe throws: 'plugin support is disabled'."
    Write-Host ""
    Write-Host "Solution to enable MemSentinel Native Plugin:" -ForegroundColor Cyan
    Write-Host "Install an '--enable-plugin' MinGW distribution such as MSYS2:" -ForegroundColor White
    Write-Host "  pacman -S mingw-w64-ucrt-x86_64-gcc" -ForegroundColor Yellow
    Write-Host "Or WinLibs (GCC with plugin headers enabled)." -ForegroundColor White
    Write-Host "------------------------------------------------------------------" -ForegroundColor DarkGray
    exit 0
}

# 5. Install for GCC with --enable-plugin
Write-Host "`n[INSTALL] GCC supports native plugins. Deploying MemSentinel..." -ForegroundColor Cyan

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$specsSrc = Join-Path $repoRoot "src\specs\memsentinel.specs"
$dllSrc = Join-Path $repoRoot "build\bin\memsentinel.dll"

if (-not (Test-Path $specsSrc)) {
    Write-Host "[ERROR] Specs file not found at: $specsSrc" -ForegroundColor Red
    exit 1
}

# Copy specs file
Copy-Item -Path $specsSrc -Destination $specsDest -Force
Write-Host "  ✓ Installed specs file to: $specsDest" -ForegroundColor Green

# Copy plugin DLL if built
if ((Test-Path $dllSrc) -and (Test-Path $pluginDirRaw)) {
    $dllDest = Join-Path $pluginDirRaw "memsentinel.dll"
    Copy-Item -Path $dllSrc -Destination $dllDest -Force
    Write-Host "  ✓ Installed plugin DLL to: $dllDest" -ForegroundColor Green
}

# 6. Verify Installation
Write-Host "`n[VERIFY] Testing MemSentinel toolchain protection..." -ForegroundColor Cyan
$testSnippet = @"
#include <stdlib.h>
int main(void) {
    int* p = (int*)malloc(16);
    free(p);
    free(p);
    return 0;
}
"@
$testFile = Join-Path $env:TEMP "memsentinel_test.c"
Set-Content -Path $testFile -Value $testSnippet

$verifyOutput = & gcc -c $testFile -o "$testFile.o" 2>&1 | Out-String
Remove-Item -ErrorAction SilentlyContinue $testFile, "$testFile.o"

if ($verifyOutput -match "MEMSENTINEL" -or $verifyOutput -match "Double free") {
    Write-Host "  ✓ Verification PASSED: Double-free compile-time rejection is ACTIVE!" -ForegroundColor Green
} else {
    Write-Host "  Notice: Standard compilation active." -ForegroundColor Yellow
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "                  Setup Check Complete                    " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
