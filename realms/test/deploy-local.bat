@echo off
setlocal
cd /d "%~dp0"

set "PRIVATE_KEY=%~1"
if "%PRIVATE_KEY%"=="" set "PRIVATE_KEY=0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"

set "OWNER_ADDRESS=%~2"
if "%OWNER_ADDRESS%"=="" set "OWNER_ADDRESS=0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266"

echo [realms/test] deploying test realm to local Ganache...
echo [realms/test] owner=%OWNER_ADDRESS%
echo.
call node deploy.js --private-key %PRIVATE_KEY% --owner %OWNER_ADDRESS%
if errorlevel 1 goto :fail

echo.
echo [realms/test] local deployment completed. See ..\..\blockchain\deployments\test.json
exit /b 0

:fail
echo.
echo [realms/test] local deployment failed.
exit /b 1
