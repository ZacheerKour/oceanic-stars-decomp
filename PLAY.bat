@echo off
if exist "%~dp0StarOcean2.exe" (start "" "%~dp0StarOcean2.exe" & exit /b 0)
if exist "%~dp0PLAY.exe" (start "" "%~dp0PLAY.exe" & exit /b 0)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play.ps1" %*
if errorlevel 1 pause
