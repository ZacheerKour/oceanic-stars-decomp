@echo off
rem Drag your two Star Ocean 2 (USA) disc files onto this file (or run: INSTALL.bat disc1 disc2). With no files, a window opens to pick them.
chcp 65001 >nul
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
echo.
pause
