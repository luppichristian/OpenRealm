@echo off
setlocal
cd /d "%~dp0"

echo [blockchain] installing npm dependencies...
call npm install
if errorlevel 1 goto :fail

echo.
echo [blockchain] dependencies installed successfully.
exit /b 0

:fail
echo.
echo [blockchain] npm install failed.
exit /b 1
