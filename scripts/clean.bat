@echo off
set ROOT=%~dp0..
cd /d "%ROOT%"

echo Removing build\ and dist\ ...
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist

echo Done.
