@echo off
setlocal
cd /d "%~dp0"

echo [realms/test] starting local Ganache node for the test realm...
echo [realms/test] chainId=31337
echo [realms/test] rpcUrl=http://127.0.0.1:8545
echo [realms/test] mnemonic=test test test test test test test test test test test junk
echo.
call npx ganache --wallet.mnemonic "test test test test test test test test test test test junk" --wallet.totalAccounts 10 --chain.chainId 31337 --server.port 8545
exit /b %errorlevel%
