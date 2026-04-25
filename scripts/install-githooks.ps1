$ErrorActionPreference = "Stop"
$root = git rev-parse --show-toplevel 2>$null
if (-not $root) {
    Write-Error "Run this from inside a Git repository."
}
Set-Location -LiteralPath $root
git config core.hooksPath .githooks
Write-Host "Set core.hooksPath to .githooks in: $root"
Write-Host "Hooks in .githooks will run for this clone (e.g. post-commit on branch pub)."
