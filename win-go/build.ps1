# Fail fast on any error so the script never continues in a half-built state.
$ErrorActionPreference = "Stop"
# Module root (this script lives in win-go/). All paths are derived from here; callers may use any cwd.
$ModuleRoot = $PSScriptRoot
$CodebaseRoot = Split-Path $ModuleRoot -Parent
$ShitwiperRoot = Split-Path $CodebaseRoot -Parent
$goCmd = Get-Command go -ErrorAction SilentlyContinue
$go = if ($goCmd) { $goCmd.Source } else { "go" }
# Central path to Windows version resource template that feeds goversioninfo.
$VersionInfoPath = Join-Path $ModuleRoot "versioninfo.json"

# Copy a file with retry attempts to tolerate short-lived file locks (AV/indexer/etc.).
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
            if ($i -ge $Attempts) {
                throw
            }
            Start-Sleep -Milliseconds $DelayMs
        }
    }
}

# Increment build number in versioninfo.json and keep all version fields synchronized.
# This guarantees each build script run emits a monotonically increasing build suffix
# and that About/Explorer show the same dotted version.
function Increment-VersionBuildNumber {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "versioninfo.json not found: $Path"
    }

    # Parse current JSON as an object graph we can update in place.
    $raw = Get-Content -LiteralPath $Path -Raw
    $vi = $raw | ConvertFrom-Json

    # Read base semantic parts and bump only Build.
    $maj = [int]$vi.FixedFileInfo.FileVersion.Major
    $min = [int]$vi.FixedFileInfo.FileVersion.Minor
    $pat = [int]$vi.FixedFileInfo.FileVersion.Patch
    $bld = [int]$vi.FixedFileInfo.FileVersion.Build + 1
    if ($bld -gt 0xFFFF) {
        throw "Build counter exceeds 0xFFFF (65535). Bump Major/Minor/Patch in versioninfo.json and set Build to 0, then rebuild."
    }

    # Keep numeric file/product versions aligned.
    $vi.FixedFileInfo.FileVersion.Build = $bld
    $vi.FixedFileInfo.ProductVersion.Major = $maj
    $vi.FixedFileInfo.ProductVersion.Minor = $min
    $vi.FixedFileInfo.ProductVersion.Patch = $pat
    $vi.FixedFileInfo.ProductVersion.Build = $bld

    # Fourth component is four uppercase hex digits (e.g. 0.1.0.000C); numeric Build stays a decimal integer (0..65535).
    $ver = "$maj.$min.$pat.$($bld.ToString('X4'))"
    $vi.StringFileInfo.FileVersion = $ver
    $vi.StringFileInfo.ProductVersion = $ver

    # Write UTF-8 (no BOM) for stable diffs/tool compatibility.
    $updated = $vi | ConvertTo-Json -Depth 16
    [System.IO.File]::WriteAllText($Path, $updated + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    Write-Host "Version bumped to: $ver"
}

function Clear-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    Get-ChildItem -LiteralPath $Path -Force -ErrorAction SilentlyContinue | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName -Recurse -Force
    }
}

function Get-RepoRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$GitRoot,
        [Parameter(Mandatory = $true)][string]$AbsolutePath
    )
    $g = (Resolve-Path -LiteralPath $GitRoot).Path.TrimEnd('\', '/')
    $a = (Resolve-Path -LiteralPath $AbsolutePath).Path
    if (-not $a.StartsWith($g, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is not under git root: $AbsolutePath (root: $g)"
    }
    return $a.Substring($g.Length).TrimStart('\', '/').Replace('\', '/')
}

function Commit-BuildSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$GitRoot,
        [Parameter(Mandatory = $true)][string]$ModuleRoot,
        [Parameter(Mandatory = $true)][string]$Message
    )
    # Stage the full repository snapshot (not partial folder-only staging).
    git -C $GitRoot add -A
    if ($LASTEXITCODE -ne 0) { throw "git add -A failed" }
    git -C $GitRoot diff --cached --quiet
    if ($LASTEXITCODE -eq 0) {
        throw "Nothing staged for build commit. Expected changes under win-go after version bump and go generate."
    }
    git -C $GitRoot commit -m $Message
    if ($LASTEXITCODE -ne 0) { throw "git commit failed. Configure user.name / user.email if needed." }
}

function Read-ProductVersionInfo {
    param([Parameter(Mandatory = $true)][string]$Path)
    $vi = (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
    $maj = [int]$vi.FixedFileInfo.FileVersion.Major
    $min = [int]$vi.FixedFileInfo.FileVersion.Minor
    $pat = [int]$vi.FixedFileInfo.FileVersion.Patch
    $bld = [int]$vi.FixedFileInfo.FileVersion.Build
    return @{
        ProductVersion = "$maj.$min.$pat.$($bld.ToString('X4'))"
        Build          = $bld
        BuildPadded    = $bld.ToString('X4')
    }
}

function Prepare-CurrentBuildFolder {
    param(
        [Parameter(Mandatory = $true)][string]$WinBinRoot,
        [Parameter(Mandatory = $true)][string]$CurrentDir
    )
    New-Item -ItemType Directory -Force -Path $WinBinRoot | Out-Null
    New-Item -ItemType Directory -Force -Path $CurrentDir | Out-Null

    $existingItems = @(Get-ChildItem -LiteralPath $CurrentDir -Force -ErrorAction SilentlyContinue)
    if ($existingItems.Count -eq 0) {
        return
    }

    $dateMarker = Get-ChildItem -LiteralPath $CurrentDir -File -Filter "*.date" -Force -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $dateMarker) {
        Write-Host "No .date marker in current; wiping current folder."
        Clear-DirectoryContents -Path $CurrentDir
        return
    }

    $stampName = [System.IO.Path]::GetFileNameWithoutExtension($dateMarker.Name)
    $archiveDir = Join-Path $WinBinRoot $stampName
    if (Test-Path -LiteralPath $archiveDir) {
        Write-Host "Archive folder already exists ($archiveDir); wiping current folder."
        Clear-DirectoryContents -Path $CurrentDir
        return
    }

    New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null
    foreach ($item in $existingItems) {
        Move-Item -LiteralPath $item.FullName -Destination $archiveDir -Force
    }
    Clear-DirectoryContents -Path $CurrentDir
}

$GitRoot = $null
$walkDir = (Resolve-Path -LiteralPath $ModuleRoot).Path
while ($walkDir) {
    if (Test-Path -LiteralPath (Join-Path $walkDir ".git")) {
        $GitRoot = $walkDir
        break
    }
    $parentDir = Split-Path -Path $walkDir -Parent
    if ($parentDir -eq $walkDir) { break }
    $walkDir = $parentDir
}
$WinBinRoot = Join-Path $ShitwiperRoot "bin\win"

if (-not $GitRoot) {
    Write-Warning "No Git repository found above win-go; skipping pre-build commit and trace (branch/commit unknown)."
}

# Bump version before go:generate so generated .syso embeds updated version metadata.
Increment-VersionBuildNumber -Path $VersionInfoPath

$verInfo = Read-ProductVersionInfo -Path $VersionInfoPath
$productVer = $verInfo.ProductVersion
$buildNum = $verInfo.Build
$buildPadded = $verInfo.BuildPadded

$branch = "unknown"
$commit = "unknown"
$commitShort = "unknown"

# About artwork source in repository root assets.
$Src = Join-Path $ModuleRoot "..\assets\art\about-bunny.png"
# Embedded-art destination inside win-go module for //go:embed.
$EmbedDst = Join-Path $ModuleRoot "assets\art\about-bunny.png"
New-Item -ItemType Directory -Force -Path (Split-Path $EmbedDst) | Out-Null
# Use checked-in art when available; otherwise generate placeholder art.
if (Test-Path -LiteralPath $Src) {
    Copy-Item -LiteralPath $Src -Destination $EmbedDst -Force
} else {
    & $go -C $ModuleRoot run ./tools/genaboutpng
}

# Regenerate toolbar icon assets used by the application.
& $go -C $ModuleRoot run ./tools/genicons
# Run go:generate directives (includes goversioninfo resource generation).
& $go -C $ModuleRoot generate .

if ($GitRoot) {
    Commit-BuildSnapshot -GitRoot $GitRoot -ModuleRoot $ModuleRoot -Message "build: version $productVer"
    $commit = (git -C $GitRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "git rev-parse HEAD failed" }
    $branch = (git -C $GitRoot rev-parse --abbrev-ref HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "git rev-parse --abbrev-ref HEAD failed" }
    $commitShort = (git -C $GitRoot rev-parse --short=7 HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "git rev-parse --short HEAD failed" }
}

# Build output target: <Shitwiper>/bin/win/current (relative to repo layout: win-go is under codebase)
$OutDir = Join-Path $WinBinRoot "current"
$Exe = Join-Path $OutDir "WhatToWipe.exe"

Prepare-CurrentBuildFolder -WinBinRoot $WinBinRoot -CurrentDir $OutDir
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# Pure Go + GUI subsystem + no VCS path embed.
# Keep symbols in release builds; heavily stripped binaries are more likely to trigger generic AV heuristics.
# CGO disabled for reproducible static-style Windows build in this project.
$env:CGO_ENABLED = "0"
& $go -C $ModuleRoot build -trimpath -buildvcs=false -ldflags "-H windowsgui" -o $Exe .
# Cleanup local environment mutation even if caller reuses this shell.
Remove-Item Env:CGO_ENABLED -ErrorAction SilentlyContinue
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "go build did not produce: $Exe"
}

# Optional Authenticode signing (recommended for lower AV false-positive risk).
# Enable by setting:
#   WHATTOWIPE_SIGNTOOL = full path to signtool.exe
#   WHATTOWIPE_SIGN_ARGS = additional args before target exe (certificate selection, timestamp server, etc.)
if ($env:WHATTOWIPE_SIGNTOOL) {
    $signTool = $env:WHATTOWIPE_SIGNTOOL
    $signArgs = @("sign")
    if ($env:WHATTOWIPE_SIGN_ARGS) {
        $signArgs += $env:WHATTOWIPE_SIGN_ARGS -split "\s+"
    }
    $signArgs += $Exe
    & $signTool @signArgs
}

# Keep version template alongside payload; installer scripts can consume this if needed.
Copy-WithRetry -Source $VersionInfoPath -Destination (Join-Path $OutDir "versioninfo.json")

# Marker / archive folder stem: yyyy-MM-dd_HH-mm_000C (four hex digits, uppercase build suffix in name).
# Marker file body records the same so the build is visible without parsing the filename.
$folderTime = Get-Date -Format "yyyy-MM-dd_HH-mm"
$folderStem = "${folderTime}_${buildPadded}"
$Marker = Join-Path $OutDir ($folderStem + ".date")
$markerBody = "productVersion=$productVer`nbuild=$buildNum`nbuildPadded=$buildPadded`nfolderStem=$folderStem`n"
[System.IO.File]::WriteAllText($Marker, $markerBody, [System.Text.UTF8Encoding]::new($false))

$metaPath = Join-Path $OutDir "build-meta.json"
@{
    build          = $buildNum
    buildPadded    = $buildPadded
    folderStem     = $folderStem
    productVersion = $productVer
    branch         = $branch
    commit         = $commit
    commitShort    = $commitShort
    timeUtc        = (Get-Date).ToUniversalTime().ToString("o")
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $metaPath -Encoding utf8

$historyDir = Join-Path $CodebaseRoot "docs\history"
$historyPath = Join-Path $historyDir "builds.txt"
if (-not (Test-Path -LiteralPath $historyDir)) {
    New-Item -ItemType Directory -Force -Path $historyDir | Out-Null
}
if (-not (Test-Path -LiteralPath $historyPath)) {
    $header = "version`tbranch`tcommit_full`tcommit_short" + [Environment]::NewLine
    [System.IO.File]::WriteAllText($historyPath, $header, [System.Text.UTF8Encoding]::new($false))
}
$histLine = ($productVer + "`t" + $branch + "`t" + $commit + "`t" + $commitShort + [Environment]::NewLine)
[System.IO.File]::AppendAllText($historyPath, $histLine, [System.Text.UTF8Encoding]::new($false))

if ($GitRoot) {
    git -C $GitRoot add -A
    if ($LASTEXITCODE -ne 0) { throw "git add -A failed" }
    git -C $GitRoot commit -m "docs: append build $productVer to history"
    if ($LASTEXITCODE -ne 0) { throw "git commit (build history) failed" }
}

# Final summary for quick operator verification.
Write-Host "Built:  $Exe"
Write-Host "Version info: " (Join-Path $OutDir "versioninfo.json")
Write-Host "Marker: $Marker"
Write-Host "Build meta: $metaPath"
Write-Host "History: $historyPath"
