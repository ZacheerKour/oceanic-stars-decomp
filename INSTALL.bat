@echo off
rem Fallback for INSTALL.exe (double-click or drag your two disc files onto INSTALL.exe instead). Uses the exe when present, else the PowerShell installer.
chcp 65001 >nul
cd /d "%~dp0"
if exist "%~dp0INSTALL.exe" ("%~dp0INSTALL.exe" %*) else (powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %* & echo. & pause)
