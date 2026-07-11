param(
    [string]$SourceDir = ""
)

$ErrorActionPreference = "Stop"

# Redeploy Qt runtime DLLs/plugins into an existing bin\win\current folder without rebuilding.
$ScriptDir = $PSScriptRoot
$CodebaseRoot = Split-Path $ScriptDir -Parent
$ShitwiperRoot = Split-Path $CodebaseRoot -Parent

if (-not $SourceDir) {
    $SourceDir = Join-Path $ShitwiperRoot "bin\win\current"
}
$SourceDir = (Resolve-Path -LiteralPath $SourceDir).Path

$exe = Join-Path $SourceDir "EraseAndRewrite.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "EraseAndRewrite.exe not found in $SourceDir"
}

$deployScript = Join-Path $CodebaseRoot "win-cpp-qt\deploy-standalone.ps1"
if (-not (Test-Path -LiteralPath $deployScript)) {
    throw "deploy-standalone.ps1 not found: $deployScript"
}

& $deployScript -TargetDir $SourceDir -ExePath $exe
if ($LASTEXITCODE -ne 0) { throw "deploy failed" }

Write-Host "Redeployed standalone runtime to: $SourceDir"
