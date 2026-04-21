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

# Pure Go + stripped symbols + no VCS path embed: fewer generic AV heuristics than default `go build`.
$env:CGO_ENABLED = "0"
& $go build -trimpath -buildvcs=false -ldflags "-s -w -H windowsgui" -o $Exe .
Remove-Item Env:CGO_ENABLED -ErrorAction SilentlyContinue
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "go build did not produce: $Exe"
}
& $go run ./tools/seedconfig $OutDir
Copy-Item -LiteralPath $Exe -Destination $ProperBin -Force
& $go run ./tools/seedconfig $ProperDir
if (-not (Test-Path -LiteralPath $ProperBin)) {
    throw "Copy failed; expected: $ProperBin"
}
$Cfg = Join-Path $OutDir "WhatToWipe.config.txt"
Write-Host "Built:  $Exe"
Write-Host "Config: $Cfg"
Write-Host "Copied: $ProperBin"
