@echo off
rem Player install. Usage: INSTALL.bat "<folder with your two Star Ocean 2 USA discs>"  (or drag the disc files onto this file)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
if errorlevel 1 (echo. & echo Install failed, see the messages above.) else (echo. & echo Done. Start the game with PLAY.bat.)
pause
