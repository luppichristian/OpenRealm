@echo off
setlocal
cd /d "%~dp0"

echo [blockchain] running build + tests...
call npm run build
if errorlevel 1 goto :fail

call npm test
if errorlevel 1 goto :fail

echo.
echo [blockchain] build and tests passed.
exit /b 0

:fail
echo.
echo [blockchain] verification failed.
exit /b 1
