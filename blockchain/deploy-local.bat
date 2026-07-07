@echo off
setlocal
cd /d "%~dp0"
call ..\realms\test\deploy-local.bat %*
exit /b %errorlevel%
