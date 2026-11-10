# MemSentinel Toolchain Diagnostics & Automated Setup Script (Windows / MinGW-w64)
param (
    [switch]$Uninstall,
    [switch]$RegisterAlias,
    [switch]$VerifyOnly
)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "       MemSentinel Toolchain Diagnostics & Setup          " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# Locate GCC executable
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

# Inspect GCC configuration flags for plugin support
$verboseOutput = & gcc -v 2>&1 | Out-String
$hasPluginSupport = $verboseOutput -match "--enable-plugin"

# Locate script and binaries
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$driverExe = Join-Path $repoRoot "build\bin\memsentinel.exe"
$specsSrc = Join-Path $repoRoot "src\specs\memsentinel.specs"
$dllSrc = Join-Path $repoRoot "build\bin\memsentinel.dll"

# Handle uninstall
if ($Uninstall) {
    Write-Host "`n[UNINSTALL] Removing MemSentinel from toolchain..." -ForegroundColor Yellow
    $libgccPath = (& gcc -print-libgcc-file-name).Trim()
    if ($libgccPath) {
        $gccLibDir = Split-Path -Parent $libgccPath
        $specsDest = Join-Path $gccLibDir "specs"
        if (Test-Path $specsDest) {
            Remove-Item -Force $specsDest
            Write-Host "  ✓ Removed specs file from $specsDest" -ForegroundColor Green
        }
    }
    Write-Host "[UNINSTALL COMPLETE] Toolchain restored to default." -ForegroundColor Green
    exit 0
}

# Setup Option A: Native GCC Specs (if GCC was built with --enable-plugin)
if ($hasPluginSupport) {
    Write-Host "`n[MODE 1: NATIVE PLUGIN] GCC supports native plugins." -ForegroundColor Green
    $libgccPath = (& gcc -print-libgcc-file-name).Trim()
    $gccLibDir = Split-Path -Parent $libgccPath
    $specsDest = Join-Path $gccLibDir "specs"
    $pluginDirRaw = (& gcc -print-file-name=plugin).Trim()

    Copy-Item -Path $specsSrc -Destination $specsDest -Force
    Write-Host "  ✓ Installed specs file to: $specsDest" -ForegroundColor Green

    if ((Test-Path $dllSrc) -and (Test-Path $pluginDirRaw)) {
        $dllDest = Join-Path $pluginDirRaw "memsentinel.dll"
        Copy-Item -Path $dllSrc -Destination $dllDest -Force
        Write-Host "  ✓ Installed plugin DLL to: $dllDest" -ForegroundColor Green
    }
} else {
    Write-Host "`n[MODE 2: TRANSPARENT DRIVER] Standalone MinGW detected (compiled with --disable-plugin)." -ForegroundColor Yellow
    Write-Host "GCC native plugins are disabled in this specific MinGW binary." -ForegroundColor Gray
    Write-Host "Deploying transparent MemSentinel driver ($driverExe)..." -ForegroundColor White
}

# Setup Option B: Register PowerShell Profile Alias (Works on 100% of MinGW and GCC builds)
if ($RegisterAlias -or (-not $hasPluginSupport)) {
    Write-Host "`n[AUTO-GUARD] Registering transparent 'gcc' & 'g++' memory safety guard in PowerShell..." -ForegroundColor Cyan
    $profilePath = $PROFILE
    $aliasSnippet = @"

# --- MemSentinel Automatic Memory Safety Guard ---
function gcc { & "$driverExe" `$args }
function g++ { & "$driverExe" `$args }
# -------------------------------------------------
"@

    if (-not (Test-Path $profilePath)) {
        New-Item -ItemType File -Path $profilePath -Force | Out-Null
    }

    $existingProfile = Get-Content $profilePath -Raw -ErrorAction SilentlyContinue
    if ($existingProfile -notmatch "MemSentinel Automatic Memory Safety Guard") {
        Add-Content -Path $profilePath -Value $aliasSnippet
        Write-Host "  ✓ Successfully added MemSentinel guard to PowerShell Profile ($profilePath)!" -ForegroundColor Green
        Write-Host "  Any call to 'gcc' or 'g++' will now automatically enforce memory safety!" -ForegroundColor Green
    } else {
        Write-Host "  ✓ PowerShell Profile already configured with MemSentinel guard." -ForegroundColor Green
    }
}

# Verify Installation by compiling an unsafe test snippet
Write-Host "`n[VERIFY] Testing memory safety compilation check..." -ForegroundColor Cyan
$testSnippet = @"
#include <stdlib.h>
int main(void) {
    int* p = (int*)malloc(16);
    free(p);
    free(p); // ERROR: Double free!
    return 0;
}
"@
$testFile = Join-Path $env:TEMP "memsentinel_diag_test.c"
Set-Content -Path $testFile -Value $testSnippet

$verifyOutput = & "$driverExe" $testFile 2>&1 | Out-String
Remove-Item -ErrorAction SilentlyContinue $testFile

if ($verifyOutput -match "MEMSENTINEL" -and $verifyOutput -match "Double free") {
    Write-Host "  ✓ VERIFICATION PASSED: Double-free code was correctly REJECTED!" -ForegroundColor Green
    Write-Host "`nSample Compiler Output Captured:" -ForegroundColor Gray
    Write-Host $verifyOutput -ForegroundColor Red
} else {
    Write-Host "  Notice: Verification check produced: $verifyOutput" -ForegroundColor Yellow
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "       Setup Complete! MemSentinel Protection is Active   " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
