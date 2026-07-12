# Restore versioninfo.json, build-meta.json, and a .date marker beside an existing exe
# when a build or manual deploy stopped before Write-BuildOutputMetadata ran.
param(
    [string]$BinDir,
    [string]$VersionInfoPath
)

$ErrorActionPreference = "Stop"
$ModuleRoot = Split-Path $PSScriptRoot -Parent
if (-not $BinDir) {
    $shitwiperRoot = Split-Path (Split-Path $ModuleRoot -Parent) -Parent
    $BinDir = Join-Path $shitwiperRoot "bin\win\current"
}
if (-not $VersionInfoPath) {
    $VersionInfoPath = Join-Path $ModuleRoot "versioninfo.json"
}

$exe = Join-Path $BinDir "EraseAndRewrite.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Exe not found: $exe"
}
if (-not (Test-Path -LiteralPath $VersionInfoPath)) {
    throw "versioninfo.json not found: $VersionInfoPath"
}

$vi = Get-Content -LiteralPath $VersionInfoPath -Raw | ConvertFrom-Json
$maj = [int]$vi.FixedFileInfo.FileVersion.Major
$min = [int]$vi.FixedFileInfo.FileVersion.Minor
$pat = [int]$vi.FixedFileInfo.FileVersion.Patch
$bld = [int]$vi.FixedFileInfo.FileVersion.Build
$productVer = "$maj.$min.$pat.$($bld.ToString('X4'))"
$buildPadded = $bld.ToString('X4')

$gitRoot = $null
$walkDir = (Resolve-Path -LiteralPath $ModuleRoot).Path
while ($walkDir) {
    if (Test-Path -LiteralPath (Join-Path $walkDir ".git")) {
        $gitRoot = $walkDir
        break
    }
    $parent = Split-Path $walkDir -Parent
    if ($parent -eq $walkDir) { break }
    $walkDir = $parent
}

$branch = "unknown"
$commit = "unknown"
$commitShort = "unknown"
if ($gitRoot) {
    $branch = (git -C $gitRoot rev-parse --abbrev-ref HEAD).Trim()
    $commit = (git -C $gitRoot rev-parse HEAD).Trim()
    $commitShort = (git -C $gitRoot rev-parse --short=7 HEAD).Trim()
}

Copy-Item -LiteralPath $VersionInfoPath -Destination (Join-Path $BinDir "versioninfo.json") -Force

$folderTime = Get-Date -Format "yyyy-MM-dd_HH-mm"
$folderStem = "${folderTime}_${buildPadded}"
$marker = Join-Path $BinDir ($folderStem + ".date")
$markerBody = "productVersion=$productVer`nbuild=$bld`nbuildPadded=$buildPadded`nfolderStem=$folderStem`n"
[System.IO.File]::WriteAllText($marker, $markerBody, [System.Text.UTF8Encoding]::new($false))

$metaPath = Join-Path $BinDir "build-meta.json"
@{
    build          = $bld
    buildPadded    = $buildPadded
    folderStem     = $folderStem
    productVersion = $productVer
    branch         = $branch
    commit         = $commit
    commitShort    = $commitShort
    timeUtc        = (Get-Date).ToUniversalTime().ToString("o")
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $metaPath -Encoding utf8

Write-Host "Restored metadata for $productVer in $BinDir"
Write-Host "Marker: $marker"
Write-Host "Meta: $metaPath"
