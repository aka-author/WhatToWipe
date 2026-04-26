@echo off
setlocal EnableExtensions

if "%~1"=="" (
  echo Usage: %~nx0 ^<SourceDir^> ^<OutputRootDir^>
  exit /b 2
)
if "%~2"=="" (
  echo Usage: %~nx0 ^<SourceDir^> ^<OutputRootDir^>
  exit /b 2
)

rem Script dir always ends with \ ; paths below do not depend on caller cwd.
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\installer\Erase & Rewrite.iss") do set "ISS_FILE=%%~fI"
if not exist "%ISS_FILE%" (
  echo Inno script not found: "%ISS_FILE%"
  exit /b 9
)

set "SOURCE_DIR=%~f1"

if not exist "%SOURCE_DIR%" (
  echo SourceDir does not exist: "%SOURCE_DIR%"
  exit /b 3
)

if not exist "%SOURCE_DIR%\Erase & Rewrite.exe" (
  echo Required file is missing: "%SOURCE_DIR%\Erase & Rewrite.exe"
  exit /b 4
)

set "OUTPUT_ROOT_DIR=%~f2"

if not exist "%OUTPUT_ROOT_DIR%" (
  mkdir "%OUTPUT_ROOT_DIR%" || exit /b 5
)

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format \"yyyy-MM-dd_HH-mm\""') do set "STAMP=%%I"
if "%STAMP%"=="" (
  echo Failed to get date-time stamp.
  exit /b 7
)
set "OUTPUT_DIR=%OUTPUT_ROOT_DIR%\%STAMP%"
if not exist "%OUTPUT_DIR%" (
  mkdir "%OUTPUT_DIR%" || exit /b 8
)

set "ISCC_EXE="
if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "ISCC_EXE=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if "%ISCC_EXE%"=="" if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC_EXE=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if "%ISCC_EXE%"=="" if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC_EXE=%ProgramFiles%\Inno Setup 6\ISCC.exe"

if "%ISCC_EXE%"=="" (
  for %%I in (ISCC.exe) do set "ISCC_EXE=%%~$PATH:I"
)

if "%ISCC_EXE%"=="" (
  echo ISCC.exe not found. Install Inno Setup 6 or add ISCC.exe to PATH.
  exit /b 6
)

echo Using SourceDir: "%SOURCE_DIR%"
echo Using OutputRootDir: "%OUTPUT_ROOT_DIR%"
echo Using OutputDir: "%OUTPUT_DIR%"
echo Using ISCC: "%ISCC_EXE%"

"%ISCC_EXE%" ^
  "/DSourceDir=%SOURCE_DIR%" ^
  "/DOutputDir=%OUTPUT_DIR%" ^
  "%ISS_FILE%"
if errorlevel 1 exit /b %errorlevel%

echo Installer build completed.
exit /b 0
