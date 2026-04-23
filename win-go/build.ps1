$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
$go = if (Test-Path "C:\Program Files\Go\bin\go.exe") { "C:\Program Files\Go\bin\go.exe" } else { "go" }

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

$Src = Join-Path $PSScriptRoot "..\assets\art\about-bunny.png"
$EmbedDst = Join-Path $PSScriptRoot "assets\art\about-bunny.png"
New-Item -ItemType Directory -Force -Path (Split-Path $EmbedDst) | Out-Null
if (Test-Path -LiteralPath $Src) {
    Copy-Item -LiteralPath $Src -Destination $EmbedDst -Force
} else {
    & $go run ./tools/genaboutpng
}

& $go run ./tools/genicons
& $go generate .

# Single obvious folder under this module (not repo-root /bin/, which may be gitignored).
$OutDir = Join-Path $PSScriptRoot "dist"
$Exe = Join-Path $OutDir "WhatToWipe.exe"
# Optional second copy for install tree: <Shitwiper>/bin/win/
$CodebaseRoot = Split-Path $PSScriptRoot -Parent
$ShitwiperRoot = Split-Path $CodebaseRoot -Parent
$ProperDir = Join-Path $ShitwiperRoot "bin\win"
$ProperBin = Join-Path $ProperDir "WhatToWipe.exe"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path $ProperDir | Out-Null

# Pure Go + GUI subsystem + no VCS path embed.
# Keep symbols in release builds; heavily stripped binaries are more likely to trigger generic AV heuristics.
$env:CGO_ENABLED = "0"
& $go build -trimpath -buildvcs=false -ldflags "-H windowsgui" -o $Exe .
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

& $go run ./tools/seedconfig $OutDir
Copy-WithRetry -Source $Exe -Destination $ProperBin
& $go run ./tools/seedconfig $ProperDir
if (-not (Test-Path -LiteralPath $ProperBin)) {
    throw "Copy failed; expected: $ProperBin"
}
$Cfg = Join-Path $OutDir "WhatToWipe.config.txt"
Write-Host "Built:  $Exe"
Write-Host "Config: $Cfg"
Write-Host "Copied: $ProperBin"
