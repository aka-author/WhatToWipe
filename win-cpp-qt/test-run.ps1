param(
    [string]$BinDir = "",
    [switch]$StaticQt
)

$ErrorActionPreference = "Stop"

if (-not $BinDir) {
    $codebase = Split-Path $PSScriptRoot -Parent
    $root = Split-Path $codebase -Parent
    $BinDir = Join-Path $root "bin\win\current"
}
$BinDir = (Resolve-Path -LiteralPath $BinDir).Path
$exe = Join-Path $BinDir "EraseAndRewrite.exe"

if (-not (Test-Path -LiteralPath $exe)) {
    throw "Missing executable: $exe"
}

if (-not $StaticQt) {
    $qtDll = Join-Path $BinDir "Qt6Core.dll"
    if (-not (Test-Path -LiteralPath $qtDll)) {
        $StaticQt = $true
    }
}

$required = @("EraseAndRewrite.exe")
if (-not $StaticQt) {
    $required += @(
        "Qt6Core.dll",
        "Qt6Gui.dll",
        "Qt6Widgets.dll",
        "libstdc++-6.dll",
        "libgcc_s_seh-1.dll",
        "libwinpthread-1.dll",
        (Join-Path "platforms" "qwindows.dll")
    )
}
foreach ($rel in $required) {
    $full = Join-Path $BinDir $rel
    if (-not (Test-Path -LiteralPath $full)) {
        throw "Missing required deploy file: $rel"
    }
}

$objdump = Get-Command objdump -ErrorAction SilentlyContinue
if (-not $objdump) {
    $guesses = @(
        "C:\cpp\qt\Tools\mingw1310_64\bin\objdump.exe",
        "C:\cpp\gnu\bin\objdump.exe"
    )
    foreach ($g in $guesses) {
        if (Test-Path -LiteralPath $g) {
            $objdump = Get-Command $g
            break
        }
    }
}
if ($objdump) {
    $importLines = & $objdump.Source -p $exe | Select-String "^\s+DLL Name:"
    $bad = @("libstdc++-6.dll", "libgcc_s_seh-1.dll")
    foreach ($b in $bad) {
        if ($importLines | Where-Object { $_.Line -like "*$b*" }) {
            throw "Executable still imports $b; static runtime link failed."
        }
    }
    if ($StaticQt) {
        $qtDlls = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Svg.dll")
        foreach ($q in $qtDlls) {
            if ($importLines | Where-Object { $_.Line -like "*$q*" }) {
                throw "Static build still imports $q."
            }
        }
    }
}

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exe
$psi.WorkingDirectory = $BinDir
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $false
$p = [System.Diagnostics.Process]::Start($psi)
if (-not $p) {
    throw "Failed to start $exe"
}

Start-Sleep -Seconds 4
if ($p.HasExited) {
    throw "EraseAndRewrite exited early with code $($p.ExitCode)"
}

$hasWindow = $false
$deadline = (Get-Date).AddSeconds(8)
while ((Get-Date) -lt $deadline) {
    $p.Refresh()
    if ($p.HasExited) {
        throw "EraseAndRewrite exited during window check with code $($p.ExitCode)"
    }
    if ($p.MainWindowHandle -ne [IntPtr]::Zero) {
        $hasWindow = $true
        break
    }
    Start-Sleep -Milliseconds 250
}

if (-not $hasWindow) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    throw "EraseAndRewrite started but no main window appeared."
}

Stop-Process -Id $p.Id -Force
Write-Host "RUN OK: $exe"
