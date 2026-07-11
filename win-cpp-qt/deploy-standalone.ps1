param(
    [Parameter(Mandatory = $true)][string]$TargetDir,
    [Parameter(Mandatory = $true)][string]$ExePath,
    [string]$QtPrefix = "",
    [string]$MingwRoot = ""
)

$ErrorActionPreference = "Stop"

function Resolve-QtPrefix {
    param([string]$Hint)
    if ($Hint -and (Test-Path -LiteralPath $Hint)) {
        return (Resolve-Path -LiteralPath $Hint).Path
    }
    if ($env:CMAKE_PREFIX_PATH -and (Test-Path -LiteralPath $env:CMAKE_PREFIX_PATH)) {
        return (Resolve-Path -LiteralPath $env:CMAKE_PREFIX_PATH).Path
    }
    $guesses = @(
        "C:\cpp\qt\6.10.3\mingw_64",
        "C:\Qt\6.10.3\mingw_64",
        "C:\Qt\6.8.0\mingw_64",
        "C:\Qt\6.7.3\mingw_64",
        "C:\Qt\6.6.3\mingw_64"
    )
    foreach ($p in $guesses) {
        if (Test-Path -LiteralPath $p) {
            return (Resolve-Path -LiteralPath $p).Path
        }
    }
    throw "Qt prefix not found. Set -QtPrefix or CMAKE_PREFIX_PATH."
}

function Resolve-QtMingwRoot {
    param(
        [Parameter(Mandatory = $true)][string]$QtPrefix,
        [string]$Hint = ""
    )
    if ($Hint -and (Test-Path -LiteralPath $Hint)) {
        return (Resolve-Path -LiteralPath $Hint).Path
    }
    $versionDir = Split-Path $QtPrefix -Parent
    $qtRoot = Split-Path $versionDir -Parent
    $toolsDir = Join-Path $qtRoot "Tools"
    if (-not (Test-Path -LiteralPath $toolsDir)) {
        throw "Qt Tools directory not found: $toolsDir"
    }
    $candidates = Get-ChildItem -LiteralPath $toolsDir -Directory -Filter "mingw*_64" |
        Sort-Object Name -Descending
    foreach ($cand in $candidates) {
        $gpp = Join-Path $cand.FullName "bin\g++.exe"
        if (Test-Path -LiteralPath $gpp) {
            return (Resolve-Path -LiteralPath $cand.FullName).Path
        }
    }
    throw "No mingw*_64 toolchain with g++.exe under $toolsDir"
}

function Copy-WithRetry {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination,
        [int]$Attempts = 8,
        [int]$DelayMs = 400
    )
    for ($i = 1; $i -le $Attempts; $i++) {
        try {
            Copy-Item -LiteralPath $Source -Destination $Destination -Force
            return
        } catch {
            if ($i -ge $Attempts) { throw }
            Start-Sleep -Milliseconds $DelayMs
        }
    }
}

function Resolve-Windeployqt {
    param([Parameter(Mandatory = $true)][string]$QtPrefix)
    $candidates = @(
        (Join-Path $QtPrefix "bin\windeployqt.exe"),
        (Join-Path $QtPrefix "bin\windeployqt-qt6.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) {
            return (Resolve-Path -LiteralPath $c).Path
        }
    }
    $cmd = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    throw "windeployqt not found under $QtPrefix\bin and not in PATH."
}

if (-not (Test-Path -LiteralPath $TargetDir)) {
    throw "Target directory does not exist: $TargetDir"
}
if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Executable does not exist: $ExePath"
}

$resolvedQt = Resolve-QtPrefix -Hint $QtPrefix
$windeployqt = Resolve-Windeployqt -QtPrefix $resolvedQt
$qtBin = Join-Path $resolvedQt "bin"

$resolvedMingw = Resolve-QtMingwRoot -QtPrefix $resolvedQt -Hint $MingwRoot
$mingwBin = Join-Path $resolvedMingw "bin"

Write-Host "Deploying standalone Qt runtime to: $TargetDir"
Write-Host "Qt prefix: $resolvedQt"
Write-Host "Qt MinGW: $resolvedMingw"
Write-Host "windeployqt: $windeployqt"

$env:PATH = "$qtBin;" + $env:PATH

$deployArgs = @(
    "--release",
    "--compiler-runtime",
    "--no-translations",
    "--no-quick-import",
    "--no-system-d3d-compiler",
    "--no-opengl-sw",
    "--dir", $TargetDir,
    $ExePath
)

& $windeployqt @deployArgs
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

# windeployqt copies Qt runtime for Qt6*.dll; overwrite with the paired Qt toolchain DLLs.
$runtimeDlls = @("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll")
foreach ($dll in $runtimeDlls) {
    $src = Join-Path $mingwBin $dll
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Missing Qt toolchain runtime DLL: $src"
    }
    Copy-WithRetry -Source $src -Destination (Join-Path $TargetDir $dll)
}

$required = @(
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    (Join-Path "platforms" "qwindows.dll"),
    "libstdc++-6.dll",
    "libgcc_s_seh-1.dll",
    "libwinpthread-1.dll"
)
foreach ($rel in $required) {
    $full = Join-Path $TargetDir $rel
    if (-not (Test-Path -LiteralPath $full)) {
        throw "Standalone deploy incomplete; missing required file: $rel"
    }
}

Write-Host "Standalone deploy OK: $TargetDir"
