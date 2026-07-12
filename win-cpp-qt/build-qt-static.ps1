param(
    [string]$QtVersion = "6.10.3",
    [string]$SharedQtPrefix = "",
    [string]$StaticQtPrefix = "",
    [string]$SourceRoot = "C:\cpp\qt-src",
    [switch]$SkipDownload,
    [switch]$SkipQtbase,
    [switch]$SkipQtsvg
)

$ErrorActionPreference = "Stop"

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

function Test-StaticQtKit {
    param([Parameter(Mandatory = $true)][string]$Prefix)
    $coreTargets = Join-Path $Prefix "lib\cmake\Qt6Core\Qt6CoreTargets.cmake"
    if (-not (Test-Path -LiteralPath $coreTargets)) {
        return $false
    }
    $text = Get-Content -LiteralPath $coreTargets -Raw
    return ($text -match "add_library\(Qt6::Core STATIC IMPORTED\)")
}

if (-not $SharedQtPrefix) {
    $guesses = @(
        "C:\cpp\qt\$QtVersion\mingw_64",
        "C:\Qt\$QtVersion\mingw_64"
    )
    foreach ($p in $guesses) {
        if (Test-Path -LiteralPath $p) {
            $SharedQtPrefix = $p
            break
        }
    }
}
if (-not $SharedQtPrefix) {
    throw "Shared Qt prefix not found. Pass -SharedQtPrefix."
}
$SharedQtPrefix = (Resolve-Path -LiteralPath $SharedQtPrefix).Path

if (-not $StaticQtPrefix) {
    $StaticQtPrefix = Join-Path (Split-Path $SharedQtPrefix -Parent) "mingw_64_static"
}

if (Test-StaticQtKit -Prefix $StaticQtPrefix) {
    Write-Host "Static Qt kit already present: $StaticQtPrefix"
    return
}

$mingwRoot = Resolve-QtMingwRoot -QtPrefix $SharedQtPrefix
$gcc = Join-Path $mingwRoot "bin\gcc.exe"
$gxx = Join-Path $mingwRoot "bin\g++.exe"
$windres = Join-Path $mingwRoot "bin\windres.exe"
if (-not (Test-Path -LiteralPath $gxx)) {
    throw "MinGW g++ not found: $gxx"
}
$gccCmake = ($gcc -replace '\\', '/')
$gxxCmake = ($gxx -replace '\\', '/')
$windresCmake = ($windres -replace '\\', '/')
$staticPrefixCmake = ($StaticQtPrefix -replace '\\', '/')

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) { throw "cmake not found in PATH" }
$ninja = Get-Command ninja -ErrorAction SilentlyContinue
if (-not $ninja) { throw "ninja not found in PATH" }

$srcVersionDir = Join-Path $SourceRoot "$QtVersion\Src"
$qtbaseSrc = Join-Path $srcVersionDir "qtbase"
$qtsvgSrc = Join-Path $srcVersionDir "qtsvg"

if (-not $SkipDownload) {
    if (-not (Test-Path -LiteralPath $qtbaseSrc) -or -not (Test-Path -LiteralPath $qtsvgSrc)) {
        New-Item -ItemType Directory -Force -Path $SourceRoot | Out-Null
        Write-Host "Downloading Qt $QtVersion source (qtbase, qtsvg) to $SourceRoot ..."
        & python -m aqt install-src windows $QtVersion -O $SourceRoot --archives qtbase qtsvg
        if ($LASTEXITCODE -ne 0) { throw "aqt install-src failed" }
    }
}

if (-not (Test-Path -LiteralPath $qtbaseSrc)) {
    throw "qtbase source missing: $qtbaseSrc"
}
if (-not (Test-Path -LiteralPath $qtsvgSrc)) {
    throw "qtsvg source missing: $qtsvgSrc"
}

$env:PATH = "$(Join-Path $mingwRoot 'bin');$env:PATH"

$commonCmake = @(
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_INSTALL_PREFIX=$staticPrefixCmake",
    "-DCMAKE_C_COMPILER=$gccCmake",
    "-DCMAKE_CXX_COMPILER=$gxxCmake",
    "-DCMAKE_RC_COMPILER=$windresCmake",
    "-DCMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders=TRUE",
    "-DBUILD_SHARED_LIBS=OFF",
    "-DQT_BUILD_EXAMPLES=OFF",
    "-DQT_BUILD_TESTS=OFF",
    "-DQT_BUILD_TOOLS=ON"
)

if (-not $SkipQtbase) {
    $qtbaseBuild = Join-Path $qtbaseSrc "build-static"
    if (Test-Path -LiteralPath $qtbaseBuild) {
        Remove-Item -LiteralPath $qtbaseBuild -Recurse -Force
    }
    Write-Host "Configuring static qtbase ..."
    & cmake -S $qtbaseSrc -B $qtbaseBuild @commonCmake
    if ($LASTEXITCODE -ne 0) { throw "qtbase cmake configure failed" }
    Write-Host "Building static qtbase (this takes a while) ..."
    & cmake --build $qtbaseBuild --parallel
    if ($LASTEXITCODE -ne 0) { throw "qtbase build failed" }
    Write-Host "Installing static qtbase to $StaticQtPrefix ..."
    & cmake --install $qtbaseBuild
    if ($LASTEXITCODE -ne 0) { throw "qtbase install failed" }
}

if (-not (Test-Path -LiteralPath (Join-Path $StaticQtPrefix "lib\cmake\Qt6Core"))) {
    throw "qtbase install did not produce Qt6Core cmake package under $StaticQtPrefix"
}

if (-not $SkipQtsvg) {
    $qtsvgBuild = Join-Path $qtsvgSrc "build-static"
    if (Test-Path -LiteralPath $qtsvgBuild) {
        Remove-Item -LiteralPath $qtsvgBuild -Recurse -Force
    }
    Write-Host "Configuring static qtsvg ..."
  $qtsvgArgs = @(
        "-S", $qtsvgSrc,
        "-B", $qtsvgBuild
    ) + $commonCmake + @("-DCMAKE_PREFIX_PATH=$staticPrefixCmake")
    & cmake @qtsvgArgs
    if ($LASTEXITCODE -ne 0) { throw "qtsvg cmake configure failed" }
    Write-Host "Building static qtsvg ..."
    & cmake --build $qtsvgBuild --parallel
    if ($LASTEXITCODE -ne 0) { throw "qtsvg build failed" }
    Write-Host "Installing static qtsvg ..."
    & cmake --install $qtsvgBuild
    if ($LASTEXITCODE -ne 0) { throw "qtsvg install failed" }
}

if (-not (Test-StaticQtKit -Prefix $StaticQtPrefix)) {
    throw "Static Qt kit verification failed under $StaticQtPrefix"
}

Write-Host "Static Qt kit ready: $StaticQtPrefix"
