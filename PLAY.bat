@echo off
if exist "%~dp0PLAY.exe" (start "" "%~dp0PLAY.exe" & exit /b 0)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play.ps1" %*
if errorlevel 1 pause
