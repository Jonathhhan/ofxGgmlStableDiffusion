@echo off
setlocal

powershell -ExecutionPolicy Bypass -File "%~dp0release-candidate.ps1" %*
exit /b %ERRORLEVEL%
