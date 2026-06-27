@echo off
setlocal
cd /d "%~dp0"

set "PRIVATE_KEY=%~1"
if "%PRIVATE_KEY%"=="" set "PRIVATE_KEY=0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"

set "OWNER_ADDRESS=%~2"
if "%OWNER_ADDRESS%"=="" set "OWNER_ADDRESS=0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266"

echo [blockchain] deploying to local Ganache...
echo [blockchain] owner=%OWNER_ADDRESS%
echo.
call npm run deploy:local -- --private-key %PRIVATE_KEY% --owner %OWNER_ADDRESS%
if errorlevel 1 goto :fail

echo.
echo [blockchain] local deployment completed. See deployments\local.json
exit /b 0

:fail
echo.
echo [blockchain] local deployment failed.
exit /b 1
