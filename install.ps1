<#
.SYNOPSIS
    Safe-C Interactive Toolchain Installer & Uninstaller for Windows (MinGW-w64 / MSYS2).
.DESCRIPTION
    Finds GCC/G++ on the system, asks confirmation or lets you enter the gcc/g++ path manually,
    auto-copies/installs the Safe-C plugin, merged specs, and optional runtime files,
    and cleanly uninstalls Safe-C without disturbing other parts of the system.
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$Action = "",

    [Alias("install")]
    [switch]$Install,

    [Alias("update")]
    [switch]$Update,

    [Alias("uninstall")]
    [switch]$Uninstall,

    [string]$GccPath = "",
    [string]$GppPath = "",
    [switch]$FromWeb,
    [Alias("Version")]
    [string]$ReleaseTag = "",
    [switch]$NoRuntime,
    [switch]$Yes,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoUrl = "https://github.com/Kush-Kelaiya22/Safe-C"
if (-not [string]::IsNullOrWhiteSpace($ReleaseTag)) { $FromWeb = $true }

function Show-Usage {
    @"
Safe-C Toolchain Installer & Uninstaller (Windows)

Usage:
  powershell -ExecutionPolicy Bypass -File .\install.ps1 [install | update | uninstall] [OPTIONS]

Actions:
  install, -Install        Install Safe-C into MinGW-w64 GCC/G++
  update, -Update          Check GitHub Releases and update if a newer version is available
  uninstall, -Uninstall    Uninstall Safe-C cleanly without disturbing other files

Options:
  -GccPath <path>          Pre-select path to gcc.exe (or toolchain bin directory)
  -GppPath <path>          Pre-select path to g++.exe (defaults to sibling of gcc.exe)
  -FromWeb                 Download prebuilt plugin, specs, and runtime files from GitHub Releases
  -ReleaseTag <tag>        Release tag to download from GitHub (implies -FromWeb; default: latest)
  -NoRuntime               Install only plugin and specs (skip libsafe_c_rt/headers)
  -Yes                     Accept confirmations non-interactively
  -Help                    Show this help message
"@
}

if ($Help) {
    Show-Usage
    exit 0
}

if ($Uninstall) { $Action = "uninstall" }
elseif ($Update -and [string]::IsNullOrWhiteSpace($Action)) { $Action = "update" }
elseif ($Install -and [string]::IsNullOrWhiteSpace($Action)) { $Action = "install" }

function Invoke-Capture {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath,
        [string[]]$Arguments = @()
    )
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $ExePath
    $psi.Arguments = ($Arguments | ForEach-Object {
        if ($_ -match '\s|"') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }) -join ' '
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true

    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdout = $proc.StandardOutput.ReadToEnd()
    $stderr = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    return [PSCustomObject]@{
        ExitCode = $proc.ExitCode
        StdOut   = $stdout
        StdErr   = $stderr
        Combined = ($stdout + "`n" + $stderr)
    }
}

function Prompt-YesNo {
    param(
        [string]$Message,
        [bool]$DefaultYes = $true
    )
    if ($Yes) { return $true }
    $reply = Read-Host -Prompt $Message
    if ([string]::IsNullOrWhiteSpace($reply)) { return $DefaultYes }
    return ($reply.Trim() -match '^(y|yes)$')
}

function Test-RealGcc {
    param([string]$Candidate)
    if ([string]::IsNullOrWhiteSpace($Candidate)) { return $false }
    if (-not (Test-Path -LiteralPath $Candidate -PathType Leaf)) { return $false }
    try {
        $res = Invoke-Capture -ExePath $Candidate -Arguments @("-dumpspecs")
        return ($res.ExitCode -eq 0 -and -not [string]::IsNullOrWhiteSpace($res.StdOut))
    } catch {
        return $false
    }
}

function Resolve-SiblingCompiler {
    param(
        [string]$BaseExe,
        [ValidateSet("gcc", "g++")][string]$Wanted
    )
    $dir = Split-Path -Parent $BaseExe
    $name = Split-Path -Leaf $BaseExe
    $candidateName = if ($Wanted -eq "g++") {
        if ($name -match '^(.*)gcc(.*)$') { "$($Matches[1])g++$($Matches[2])" } else { "g++.exe" }
    } else {
        if ($name -match '^(.*)g\+\+(.*)$') { "$($Matches[1])gcc$($Matches[2])" } else { "gcc.exe" }
    }
    $cand = Join-Path $dir $candidateName
    if (Test-RealGcc $cand) { return $cand }
    return ""
}

function Resolve-CompilerInput {
    param(
        [string]$RawInput,
        [string]$PreferredKind = "gcc"
    )
    $clean = $RawInput.Trim().Trim('"').Trim("'")
    if ([string]::IsNullOrWhiteSpace($clean)) { return "" }

    if (Test-Path -LiteralPath $clean -PathType Container) {
        $tryNames = if ($PreferredKind -eq "g++") {
            @("g++.exe", "g++", "gcc.exe", "gcc")
        } else {
            @("gcc.exe", "gcc", "g++.exe", "g++")
        }
        foreach ($n in $tryNames) {
            $p = Join-Path $clean $n
            if (Test-RealGcc $p) { return (Resolve-Path -LiteralPath $p).Path }
        }
        return ""
    }

    if (Test-RealGcc $clean) {
        return (Resolve-Path -LiteralPath $clean).Path
    }

    $cmd = Get-Command -Name $clean -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd -and (Test-RealGcc $cmd.Source)) {
        return (Resolve-Path -LiteralPath $cmd.Source).Path
    }
    return ""
}

function Get-GccInfo {
    param([string]$GccExe)

    $verRes = Invoke-Capture -ExePath $GccExe -Arguments @("--version")
    $firstVer = (($verRes.StdOut -split "`r?`n")[0]).Trim()

    $searchRes = Invoke-Capture -ExePath $GccExe -Arguments @("-print-search-dirs")
    $installDir = ""
    $programDirs = @()
    foreach ($line in ($searchRes.StdOut -split "`r?`n")) {
        if ($line -match '^install:\s*(.+)$') {
            $installDir = $Matches[1].Trim().TrimEnd('\', '/')
        } elseif ($line -match '^programs:\s*=?(.*)$') {
            $programDirs = $Matches[1].Trim() -split ';' | Where-Object { $_ -ne "" }
        }
    }
    if ([string]::IsNullOrWhiteSpace($installDir)) {
        $libgcc = Invoke-Capture -ExePath $GccExe -Arguments @("-print-libgcc-file-name")
        if ($libgcc.ExitCode -eq 0 -and -not [string]::IsNullOrWhiteSpace($libgcc.StdOut)) {
            $installDir = Split-Path -Parent $libgcc.StdOut.Trim()
        }
    }
    if ($installDir) {
        try { $installDir = [System.IO.Path]::GetFullPath($installDir).TrimEnd('\', '/') } catch {}
    }

    $pluginRes = Invoke-Capture -ExePath $GccExe -Arguments @("-print-file-name=plugin")
    $pluginRaw = $pluginRes.StdOut.Trim()
    $pluginDir = ""
    if (-not [string]::IsNullOrWhiteSpace($pluginRaw) -and $pluginRaw -ne "plugin") {
        try { $pluginDir = [System.IO.Path]::GetFullPath($pluginRaw).TrimEnd('\', '/') } catch { $pluginDir = $pluginRaw }
    }

    $vRes = Invoke-Capture -ExePath $GccExe -Arguments @("-v")
    $hasPlugin = ($vRes.Combined -match '--enable-plugin')

    return [PSCustomObject]@{
        Exe         = $GccExe
        VersionLine = $firstVer
        InstallDir  = $installDir
        PluginDir   = $pluginDir
        HasPlugin   = [bool]$hasPlugin
        ProgramDirs = $programDirs
    }
}

function Find-SystemGccs {
    $seen = @{}
    $found = New-Object System.Collections.Generic.List[string]

    $cmds = Get-Command -Name "gcc.exe", "gcc" -All -ErrorAction SilentlyContinue
    foreach ($c in $cmds) {
        if ($c.Source -and (Test-RealGcc $c.Source)) {
            $full = (Resolve-Path -LiteralPath $c.Source).Path
            $key = $full.ToLowerInvariant()
            if (-not $seen.ContainsKey($key)) {
                $seen[$key] = $true
                $found.Add($full)
            }
        }
    }

    $commonPaths = @(
        "C:\msys64\ucrt64\bin\gcc.exe",
        "C:\msys64\mingw64\bin\gcc.exe",
        "C:\mingw64\bin\gcc.exe",
        "C:\MinGW\bin\gcc.exe",
        "C:\TDM-GCC-64\bin\gcc.exe"
    )
    if ($env:ChocolateyInstall) {
        $commonPaths += (Join-Path $env:ChocolateyInstall "bin\gcc.exe")
    }
    if ($env:USERPROFILE) {
        $commonPaths += (Join-Path $env:USERPROFILE "scoop\apps\gcc\current\bin\gcc.exe")
        $commonPaths += (Join-Path $env:USERPROFILE "scoop\apps\mingw\current\bin\gcc.exe")
    }

    foreach ($p in $commonPaths) {
        if (Test-RealGcc $p) {
            $full = (Resolve-Path -LiteralPath $p).Path
            $key = $full.ToLowerInvariant()
            if (-not $seen.ContainsKey($key)) {
                $seen[$key] = $true
                $found.Add($full)
            }
        }
    }

    return ,$found.ToArray()
}

function Show-CompilerSummary {
    param(
        [string]$GccExe,
        [string]$GppExe
    )
    $info = Get-GccInfo -GccExe $GccExe
    Write-Host ""
    Write-Host "Selected GCC/G++ Toolchain:" -ForegroundColor Cyan
    Write-Host "  gcc           : $($info.Exe)  [$($info.VersionLine)]"
    if (-not [string]::IsNullOrWhiteSpace($GppExe)) {
        $gppVer = ((Invoke-Capture -ExePath $GppExe -Arguments @("--version")).StdOut -split "`r?`n")[0].Trim()
        Write-Host "  g++           : $GppExe  [$gppVer]"
    } else {
        Write-Host "  g++           : (not paired / using $GccExe)"
    }
    Write-Host "  Install dir   : $($info.InstallDir)"
    if ($info.HasPlugin -and -not [string]::IsNullOrWhiteSpace($info.PluginDir)) {
        Write-Host "  Plugin dir    : $($info.PluginDir)  (--enable-plugin supported)"
    } else {
        Write-Host "  Plugin support: disabled (--enable-plugin absent; direct MinGW addon mode)"
    }
}

function Prompt-ManualCompiler {
    while ($true) {
        Write-Host ""
        $inPath = Read-Host -Prompt "Enter full path to gcc.exe or g++.exe (or toolchain bin directory, 'q' to cancel)"
        if ($inPath -match '^[qQ]$') { return $null }
        if ([string]::IsNullOrWhiteSpace($inPath)) {
            Write-Warning "Path cannot be empty."
            continue
        }

        $resolved = Resolve-CompilerInput -RawInput $inPath -PreferredKind "gcc"
        if ([string]::IsNullOrWhiteSpace($resolved)) {
            Write-Warning "'$inPath' is not a valid GCC/G++ executable or directory containing one."
            continue
        }

        $leaf = (Split-Path -Leaf $resolved).ToLowerInvariant()
        if ($leaf -like '*g++*') {
            $selGpp = $resolved
            $selGcc = Resolve-SiblingCompiler -BaseExe $resolved -Wanted "gcc"
            if ([string]::IsNullOrWhiteSpace($selGcc)) { $selGcc = $resolved }
        } else {
            $selGcc = $resolved
            $selGpp = Resolve-SiblingCompiler -BaseExe $resolved -Wanted "g++"
        }

        $gppPrompt = if ($selGpp) {
            "Optional g++.exe path [Enter to keep '$selGpp']"
        } else {
            "Optional g++.exe path [Enter to skip or type path]"
        }
        $inGpp = Read-Host -Prompt $gppPrompt
        if (-not [string]::IsNullOrWhiteSpace($inGpp)) {
            $customGpp = Resolve-CompilerInput -RawInput $inGpp -PreferredKind "g++"
            if ($customGpp) {
                $selGpp = $customGpp
            } else {
                Write-Warning "'$inGpp' is not a valid g++.exe; keeping '$selGpp'."
            }
        }

        Show-CompilerSummary -GccExe $selGcc -GppExe $selGpp
        Write-Host ""
        if (Prompt-YesNo -Message "Confirm using this compiler path? [Y/n]" -DefaultYes $true) {
            return [PSCustomObject]@{ Gcc = $selGcc; Gpp = $selGpp }
        }
    }
}

function Select-Compiler {
    if (-not [string]::IsNullOrWhiteSpace($GccPath)) {
        $selGcc = Resolve-CompilerInput -RawInput $GccPath -PreferredKind "gcc"
        if ([string]::IsNullOrWhiteSpace($selGcc)) {
            throw "Specified -GccPath '$GccPath' is not a valid GCC executable."
        }
        $selGpp = if (-not [string]::IsNullOrWhiteSpace($GppPath)) {
            $r = Resolve-CompilerInput -RawInput $GppPath -PreferredKind "g++"
            if (-not $r) { throw "Specified -GppPath '$GppPath' is not a valid G++ executable." }
            $r
        } else {
            Resolve-SiblingCompiler -BaseExe $selGcc -Wanted "g++"
        }

        Show-CompilerSummary -GccExe $selGcc -GppExe $selGpp
        if ($Yes) {
            return [PSCustomObject]@{ Gcc = $selGcc; Gpp = $selGpp }
        }
        $ans = Read-Host -Prompt "Is this gcc/g++ path correct? [Y/n/m(anual)/q]"
        if ([string]::IsNullOrWhiteSpace($ans) -or $ans.Trim() -match '^(y|yes)$') {
            return [PSCustomObject]@{ Gcc = $selGcc; Gpp = $selGpp }
        }
        if ($ans.Trim() -match '^(n|no|m|manual)$') {
            return Prompt-ManualCompiler
        }
        return $null
    }

    $candidates = Find-SystemGccs
    if ($candidates.Count -eq 0) {
        Write-Warning "No gcc.exe found automatically on PATH or common MinGW locations."
        if ($Yes) {
            throw "Pass -GccPath <path> when using -Yes without gcc.exe on PATH."
        }
        return Prompt-ManualCompiler
    }

    if ($candidates.Count -eq 1 -or $Yes) {
        $cand = $candidates[0]
        $candGpp = if ($GppPath) { Resolve-CompilerInput -RawInput $GppPath -PreferredKind "g++" } else { Resolve-SiblingCompiler -BaseExe $cand -Wanted "g++" }
        Show-CompilerSummary -GccExe $cand -GppExe $candGpp
        if ($Yes) {
            return [PSCustomObject]@{ Gcc = $cand; Gpp = $candGpp }
        }
        $ans = Read-Host -Prompt "Is this gcc/g++ path correct? [Y/n/m(anual)/q]"
        if ([string]::IsNullOrWhiteSpace($ans) -or $ans.Trim() -match '^(y|yes)$') {
            return [PSCustomObject]@{ Gcc = $cand; Gpp = $candGpp }
        }
        if ($ans.Trim() -match '^(n|no|m|manual)$') {
            return Prompt-ManualCompiler
        }
        return $null
    }

    Write-Host ""
    Write-Host "Detected GCC installations on this system:" -ForegroundColor Cyan
    for ($i = 0; $i -lt $candidates.Count; $i++) {
        $ver = ((Invoke-Capture -ExePath $candidates[$i] -Arguments @("--version")).StdOut -split "`r?`n")[0].Trim()
        Write-Host ("  {0}) {1}  [{2}]" -f ($i + 1), $candidates[$i], $ver)
    }
    Write-Host "  m) Enter gcc/g++ path manually"
    Write-Host "  q) Cancel"
    Write-Host ""

    $choice = Read-Host -Prompt ("Select compiler [1-{0}/m/q] (default 1)" -f $candidates.Count)
    if ([string]::IsNullOrWhiteSpace($choice)) { $choice = "1" }
    if ($choice -match '^[qQ]$') { return $null }
    if ($choice -match '^(m|manual)$') { return Prompt-ManualCompiler }

    $idx = 0
    if ([int]::TryParse($choice, [ref]$idx) -and $idx -ge 1 -and $idx -le $candidates.Count) {
        $chosen = $candidates[$idx - 1]
        $chosenGpp = if ($GppPath) { Resolve-CompilerInput -RawInput $GppPath -PreferredKind "g++" } else { Resolve-SiblingCompiler -BaseExe $chosen -Wanted "g++" }
        Show-CompilerSummary -GccExe $chosen -GppExe $chosenGpp
        $ans = Read-Host -Prompt "Is this gcc/g++ path correct? [Y/n/m(anual)/q]"
        if ([string]::IsNullOrWhiteSpace($ans) -or $ans.Trim() -match '^(y|yes)$') {
            return [PSCustomObject]@{ Gcc = $chosen; Gpp = $chosenGpp }
        }
        if ($ans.Trim() -match '^(n|no|m|manual)$') {
            return Prompt-ManualCompiler
        }
        return $null
    }

    Write-Warning "Invalid selection."
    return $null
}

function Find-SafeCArtifacts {
    param([string]$BaseDir = $ScriptDir)

    $pluginCandidates = @(
        (Join-Path $BaseDir "build\bin\safe_c.dll"),
        (Join-Path $BaseDir "bin\safe_c.dll"),
        (Join-Path $BaseDir "build_mingw\bin\safe_c.dll"),
        (Join-Path $BaseDir "safe_c.dll")
    )
    $pluginSrc = $pluginCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1

    $specsCandidates = @(
        (Join-Path $BaseDir "src\specs\safe_c.specs"),
        (Join-Path $BaseDir "specs\safe_c.specs")
    )
    $specsSrc = $specsCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1

    $rtCandidates = @(
        (Join-Path $BaseDir "build\bin\libsafe_c_rt.dll"),
        (Join-Path $BaseDir "build\lib\libsafe_c_rt.dll.a"),
        (Join-Path $BaseDir "build\lib\libsafe_c_rt.a"),
        (Join-Path $BaseDir "bin\libsafe_c_rt.dll"),
        (Join-Path $BaseDir "lib\libsafe_c_rt.dll.a"),
        (Join-Path $BaseDir "lib\libsafe_c_rt.a")
    )
    $rtFiles = @($rtCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })

    $incDir = Join-Path $BaseDir "include\safe_c"
    if (-not (Test-Path -LiteralPath $incDir -PathType Container)) { $incDir = "" }

    $detectedVer = ""
    $cmakeFile = Join-Path $BaseDir "CMakeLists.txt"
    if (Test-Path -LiteralPath $cmakeFile -PathType Leaf) {
        $m = Select-String -Path $cmakeFile -Pattern '^\s*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)' | Select-Object -First 1
        if ($m) { $detectedVer = "v$($m.Matches[0].Groups[1].Value)" }
    } elseif ((Split-Path -Leaf $BaseDir) -match '^Safe-C-([0-9]+\.[0-9]+\.[0-9]+)-') {
        $detectedVer = "v$($Matches[1])"
    }

    return [PSCustomObject]@{
        PluginSrc  = $pluginSrc
        SpecsSrc   = $specsSrc
        RtFiles    = $rtFiles
        HeaderDir  = $incDir
        TempRoot   = ""
        VersionTag = $detectedVer
        SelfScript = ""
    }
}

function Resolve-LatestReleaseTag {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $latestUrl = "$RepoUrl/releases/latest"
    $req = [System.Net.HttpWebRequest]::Create($latestUrl)
    $req.Method = "HEAD"
    $req.AllowAutoRedirect = $true
    $req.UserAgent = "Safe-C-Installer"
    $resp = $req.GetResponse()
    try {
        $finalUri = $resp.ResponseUri.AbsoluteUri
        if ($finalUri -match '/releases/tag/([^/?#]+)$') {
            return $Matches[1].Trim()
        }
    } finally {
        $resp.Close()
    }
    throw "Could not resolve latest release tag from $latestUrl"
}

function Get-ArtifactsFromWeb {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $tag = $ReleaseTag.Trim()
    if ([string]::IsNullOrWhiteSpace($tag) -or $tag -eq "latest") {
        Write-Host ""
        Write-Host "Resolving latest Safe-C release from GitHub..."
        $tag = Resolve-LatestReleaseTag
    } elseif (-not $tag.StartsWith("v")) {
        $tag = "v$tag"
    }

    $ver = $tag.TrimStart("v")
    $archiveName = "Safe-C-$ver-win64.zip"
    $dlUrl = "$RepoUrl/releases/download/$tag/$archiveName"

    $tmpRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("safec-web-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $tmpRoot -Force | Out-Null
    $zipPath = Join-Path $tmpRoot $archiveName
    $extractDir = Join-Path $tmpRoot "extracted"

    try {
        Write-Host "Downloading $archiveName ($tag) from GitHub Releases..."
        Invoke-WebRequest -Uri $dlUrl -OutFile $zipPath -UseBasicParsing
        Expand-Archive -LiteralPath $zipPath -DestinationPath $extractDir -Force

        $pkgRoot = Join-Path $extractDir "Safe-C-$ver-win64"
        if (-not (Test-Path -LiteralPath $pkgRoot -PathType Container)) {
            $firstDir = Get-ChildItem -LiteralPath $extractDir -Directory | Select-Object -First 1
            $pkgRoot = if ($firstDir) { $firstDir.FullName } else { $extractDir }
        }

        $arts = Find-SafeCArtifacts -BaseDir $pkgRoot
        $arts.TempRoot = $tmpRoot
        $arts.VersionTag = $tag
        $selfCand = Join-Path $pkgRoot "install.ps1"
        if (Test-Path -LiteralPath $selfCand -PathType Leaf) { $arts.SelfScript = $selfCand }
        if ([string]::IsNullOrWhiteSpace($arts.PluginSrc) -or [string]::IsNullOrWhiteSpace($arts.SpecsSrc)) {
            throw "Downloaded archive did not contain safe_c.dll and safe_c.specs."
        }
        Write-Host "[Downloaded] Ready to load $tag from web package." -ForegroundColor Green
        return $arts
    } catch {
        Remove-Item -LiteralPath $tmpRoot -Recurse -Force -ErrorAction SilentlyContinue
        throw
    }
}

function Build-MergedSpecsText {
    param(
        [string]$GccExe,
        [string]$SpecsSrcPath
    )
    $ruleLine = $null
    foreach ($line in (Get-Content -LiteralPath $SpecsSrcPath -Encoding UTF8)) {
        $t = $line.Trim()
        if ($t -eq "" -or $t.StartsWith("#") -or $t.StartsWith("*")) { continue }
        if ($t.StartsWith("+")) { $t = $t.Substring(1).TrimStart() }
        $ruleLine = $t
        break
    }
    if ([string]::IsNullOrWhiteSpace($ruleLine)) {
        throw "Failed to extract Safe-C self_spec rule from $SpecsSrcPath"
    }

    $dump = (Invoke-Capture -ExePath $GccExe -Arguments @("-dumpspecs")).StdOut
    $lines = $dump -split "`r?`n"
    $out = New-Object System.Collections.Generic.List[string]
    $inSelf = $false
    $replaced = $false
    $selfExisting = New-Object System.Collections.Generic.List[string]

    foreach ($line in $lines) {
        if (-not $inSelf -and $line -match '^\*self_spec:\s*$') {
            $out.Add("*self_spec:")
            $inSelf = $true
            $replaced = $true
            continue
        }
        if ($inSelf) {
            if ($line -match '^\*') {
                $combined = if ($selfExisting.Count -gt 0) {
                    ($selfExisting -join ' ') + ' ' + $ruleLine
                } else {
                    $ruleLine
                }
                $out.Add($combined)
                $out.Add("")
                $inSelf = $false
                $out.Add($line)
                continue
            }
            if (-not [string]::IsNullOrWhiteSpace($line) -and ($line -notmatch 'fplugin=safe_c')) {
                $selfExisting.Add($line.Trim())
            }
            continue
        }
        $out.Add($line)
    }

    if ($inSelf) {
        $combined = if ($selfExisting.Count -gt 0) {
            ($selfExisting -join ' ') + ' ' + $ruleLine
        } else {
            $ruleLine
        }
        $out.Add($combined)
    } elseif (-not $replaced) {
        $out.Add("")
        $out.Add("*self_spec:")
        $out.Add($ruleLine)
    }

    return ($out -join "`n") + "`n"
}

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Content
    )
    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $Content, $utf8NoBom)
}

function Install-Target {
    param(
        [string]$GccExe,
        [string]$GppExe,
        $Artifacts
    )
    $info = Get-GccInfo -GccExe $GccExe
    if ([string]::IsNullOrWhiteSpace($info.InstallDir)) {
        throw "Could not determine GCC install directory for $GccExe"
    }

    if (-not $info.HasPlugin -or [string]::IsNullOrWhiteSpace($info.PluginDir)) {
        $py = Get-Command -Name "python3", "python" -ErrorAction SilentlyContinue | Select-Object -First 1
        $helper = Join-Path $ScriptDir "tools\integrate_toolchain.py"
        if ($py -and (Test-Path -LiteralPath $helper -PathType Leaf)) {
            Write-Host "Non-plugin MinGW-w64 detected; invoking integrate_toolchain.py ..."
            & $py.Source $helper $GccExe
            if ($LASTEXITCODE -ne 0) { throw "integrate_toolchain.py failed with exit code $LASTEXITCODE" }
            return
        }
        throw "GCC at '$GccExe' lacks --enable-plugin and tools\integrate_toolchain.py / Python is unavailable."
    }

    New-Item -ItemType Directory -Path $info.PluginDir -Force | Out-Null
    $destPlugin = Join-Path $info.PluginDir "safe_c.dll"
    Copy-Item -LiteralPath $Artifacts.PluginSrc -Destination $destPlugin -Force
    Write-Host "[Installed] Plugin  -> $destPlugin" -ForegroundColor Green

    $destSpecs = Join-Path $info.InstallDir "specs"
    $specsBak = Join-Path $info.InstallDir "specs.safe_c.bak"
    if (Test-Path -LiteralPath $destSpecs -PathType Leaf) {
        $existingContent = Get-Content -LiteralPath $destSpecs -Raw -ErrorAction SilentlyContinue
        if ($existingContent -notmatch 'fplugin=safe_c' -and -not (Test-Path -LiteralPath $specsBak -PathType Leaf)) {
            Copy-Item -LiteralPath $destSpecs -Destination $specsBak -Force
            Write-Host "[Backup]    Preserved existing specs -> $specsBak" -ForegroundColor Yellow
        }
    }

    $mergedSpecs = Build-MergedSpecsText -GccExe $GccExe -SpecsSrcPath $Artifacts.SpecsSrc
    Write-Utf8NoBom -Path $destSpecs -Content $mergedSpecs
    Write-Host "[Installed] Specs   -> $destSpecs" -ForegroundColor Green

    $manifestLines = New-Object System.Collections.Generic.List[string]
    $manifestLines.Add("# Safe-C installation manifest (managed by install.ps1)")
    $manifestLines.Add("GCC=$GccExe")
    if ($GppExe) { $manifestLines.Add("GPP=$GppExe") }
    if ($Artifacts.VersionTag) { $manifestLines.Add("SAFE_C_VERSION=$($Artifacts.VersionTag)") }
    $manifestLines.Add("INSTALL_DIR=$($info.InstallDir)")
    $manifestLines.Add("PLUGIN_FILE=$destPlugin")
    $manifestLines.Add("SPECS_FILE=$destSpecs")
    if (Test-Path -LiteralPath $specsBak -PathType Leaf) {
        $manifestLines.Add("SPECS_BACKUP=$specsBak")
    }

    if (-not $NoRuntime) {
        $binDir = Split-Path -Parent $GccExe
        $prefix = Split-Path -Parent $binDir
        $destBin = Join-Path $prefix "bin"
        $destLib = Join-Path $prefix "lib"
        $destInc = Join-Path $prefix "include\safe_c"

        foreach ($f in $Artifacts.RtFiles) {
            $leaf = Split-Path -Leaf $f
            if ($leaf -like '*.dll') {
                New-Item -ItemType Directory -Path $destBin -Force | Out-Null
                $dst = Join-Path $destBin $leaf
            } else {
                New-Item -ItemType Directory -Path $destLib -Force | Out-Null
                $dst = Join-Path $destLib $leaf
            }
            Copy-Item -LiteralPath $f -Destination $dst -Force
            $manifestLines.Add("EXTRA_FILE=$dst")
            Write-Host "[Installed] Runtime -> $dst" -ForegroundColor Green
        }

        if ($Artifacts.HeaderDir) {
            New-Item -ItemType Directory -Path $destInc -Force | Out-Null
            $manifestLines.Add("EXTRA_DIR=$destInc")
            foreach ($hdr in (Get-ChildItem -LiteralPath $Artifacts.HeaderDir -Filter "*.h" -File)) {
                $dst = Join-Path $destInc $hdr.Name
                Copy-Item -LiteralPath $hdr.FullName -Destination $dst -Force
                $manifestLines.Add("EXTRA_FILE=$dst")
            }
            Write-Host "[Installed] Headers -> $destInc" -ForegroundColor Green
        }
    }

    $manifestPath = Join-Path $info.InstallDir "safe_c.manifest"
    Write-Utf8NoBom -Path $manifestPath -Content (($manifestLines -join "`n") + "`n")
}

function Verify-CompilerIntegration {
    param(
        [string]$GccExe,
        [string]$GppExe
    )
    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("safec-verify-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $tmp -Force | Out-Null
    try {
        $okC = Join-Path $tmp "ok.c"
        $badC = Join-Path $tmp "bad.c"
        Write-Utf8NoBom -Path $okC -Content "#include <stdlib.h>`nint main(void){int*p=(int*)malloc(sizeof(int));if(!p)return 0;*p=42;free(p);return 0;}`n"
        Write-Utf8NoBom -Path $badC -Content "#include <stdlib.h>`nint main(void){int*p=(int*)malloc(sizeof(int));free(p);free(p);return 0;}`n"

        $rOk = Invoke-Capture -ExePath $GccExe -Arguments @("-c", $okC, "-o", (Join-Path $tmp "ok.o"))
        if ($rOk.ExitCode -ne 0) {
            Write-Warning "Valid C compile check failed:`n$($rOk.Combined)"
            return
        }
        $rBad = Invoke-Capture -ExePath $GccExe -Arguments @("-c", $badC, "-o", (Join-Path $tmp "bad.o"))
        if ($rBad.ExitCode -eq 0) {
            Write-Warning "Unsafe double-free test compiled without halting. Check specs/plugin loading."
        } else {
            Write-Host "  [OK] C auto-load check: valid code passed, unsafe double-free blocked." -ForegroundColor Green
        }

        if ($GppExe) {
            $okCpp = Join-Path $tmp "ok.cpp"
            Write-Utf8NoBom -Path $okCpp -Content "#include <vector>`nint main(){std::vector<int> v{1,2,3};return v[0]==1?0:1;}`n"
            $rCpp = Invoke-Capture -ExePath $GppExe -Arguments @("-std=c++20", $okCpp, "-o", (Join-Path $tmp "ok_cpp.exe"))
            if ($rCpp.ExitCode -eq 0) {
                Write-Host "  [OK] C++20 compile & link check passed." -ForegroundColor Green
            } else {
                Write-Warning "C++20 link check failed:`n$($rCpp.Combined)"
            }
        }
    } finally {
        Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-SafeCInstall {
    $sel = Select-Compiler
    if (-not $sel) { Write-Host "Cancelled."; return }

    $artifacts = $null
    try {
        if ($FromWeb) {
            $artifacts = Get-ArtifactsFromWeb
        } else {
            $artifacts = Find-SafeCArtifacts
            if ([string]::IsNullOrWhiteSpace($artifacts.PluginSrc)) {
                $cmake = Get-Command -Name "cmake" -ErrorAction SilentlyContinue | Select-Object -First 1
                $cmakelists = Join-Path $ScriptDir "CMakeLists.txt"
                Write-Host ""
                Write-Host "Safe-C binary (safe_c.dll) was not found locally in build\ or bin\."
                if ($cmake -and (Test-Path -LiteralPath $cmakelists -PathType Leaf)) {
                    if ($Yes) {
                        $artifacts = Get-ArtifactsFromWeb
                    } else {
                        Write-Host "Choose how to obtain Safe-C files:"
                        Write-Host "  1) Download prebuilt Safe-C release from GitHub Releases (default)"
                        Write-Host "  2) Build Safe-C locally from source with CMake"
                        Write-Host "  3) Cancel"
                        $srcChoice = Read-Host -Prompt "Choose option [1/2/3] (default 1)"
                        if ([string]::IsNullOrWhiteSpace($srcChoice)) { $srcChoice = "1" }
                        switch ($srcChoice.Trim()) {
                            "1" { $artifacts = Get-ArtifactsFromWeb }
                            "2" {
                                $genArgs = @("-S", $ScriptDir, "-B", (Join-Path $ScriptDir "build"), "-DCMAKE_BUILD_TYPE=Release")
                                if (Get-Command -Name "ninja" -ErrorAction SilentlyContinue) { $genArgs += @("-G", "Ninja") }
                                $genArgs += "-DCMAKE_C_COMPILER=$($sel.Gcc)"
                                if ($sel.Gpp) { $genArgs += "-DCMAKE_CXX_COMPILER=$($sel.Gpp)" }
                                & $cmake.Source @genArgs
                                if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
                                & $cmake.Source --build (Join-Path $ScriptDir "build") --config Release
                                if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
                                $artifacts = Find-SafeCArtifacts
                            }
                            default { Write-Host "Cancelled."; return }
                        }
                    }
                } elseif (Prompt-YesNo -Message "Download latest prebuilt Safe-C release from GitHub Releases? [Y/n]" -DefaultYes $true) {
                    $artifacts = Get-ArtifactsFromWeb
                } else {
                    Write-Host "Cancelled."
                    return
                }
            }
        }

        if ([string]::IsNullOrWhiteSpace($artifacts.PluginSrc)) {
            throw "safe_c.dll was not found. Build Safe-C locally or pass -FromWeb."
        }
        if ([string]::IsNullOrWhiteSpace($artifacts.SpecsSrc)) {
            throw "safe_c.specs was not found in src\specs\ or specs\."
        }

        Write-Host ""
        Write-Host "Ready to install Safe-C from:"
        Write-Host "  Plugin source : $($artifacts.PluginSrc)"
        Write-Host "  Specs source  : $($artifacts.SpecsSrc)"
        if (-not (Prompt-YesNo -Message "Proceed with installation? [Y/n]" -DefaultYes $true)) {
            Write-Host "Installation cancelled."
            return
        }

        $gccInfo = Get-GccInfo -GccExe $sel.Gcc
        Install-Target -GccExe $sel.Gcc -GppExe $sel.Gpp -Artifacts $artifacts

        if ($sel.Gpp) {
            $gppInfo = Get-GccInfo -GccExe $sel.Gpp
            if ($gppInfo.InstallDir -and $gppInfo.InstallDir -ne $gccInfo.InstallDir) {
                Write-Host "Note: g++.exe uses a separate install dir ($($gppInfo.InstallDir)); installing there too..."
                Install-Target -GccExe $sel.Gpp -GppExe $sel.Gpp -Artifacts $artifacts
            }
        }

        Write-Host ""
        Write-Host "Verifying compiler integration..."
        Verify-CompilerIntegration -GccExe $sel.Gcc -GppExe $sel.Gpp
        Write-Host ""
        Write-Host "Safe-C installation complete. Standard 'gcc' and 'g++' commands now check memory safety automatically." -ForegroundColor Cyan
        Write-Host "To disable temporarily on a build: set SAFE_C_DISABLE=1  (or pass -DSAFE_C_DISABLE)."
    } finally {
        if ($artifacts -and $artifacts.TempRoot -and (Test-Path -LiteralPath $artifacts.TempRoot -PathType Container)) {
            Remove-Item -LiteralPath $artifacts.TempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

function Uninstall-SingleTarget {
    param([string]$GccExe)
    $info = Get-GccInfo -GccExe $GccExe
    if ([string]::IsNullOrWhiteSpace($info.InstallDir)) {
        throw "Could not determine GCC install directory for $GccExe"
    }

    $manifestPath = Join-Path $info.InstallDir "safe_c.manifest"
    Write-Host ""
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        Write-Host "Found Safe-C installation manifest: $manifestPath" -ForegroundColor Cyan
        $pluginFile = ""
        $specsFile = ""
        $specsBackup = ""
        $extraFiles = New-Object System.Collections.Generic.List[string]
        $extraDirs = New-Object System.Collections.Generic.List[string]

        foreach ($line in (Get-Content -LiteralPath $manifestPath -Encoding UTF8)) {
            if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) { continue }
            $idx = $line.IndexOf("=")
            if ($idx -lt 1) { continue }
            $k = $line.Substring(0, $idx)
            $v = $line.Substring($idx + 1)
            switch ($k) {
                "PLUGIN_FILE"  { $pluginFile = $v }
                "SPECS_FILE"   { $specsFile = $v }
                "SPECS_BACKUP" { $specsBackup = $v }
                "EXTRA_FILE"   { $extraFiles.Add($v) }
                "EXTRA_DIR"    { $extraDirs.Add($v) }
            }
        }

        Write-Host "The following Safe-C changes will be reverted cleanly:"
        if ($pluginFile) { Write-Host "  Remove plugin : $pluginFile" }
        if ($specsBackup -and (Test-Path -LiteralPath $specsBackup -PathType Leaf)) {
            Write-Host "  Restore specs : $specsBackup -> $specsFile"
        } elseif ($specsFile) {
            Write-Host "  Remove specs  : $specsFile (only if Safe-C managed)"
        }
        if ($extraFiles.Count -gt 0) {
            Write-Host ("  Remove {0} Safe-C runtime/header file(s)" -f $extraFiles.Count)
        }

        if (-not (Prompt-YesNo -Message "Proceed with Safe-C uninstall? [Y/n]" -DefaultYes $true)) {
            Write-Host "Uninstall cancelled."
            return
        }

        if ($specsBackup -and (Test-Path -LiteralPath $specsBackup -PathType Leaf)) {
            $dstSpecs = if ($specsFile) { $specsFile } else { Join-Path $info.InstallDir "specs" }
            Move-Item -LiteralPath $specsBackup -Destination $dstSpecs -Force
            Write-Host "[Restored]  Pre-existing specs file." -ForegroundColor Green
        } elseif ($specsFile -and (Test-Path -LiteralPath $specsFile -PathType Leaf)) {
            $raw = Get-Content -LiteralPath $specsFile -Raw -ErrorAction SilentlyContinue
            if ($raw -match 'fplugin=safe_c') {
                Remove-Item -LiteralPath $specsFile -Force
                Write-Host "[Removed]   $specsFile" -ForegroundColor Green
            } else {
                Write-Host "[Preserved] $specsFile contains non-Safe-C customization; leaving untouched." -ForegroundColor Yellow
            }
        }

        if ($pluginFile -and (Test-Path -LiteralPath $pluginFile -PathType Leaf)) {
            Remove-Item -LiteralPath $pluginFile -Force
            Write-Host "[Removed]   $pluginFile" -ForegroundColor Green
        }

        foreach ($ef in $extraFiles) {
            if (Test-Path -LiteralPath $ef -PathType Leaf) {
                Remove-Item -LiteralPath $ef -Force
                Write-Host "[Removed]   $ef" -ForegroundColor Green
            }
        }

        foreach ($ed in $extraDirs) {
            if (Test-Path -LiteralPath $ed -PathType Container) {
                $remaining = Get-ChildItem -LiteralPath $ed -Force -ErrorAction SilentlyContinue
                if ($null -eq $remaining -or $remaining.Count -eq 0) {
                    Remove-Item -LiteralPath $ed -Force
                    Write-Host "[Removed]   Empty dir $ed" -ForegroundColor Green
                }
            }
        }

        Remove-Item -LiteralPath $manifestPath -Force
    } else {
        Write-Host "No manifest at $manifestPath; checking standard Safe-C files..." -ForegroundColor Cyan
        if (-not (Prompt-YesNo -Message "Check and remove Safe-C files from $($info.InstallDir)? [Y/n]" -DefaultYes $true)) {
            Write-Host "Uninstall cancelled."
            return
        }

        $candSpecs = Join-Path $info.InstallDir "specs"
        $candBak = Join-Path $info.InstallDir "specs.safe_c.bak"
        if (Test-Path -LiteralPath $candBak -PathType Leaf) {
            Move-Item -LiteralPath $candBak -Destination $candSpecs -Force
            Write-Host "[Restored]  $candSpecs from backup." -ForegroundColor Green
        } elseif (Test-Path -LiteralPath $candSpecs -PathType Leaf) {
            $raw = Get-Content -LiteralPath $candSpecs -Raw -ErrorAction SilentlyContinue
            if ($raw -match 'fplugin=safe_c') {
                Remove-Item -LiteralPath $candSpecs -Force
                Write-Host "[Removed]   $candSpecs" -ForegroundColor Green
            } else {
                Write-Host "[Preserved] $candSpecs is not a Safe-C specs file; leaving untouched." -ForegroundColor Yellow
            }
        }

        if ($info.PluginDir) {
            $candPlugin = Join-Path $info.PluginDir "safe_c.dll"
            if (Test-Path -LiteralPath $candPlugin -PathType Leaf) {
                Remove-Item -LiteralPath $candPlugin -Force
                Write-Host "[Removed]   $candPlugin" -ForegroundColor Green
            }
        }

        $py = Get-Command -Name "python3", "python" -ErrorAction SilentlyContinue | Select-Object -First 1
        $helper = Join-Path $ScriptDir "tools\integrate_toolchain.py"
        if ($py -and (Test-Path -LiteralPath $helper -PathType Leaf)) {
            & $py.Source $helper --uninstall $GccExe
        }

        $prefix = Split-Path -Parent (Split-Path -Parent $GccExe)
        $knownFiles = @(
            (Join-Path $prefix "bin\libsafe_c_rt.dll"),
            (Join-Path $prefix "lib\libsafe_c_rt.dll.a"),
            (Join-Path $prefix "lib\libsafe_c_rt.a"),
            (Join-Path $prefix "include\safe_c\api.h"),
            (Join-Path $prefix "include\safe_c\common.h"),
            (Join-Path $prefix "include\safe_c\typestate.h")
        )
        foreach ($kf in $knownFiles) {
            if (Test-Path -LiteralPath $kf -PathType Leaf) {
                Remove-Item -LiteralPath $kf -Force
                Write-Host "[Removed]   $kf" -ForegroundColor Green
            }
        }
        $incSafeC = Join-Path $prefix "include\safe_c"
        if (Test-Path -LiteralPath $incSafeC -PathType Container) {
            $remaining = Get-ChildItem -LiteralPath $incSafeC -Force -ErrorAction SilentlyContinue
            if ($null -eq $remaining -or $remaining.Count -eq 0) {
                Remove-Item -LiteralPath $incSafeC -Force
            }
        }
    }
}

function Invoke-SafeCUninstall {
    $sel = Select-Compiler
    if (-not $sel) { Write-Host "Cancelled."; return }

    $gccInfo = Get-GccInfo -GccExe $sel.Gcc
    Uninstall-SingleTarget -GccExe $sel.Gcc

    if ($sel.Gpp) {
        $gppInfo = Get-GccInfo -GccExe $sel.Gpp
        if ($gppInfo.InstallDir -and $gppInfo.InstallDir -ne $gccInfo.InstallDir) {
            Uninstall-SingleTarget -GccExe $sel.Gpp
        }
    }

    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("safec-post-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $tmp -Force | Out-Null
    try {
        $testC = Join-Path $tmp "test.c"
        Write-Utf8NoBom -Path $testC -Content "int main(void){return 0;}`n"
        $res = Invoke-Capture -ExePath $sel.Gcc -Arguments @($testC, "-o", (Join-Path $tmp "test.exe"))
        if ($res.ExitCode -eq 0) {
            Write-Host ""
            Write-Host "[OK] Post-uninstall check: gcc compiles cleanly without Safe-C." -ForegroundColor Green
        }
    } finally {
        Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }

    Write-Host "Safe-C uninstalled cleanly. System compiler and unrelated files were preserved." -ForegroundColor Cyan
}

function Get-ManifestVersion {
    param([string]$ManifestPath)
    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) { return "" }
    foreach ($line in (Get-Content -LiteralPath $ManifestPath -Encoding UTF8)) {
        if ($line -match '^SAFE_C_VERSION=(.+)$') {
            return $Matches[1].Trim()
        }
    }
    return ""
}

function Test-SemVerGreater {
    param([string]$Candidate, [string]$Current)
    if ([string]::IsNullOrWhiteSpace($Current)) { return $true }
    $cClean = ($Candidate.Trim().TrimStart('v') -replace '-.*$', '')
    $curClean = ($Current.Trim().TrimStart('v') -replace '-.*$', '')
    try {
        return ([version]$cClean -gt [version]$curClean)
    } catch {
        return ($cClean -ne $curClean)
    }
}

function Update-StandaloneScript {
    param($Artifacts)
    if (-not $Artifacts -or [string]::IsNullOrWhiteSpace($Artifacts.SelfScript)) { return }
    if (-not (Test-Path -LiteralPath $Artifacts.SelfScript -PathType Leaf)) { return }
    if ((Test-Path -LiteralPath (Join-Path $ScriptDir "CMakeLists.txt") -PathType Leaf) -or (Test-Path -LiteralPath (Join-Path $ScriptDir ".git"))) {
        return
    }
    $target = Join-Path $ScriptDir "install.ps1"
    try {
        Copy-Item -LiteralPath $Artifacts.SelfScript -Destination $target -Force
        Write-Host "[Updated]   Script -> $target" -ForegroundColor Green
    } catch {
        # Non-fatal if script directory is read-only
    }
}

function Invoke-SafeCUpdate {
    $sel = Select-Compiler
    if (-not $sel) { Write-Host "Cancelled."; return }

    $gccInfo = Get-GccInfo -GccExe $sel.Gcc
    $manifestPath = Join-Path $gccInfo.InstallDir "safe_c.manifest"
    $installedVer = Get-ManifestVersion -ManifestPath $manifestPath

    $targetTag = $ReleaseTag.Trim()
    Write-Host ""
    if ([string]::IsNullOrWhiteSpace($targetTag) -or $targetTag -eq "latest") {
        Write-Host "Checking GitHub Releases for latest Safe-C version..."
        $targetTag = Resolve-LatestReleaseTag
    } elseif (-not $targetTag.StartsWith("v")) {
        $targetTag = "v$targetTag"
    }

    Write-Host "Version check:" -ForegroundColor Cyan
    Write-Host ("  Installed : {0}" -f $(if ($installedVer) { $installedVer } else { "not recorded / not installed via manifest" }))
    Write-Host "  Available : $targetTag"

    if ($installedVer -and -not (Test-SemVerGreater -Candidate $targetTag -Current $installedVer)) {
        Write-Host ""
        Write-Host "Safe-C is already up to date ($installedVer)." -ForegroundColor Green
        if ($Yes) { return }
        if (-not (Prompt-YesNo -Message "Reinstall $targetTag anyway? [y/N]" -DefaultYes $false)) {
            return
        }
    } else {
        Write-Host ""
        if (-not (Prompt-YesNo -Message "Download and install Safe-C ${targetTag}? [Y/n]" -DefaultYes $true)) {
            Write-Host "Update cancelled."
            return
        }
    }

    $script:ReleaseTag = $targetTag
    $artifacts = $null
    try {
        $artifacts = Get-ArtifactsFromWeb
        Install-Target -GccExe $sel.Gcc -GppExe $sel.Gpp -Artifacts $artifacts
        if ($sel.Gpp) {
            $gppInfo = Get-GccInfo -GccExe $sel.Gpp
            if ($gppInfo.InstallDir -and $gppInfo.InstallDir -ne $gccInfo.InstallDir) {
                Write-Host "Note: g++.exe uses a separate install dir ($($gppInfo.InstallDir)); updating there too..."
                Install-Target -GccExe $sel.Gpp -GppExe $sel.Gpp -Artifacts $artifacts
            }
        }
        Write-Host ""
        Write-Host "Verifying compiler integration..."
        Verify-CompilerIntegration -GccExe $sel.Gcc -GppExe $sel.Gpp
        Update-StandaloneScript -Artifacts $artifacts
        Write-Host ""
        Write-Host "Safe-C updated to $targetTag cleanly." -ForegroundColor Cyan
    } finally {
        if ($artifacts -and $artifacts.TempRoot -and (Test-Path -LiteralPath $artifacts.TempRoot -PathType Container)) {
            Remove-Item -LiteralPath $artifacts.TempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

if ([string]::IsNullOrWhiteSpace($Action)) {
    Write-Host "Safe-C Toolchain Installer & Uninstaller" -ForegroundColor Cyan
    Write-Host "  1) Install Safe-C into GCC/G++"
    Write-Host "  2) Update Safe-C (check GitHub Releases)"
    Write-Host "  3) Uninstall Safe-C cleanly"
    Write-Host "  4) Exit"
    $menu = Read-Host -Prompt "Choose action [1/2/3/4] (default 1)"
    if ([string]::IsNullOrWhiteSpace($menu)) { $menu = "1" }
    switch ($menu.Trim().ToLowerInvariant()) {
        "1"         { $Action = "install" }
        "install"   { $Action = "install" }
        "2"         { $Action = "update" }
        "update"    { $Action = "update" }
        "3"         { $Action = "uninstall" }
        "uninstall" { $Action = "uninstall" }
        "4"         { exit 0 }
        "q"         { exit 0 }
        "exit"      { exit 0 }
        default     { throw "Invalid action choice: $menu" }
    }
}

switch ($Action.Trim().ToLowerInvariant()) {
    "install"   { Invoke-SafeCInstall }
    "update"    { Invoke-SafeCUpdate }
    "uninstall" { Invoke-SafeCUninstall }
    default     { Show-Usage; exit 1 }
}
