$ErrorActionPreference = "Stop"

$ModuleRoot = $PSScriptRoot
$CodebaseRoot = Split-Path $ModuleRoot -Parent
$ShitwiperRoot = Split-Path $CodebaseRoot -Parent
$ToolsRoot = Join-Path $ShitwiperRoot "tools"
$WinBinRoot = Join-Path $ShitwiperRoot "bin\win"
$OutDir = Join-Path $WinBinRoot "current"
$BuildDir = Join-Path $ModuleRoot "build"
$VersionInfoPath = Join-Path $ModuleRoot "versioninfo.json"
$AppRcPath = Join-Path $ModuleRoot "resources\app.rc"
$Exe = Join-Path $OutDir "EraseAndRewrite.exe"

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

function Increment-VersionBuildNumber {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "versioninfo.json not found: $Path"
    }
    $vi = (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
    $maj = [int]$vi.FixedFileInfo.FileVersion.Major
    $min = [int]$vi.FixedFileInfo.FileVersion.Minor
    $pat = [int]$vi.FixedFileInfo.FileVersion.Patch
    $bld = [int]$vi.FixedFileInfo.FileVersion.Build + 1
    if ($bld -gt 0xFFFF) {
        throw "Build counter exceeds 0xFFFF (65535). Bump Major/Minor/Patch in versioninfo.json and set Build to 0, then rebuild."
    }
    $vi.FixedFileInfo.FileVersion.Build = $bld
    $vi.FixedFileInfo.ProductVersion.Major = $maj
    $vi.FixedFileInfo.ProductVersion.Minor = $min
    $vi.FixedFileInfo.ProductVersion.Patch = $pat
    $vi.FixedFileInfo.ProductVersion.Build = $bld
    $ver = "$maj.$min.$pat.$($bld.ToString('X4'))"
    $vi.StringFileInfo.FileVersion = $ver
    $vi.StringFileInfo.ProductVersion = $ver
    $updated = $vi | ConvertTo-Json -Depth 16
    [System.IO.File]::WriteAllText($Path, $updated + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    Write-Host "Version bumped to: $ver"
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
        Major          = $maj
        Minor          = $min
        Patch          = $pat
    }
}

function Generate-AppIcon {
    param(
        [Parameter(Mandatory = $true)][string]$CodebaseRoot,
        [Parameter(Mandatory = $true)][string]$IconDestination
    )
    $goRoot = Join-Path $CodebaseRoot "win-go"
    $genicons = Join-Path $goRoot "tools\genicons"
    if (-not (Test-Path -LiteralPath $genicons)) {
        throw "genicons tool not found: $genicons"
    }
    $go = Get-Command go -ErrorAction SilentlyContinue
    if (-not $go) {
        throw "go not found in PATH (required to generate app.ico)"
    }
    & $go.Source -C $goRoot run ./tools/genicons
    if ($LASTEXITCODE -ne 0) { throw "genicons failed" }
    $srcIco = Join-Path $goRoot "icons\app.ico"
    if (-not (Test-Path -LiteralPath $srcIco)) {
        throw "genicons did not produce icons/app.ico"
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $IconDestination) | Out-Null
    Copy-Item -LiteralPath $srcIco -Destination $IconDestination -Force
}

function Update-AppResourceFromVersionInfo {
    param(
        [Parameter(Mandatory = $true)][string]$VersionInfoPath,
        [Parameter(Mandatory = $true)][string]$AppRcPath
    )
    $vi = (Get-Content -LiteralPath $VersionInfoPath -Raw | ConvertFrom-Json)
    $maj = [int]$vi.FixedFileInfo.FileVersion.Major
    $min = [int]$vi.FixedFileInfo.FileVersion.Minor
    $pat = [int]$vi.FixedFileInfo.FileVersion.Patch
    $bld = [int]$vi.FixedFileInfo.FileVersion.Build
    $fileVer = $vi.StringFileInfo.FileVersion
    $productVer = $vi.StringFileInfo.ProductVersion
    $company = $vi.StringFileInfo.CompanyName
    $description = $vi.StringFileInfo.FileDescription
    $internal = $vi.StringFileInfo.InternalName
    $copyright = $vi.StringFileInfo.LegalCopyright
    $orig = $vi.StringFileInfo.OriginalFilename
    $product = $vi.StringFileInfo.ProductName

    $rc = @"
#include <winver.h>

1 ICON "app.ico"

VS_VERSION_INFO VERSIONINFO
 FILEVERSION $maj,$min,$pat,$bld
 PRODUCTVERSION $maj,$min,$pat,$bld
 FILEFLAGSMASK VS_FFI_FILEFLAGSMASK
 FILEFLAGS 0x0L
 FILEOS VOS__WINDOWS32
 FILETYPE VFT_APP
 FILESUBTYPE VFT2_UNKNOWN
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName", "$company"
            VALUE "FileDescription", "$description"
            VALUE "FileVersion", "$fileVer"
            VALUE "InternalName", "$internal"
            VALUE "LegalCopyright", "$copyright"
            VALUE "OriginalFilename", "$orig"
            VALUE "ProductName", "$product"
            VALUE "ProductVersion", "$productVer"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
"@
    New-Item -ItemType Directory -Force -Path (Split-Path $AppRcPath) | Out-Null
    [System.IO.File]::WriteAllText($AppRcPath, $rc, [System.Text.UTF8Encoding]::new($false))
}

function Clear-DirectoryContents {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    Get-ChildItem -LiteralPath $Path -Force -ErrorAction SilentlyContinue | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName -Recurse -Force
    }
}

function Relocate-ModuleRootExeArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$ModuleRoot,
        [Parameter(Mandatory = $true)][string]$ToolsRoot
    )
    New-Item -ItemType Directory -Force -Path $ToolsRoot | Out-Null
    $rootExeFiles = @(Get-ChildItem -LiteralPath $ModuleRoot -File -Filter "*.exe" -ErrorAction SilentlyContinue)
    foreach ($exeFile in $rootExeFiles) {
        $dst = Join-Path $ToolsRoot $exeFile.Name
        Move-Item -LiteralPath $exeFile.FullName -Destination $dst -Force
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
    if ($existingItems.Count -eq 0) { return }

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

function Commit-BuildSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$GitRoot,
        [Parameter(Mandatory = $true)][string]$Message
    )
    git -C $GitRoot add -A
    if ($LASTEXITCODE -ne 0) { throw "git add -A failed" }
    git -C $GitRoot diff --cached --quiet
    if ($LASTEXITCODE -eq 0) {
        throw "Nothing staged for build commit."
    }
    git -C $GitRoot commit -m $Message
    if ($LASTEXITCODE -ne 0) { throw "git commit failed" }
}

function Resolve-QtMingwRoot {
    param([Parameter(Mandatory = $true)][string]$QtPrefix)
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

if (-not $GitRoot) {
    Write-Warning "No Git repository found above win-cpp-qt; skipping pre-build commit and trace."
}

Relocate-ModuleRootExeArtifacts -ModuleRoot $ModuleRoot -ToolsRoot $ToolsRoot
Increment-VersionBuildNumber -Path $VersionInfoPath
$AppIconPath = Join-Path $ModuleRoot "resources\app.ico"
Generate-AppIcon -CodebaseRoot $CodebaseRoot -IconDestination $AppIconPath
Update-AppResourceFromVersionInfo -VersionInfoPath $VersionInfoPath -AppRcPath $AppRcPath

$verInfo = Read-ProductVersionInfo -Path $VersionInfoPath
$productVer = $verInfo.ProductVersion
$buildNum = $verInfo.Build
$buildPadded = $verInfo.BuildPadded

$branch = "unknown"
$commit = "unknown"
$commitShort = "unknown"

if ($GitRoot) {
    Commit-BuildSnapshot -GitRoot $GitRoot -Message "build: version $productVer"
    $commit = (git -C $GitRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "git rev-parse HEAD failed" }
    $branch = (git -C $GitRoot rev-parse --abbrev-ref HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "git rev-parse --abbrev-ref HEAD failed" }
    $commitShort = (git -C $GitRoot rev-parse --short=7 HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "git rev-parse --short HEAD failed" }
}

Prepare-CurrentBuildFolder -WinBinRoot $WinBinRoot -CurrentDir $OutDir
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) { throw "cmake not found in PATH" }

$qtPrefix = $env:CMAKE_PREFIX_PATH
if (-not $qtPrefix) {
    $qtGuess = @(
        "C:\cpp\qt\6.10.3\mingw_64",
        "C:\Qt\6.10.3\mingw_64",
        "C:\Qt\6.8.0\mingw_64",
        "C:\Qt\6.7.3\mingw_64",
        "C:\Qt\6.6.3\mingw_64"
    )
    foreach ($p in $qtGuess) {
        if (Test-Path -LiteralPath $p) {
            $qtPrefix = $p
            break
        }
    }
}
if (-not $qtPrefix) {
    throw "Qt prefix not found. Set CMAKE_PREFIX_PATH."
}
$qtPrefix = (Resolve-Path -LiteralPath $qtPrefix).Path
$mingwRoot = Resolve-QtMingwRoot -QtPrefix $qtPrefix
$toolchainFile = Join-Path $ModuleRoot "toolchain-qt-mingw.cmake"
$compilerMarker = Join-Path $BuildDir ".qt_mingw_compiler"
if (Test-Path -LiteralPath $compilerMarker) {
    $prev = (Get-Content -LiteralPath $compilerMarker -Raw).Trim()
    if ($prev -ne $mingwRoot) {
        Write-Host "Qt MinGW toolchain changed; reconfiguring build directory."
        Remove-Item -LiteralPath $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Content -LiteralPath $compilerMarker -Value $mingwRoot -NoNewline

$env:PATH = "$(Join-Path $mingwRoot 'bin');$(Join-Path $qtPrefix 'bin');$env:PATH"
$env:QT_MINGW_ROOT = $mingwRoot
Write-Host "Using CMAKE_PREFIX_PATH=$qtPrefix"
Write-Host "Using QT_MINGW_ROOT=$mingwRoot"

$cmakeArgs = @(
    "-S", $ModuleRoot,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_PREFIX_PATH=$qtPrefix",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
    "-DQT_MINGW_ROOT=$mingwRoot"
)

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

& cmake --build $BuildDir --config Release --target EraseAndRewrite
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

$built = Join-Path $BuildDir "EraseAndRewrite.exe"
if (-not (Test-Path -LiteralPath $built)) {
    $built = Join-Path $BuildDir "Release\EraseAndRewrite.exe"
}
if (-not (Test-Path -LiteralPath $built)) {
    throw "Built executable not found under $BuildDir"
}

Copy-WithRetry -Source $built -Destination $Exe

$deployScript = Join-Path $ModuleRoot "deploy-standalone.ps1"
if (-not (Test-Path -LiteralPath $deployScript)) {
    throw "deploy-standalone.ps1 not found: $deployScript"
}
& $deployScript -TargetDir $OutDir -ExePath $Exe -QtPrefix $qtPrefix -MingwRoot $mingwRoot
if ($LASTEXITCODE -ne 0) { throw "standalone Qt deploy failed" }

$testScript = Join-Path $ModuleRoot "test-run.ps1"
if (-not (Test-Path -LiteralPath $testScript)) {
    throw "test-run.ps1 not found: $testScript"
}
& $testScript -BinDir $OutDir
if ($LASTEXITCODE -ne 0) { throw "post-build run test failed" }

if ($env:ERASE_REWRITE_SIGNTOOL) {
    $signTool = $env:ERASE_REWRITE_SIGNTOOL
    $signArgs = @("sign")
    if ($env:ERASE_REWRITE_SIGN_ARGS) {
        $signArgs += $env:ERASE_REWRITE_SIGN_ARGS -split "\s+"
    }
    $signArgs += $Exe
    & $signTool @signArgs
}

Copy-WithRetry -Source $VersionInfoPath -Destination (Join-Path $OutDir "versioninfo.json")

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

Write-Host "Built:  $Exe"
Write-Host "Version info: " (Join-Path $OutDir "versioninfo.json")
Write-Host "Marker: $Marker"
Write-Host "Build meta: $metaPath"
Write-Host "History: $historyPath"
