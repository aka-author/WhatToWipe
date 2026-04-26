$ErrorActionPreference = "Stop"
$here = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$root = $null
$d = $here
while ($d) {
    if (Test-Path -LiteralPath (Join-Path $d ".git")) {
        $root = $d
        break
    }
    $parent = Split-Path -Path $d -Parent
    if ($parent -eq $d) { break }
    $d = $parent
}
if (-not $root) {
    Write-Error "No Git repository found walking up from: $here"
}
Push-Location -LiteralPath $root
try {
    git config core.hooksPath .githooks
    Write-Host "Set core.hooksPath to .githooks in: $root"
    Write-Host "Hooks in .githooks will run for this clone."
} finally {
    Pop-Location
}
