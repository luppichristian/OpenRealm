@echo off
setlocal
cd /d "%~dp0"

echo [blockchain] running contract tests...
call npm test
if errorlevel 1 goto :fail

echo.
echo [blockchain] tests passed.
exit /b 0

:fail
echo.
echo [blockchain] tests failed.
exit /b 1
