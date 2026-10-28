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
if not defined SO2_NOASK call :isdbl

call :findvs
if defined VS goto :build
if defined VSPART goto :nocmake
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
set "SO2RC=1" & goto :end
:nowinget
echo winget is not available. Install "Visual Studio 2022 Build Tools" with the "Desktop development with C++" workload
echo yourself (https://aka.ms/vs/17/release/vs_BuildTools.exe), then run INSTALL.bat again.
set "SO2RC=1" & goto :end
:declined
echo Install the Visual Studio 2022 Build Tools with the "Desktop development with C++" workload (see README), then run INSTALL.bat again.
set "SO2RC=1" & goto :end

:nocmake
echo.
echo Visual Studio is installed, but without the "C++ CMake tools for Windows" component that the build needs:
echo   "%VSPART%"
if defined SO2_NOASK goto :cmakehow
choice /c YN /m "Add that component now? Windows will ask for approval"
if errorlevel 2 goto :cmakehow
"%PF86%\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "%VSPART%" --add Microsoft.VisualStudio.Component.VC.CMake.Project --passive --wait --norestart
call :findvs
if defined VS goto :build
echo The component is still missing (the change failed, was cancelled, or Windows needs a restart). Restart if asked, then run INSTALL.bat again.
set "SO2RC=1" & goto :end
:cmakehow
echo Open "Visual Studio Installer", choose Modify on that install, tick "C++ CMake tools for Windows", apply, then run INSTALL.bat again.
set "SO2RC=1" & goto :end

:build
call :compile
if not "%SO2RC%"=="0" goto :end
".install\setup.exe" --root "%ROOT%" %*
set "SO2RC=%ERRORLEVEL%"
goto :end

:compile
setlocal
set "PATH=%PF86%\Microsoft Visual Studio\Installer;%PATH%"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || (echo Could not start the compiler environment in "%VS%". & endlocal & set "SO2RC=1" & exit /b 1)
if not exist ".install\obj" mkdir ".install\obj"
pushd "recomp\installer"
rc /nologo "/fo%ROOT%\.install\obj\install.res" install.rc >"%ROOT%\.install\setup_build.log" 2>&1 && cl /nologo /O2 /MT /GS- /utf-8 /wd4996 "/Fo%ROOT%\.install\obj\\" "/Fe%ROOT%\.install\setup.exe" install.c "%ROOT%\.install\obj\install.res" /link /MANIFEST:NO /SUBSYSTEM:CONSOLE user32.lib shell32.lib ole32.lib uuid.lib comdlg32.lib winhttp.lib bcrypt.lib >>"%ROOT%\.install\setup_build.log" 2>&1
set "SO2RC=%ERRORLEVEL%"
popd
if not "%SO2RC%"=="0" (type ".install\setup_build.log" & echo. & echo Could not build the installer program ^(see above^). & endlocal & set "SO2RC=%SO2RC%" & exit /b 1)
endlocal
set "SO2RC=0"
exit /b 0

:isdbl
rem delayed expansion only here: cmdcmdline may contain & ^ ( ) that must not be parsed as commands
setlocal EnableDelayedExpansion
echo(!cmdcmdline!| find /i "/c" >nul && (endlocal & set "DBLCLICK=1" & exit /b 0)
endlocal
exit /b 0

:findvs
set "VS=" & set "VSPART="
set "VW=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
set "CMK=Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
rem same test as recomp\installer\install.c find_vs: compiler AND cmake.exe
if exist "%VW%" for /f "usebackq delims=" %%i in (`"%VW%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath`) do if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" if exist "%%i\%CMK%" set "VS=%%i"
if not defined VS if exist "%PF86%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" if exist "%PF86%\Microsoft Visual Studio\2022\BuildTools\%CMK%" set "VS=%PF86%\Microsoft Visual Studio\2022\BuildTools"
if defined VS exit /b 0
if exist "%VW%" for /f "usebackq delims=" %%i in (`"%VW%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" set "VSPART=%%i"
if not defined VSPART if exist "%PF86%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VSPART=%PF86%\Microsoft Visual Studio\2022\BuildTools"
exit /b 0
:end
if defined DBLCLICK (echo. & pause)
exit /b %SO2RC%
