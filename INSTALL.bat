@echo off
rem Star Ocean 2 PC install: the only entry point. Plain cmd, no PowerShell, no prebuilt binary.
rem 1) finds the Visual Studio C++ compiler (or, after a Y/N question, installs the Build Tools with winget),
rem 2) compiles recomp\installer\install.c into .install\setup.exe (it is built here, never downloaded),
rem 3) runs it: it finds your discs in roms\, builds everything on this PC and writes PLAYTHISGAME.exe next to this file.
rem Safe to run again; finished steps are skipped. Optional: drag disc files onto this file. SO2_NOASK=1 disables all questions/pauses.
setlocal EnableExtensions
chcp 65001 >nul
cd /d "%~dp0"
set "ROOT=%CD%"
set "PF86=%ProgramFiles(x86)%"
if not exist ".install" mkdir ".install"
set "DBLCLICK="
if not defined SO2_NOASK echo %cmdcmdline% | find /i "/c" >nul && set "DBLCLICK=1"

call :findvs
if defined VS goto :build
echo.
echo The Visual Studio C++ compiler is needed to build the game, and was not found.
where winget >nul 2>nul || goto :nowinget
if defined SO2_NOASK goto :declined
choice /c YN /m "Install the Visual Studio 2022 Build Tools (C++, about 8 GB) now with winget? Windows will ask for approval"
if errorlevel 2 goto :declined
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --source winget --accept-package-agreements --accept-source-agreements --override "--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
call :findvs
if defined VS goto :build
echo The Build Tools install did not finish. Restart Windows if it asked you to, then run INSTALL.bat again.
set "RC=1" & goto :end
:nowinget
echo winget is not available. Install "Visual Studio 2022 Build Tools" with the "Desktop development with C++" workload
echo yourself (https://aka.ms/vs/17/release/vs_BuildTools.exe), then run INSTALL.bat again.
set "RC=1" & goto :end
:declined
echo Install the Visual Studio 2022 Build Tools with the "Desktop development with C++" workload (see README), then run INSTALL.bat again.
set "RC=1" & goto :end

:build
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || (echo Could not start the compiler environment in "%VS%". & set "RC=1" & goto :end)
if not exist ".install\obj" mkdir ".install\obj"
pushd "recomp\installer"
rc /nologo "/fo%ROOT%\.install\obj\install.res" install.rc >"%ROOT%\.install\setup_build.log" 2>&1 && cl /nologo /O2 /MT /GS- /utf-8 /wd4996 "/Fo%ROOT%\.install\obj\\" "/Fe%ROOT%\.install\setup.exe" install.c "%ROOT%\.install\obj\install.res" /link /MANIFEST:NO /SUBSYSTEM:CONSOLE user32.lib shell32.lib ole32.lib uuid.lib comdlg32.lib winhttp.lib bcrypt.lib >>"%ROOT%\.install\setup_build.log" 2>&1
set "RC=%ERRORLEVEL%"
popd
if not "%RC%"=="0" (type ".install\setup_build.log" & echo. & echo Could not build the installer program ^(see above^). & goto :end)
".install\setup.exe" --root "%ROOT%" %*
set "RC=%ERRORLEVEL%"
goto :end

:findvs
set "VS="
if exist "%PF86%\Microsoft Visual Studio\Installer\vswhere.exe" for /f "usebackq delims=" %%i in (`"%PF86%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath`) do set "VS=%%i"
if not defined VS if exist "%PF86%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VS=%PF86%\Microsoft Visual Studio\2022\BuildTools"
exit /b 0

:end
if defined DBLCLICK (echo. & pause)
exit /b %RC%
