@echo off
setlocal
cd /d "%~dp0"
echo [realms/main] deploying main realm using realms\main\realm.json...
call node deploy.js %*
exit /b %errorlevel%
