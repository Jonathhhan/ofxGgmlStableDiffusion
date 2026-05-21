@echo off
setlocal

powershell -ExecutionPolicy Bypass -File "%~dp0validate-local.ps1" %*
exit /b %ERRORLEVEL%
