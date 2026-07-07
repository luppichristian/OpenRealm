@echo off
setlocal
cd /d "%~dp0"
call ..\realms\test\start-ganache-local.bat %*
exit /b %errorlevel%
