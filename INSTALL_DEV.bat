@echo off
rem Developer toolchain (decomp: Docker image, maspsx, objdiff, splat). Usage: INSTALL_DEV.bat "<folder with the two Disc .7z files>"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_dev.ps1" %*
