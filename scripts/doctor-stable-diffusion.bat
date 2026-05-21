@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "DOCTOR_PS1=%SCRIPT_DIR%doctor-stable-diffusion.ps1"
where pwsh >nul 2>nul
if %ERRORLEVEL%==0 (
	pwsh -NoProfile -ExecutionPolicy Bypass -File "%DOCTOR_PS1%" %*
) else (
	powershell -NoProfile -ExecutionPolicy Bypass -File "%DOCTOR_PS1%" %*
)
exit /b %ERRORLEVEL%
