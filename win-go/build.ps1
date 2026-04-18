$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
$go = if (Test-Path "C:\Program Files\Go\bin\go.exe") { "C:\Program Files\Go\bin\go.exe" } else { "go" }

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

$LocalBin = Join-Path $PSScriptRoot "bin\WhatToWipe.exe"
# Install tree: <Shitwiper>/codebase/win-go/build.ps1 -> exe lands in <Shitwiper>/bin/win/
$CodebaseRoot = Split-Path $PSScriptRoot -Parent
$ShitwiperRoot = Split-Path $CodebaseRoot -Parent
$ProperBin = Join-Path $ShitwiperRoot "bin\win\WhatToWipe.exe"

New-Item -ItemType Directory -Force -Path (Split-Path $LocalBin) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $ProperBin) | Out-Null

& $go build -trimpath -ldflags "-H windowsgui" -o $LocalBin .
if (-not (Test-Path -LiteralPath $LocalBin)) {
    throw "go build did not produce: $LocalBin"
}
Copy-Item -LiteralPath $LocalBin -Destination $ProperBin -Force
if (-not (Test-Path -LiteralPath $ProperBin)) {
    throw "Copy failed; expected: $ProperBin"
}
Write-Host "Built:  $LocalBin"
Write-Host "Copied: $ProperBin"
