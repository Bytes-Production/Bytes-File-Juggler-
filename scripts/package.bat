@echo off
setlocal enabledelayedexpansion

rem Deploys Qt runtime dependencies next to Bytes.exe and builds the
rem Inno Setup installer. Run scripts\build.bat first.
rem Usage: scripts\package.bat [path-to-Qt6-dir]

set ROOT=%~dp0..
cd /d "%ROOT%"

set QT_DIR=%~1
if "%QT_DIR%"=="" set QT_DIR=C:\Qt\6.8.1\msvc2022_64

if not exist "dist\Bytes.exe" (
    echo dist\Bytes.exe not found. Run scripts\build.bat first.
    exit /b 1
)

echo === Deploying Qt runtime dependencies ===
"%QT_DIR%\bin\windeployqt.exe" --release --no-translations --no-compiler-runtime dist\Bytes.exe
if errorlevel 1 goto :error

echo === Building installer (Inno Setup) ===
where iscc >nul 2>nul
if errorlevel 1 (
    echo iscc.exe ^(Inno Setup^) was not found on PATH.
    echo Install Inno Setup from https://jrsoftware.org/isinfo.php and re-run this script,
    echo or compile installer\Bytes.iss manually from the Inno Setup IDE.
    exit /b 1
)
iscc installer\Bytes.iss
if errorlevel 1 goto :error

echo.
echo Packaging succeeded:
echo   dist\Bytes.exe
echo   dist\Bytes_Setup.exe
exit /b 0

:error
echo.
echo Packaging FAILED.
exit /b 1
