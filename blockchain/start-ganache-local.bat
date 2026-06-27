@echo off
setlocal
cd /d "%~dp0"

echo [blockchain] starting local Ganache node...
echo [blockchain] chainId=31337
echo [blockchain] mnemonic=test test test test test test test test test test test junk
echo.
call npx ganache --wallet.mnemonic "test test test test test test test test test test test junk" --wallet.totalAccounts 10 --chain.chainId 31337 --server.port 8545
exit /b %errorlevel%
