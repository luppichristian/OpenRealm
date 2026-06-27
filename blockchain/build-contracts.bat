@echo off
setlocal
cd /d "%~dp0"

echo [blockchain] building Solidity artifacts...
call npm run build
if errorlevel 1 goto :fail

echo.
echo [blockchain] build completed successfully.
exit /b 0

:fail
echo.
echo [blockchain] build failed.
exit /b 1
