@echo off
if exist "%~dp0PLAYTHISGAME.exe" (start "" "%~dp0PLAYTHISGAME.exe" & exit /b 0)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play.ps1" %*
if errorlevel 1 pause
