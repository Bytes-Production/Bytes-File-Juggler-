@echo off
setlocal enabledelayedexpansion

rem Builds Bytes in Release configuration.
rem Usage: scripts\build.bat [path-to-Qt6-cmake-dir]
rem   e.g. scripts\build.bat C:\Qt\6.8.1\msvc2022_64

set ROOT=%~dp0..
cd /d "%ROOT%"

set QT_DIR=%~1
if "%QT_DIR%"=="" set QT_DIR=C:\Qt\6.8.1\msvc2022_64

echo === Configuring Bytes (Release) ===
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 goto :error

echo === Building Bytes (Release) ===
cmake --build build --config Release
if errorlevel 1 goto :error

echo.
echo Build succeeded. Bytes.exe is in the dist\ folder.
exit /b 0

:error
echo.
echo Build FAILED.
exit /b 1
