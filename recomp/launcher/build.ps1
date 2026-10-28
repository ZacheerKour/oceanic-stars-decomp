<# Builds PLAYTHISGAME.exe (repo root, gitignored) from recomp\launcher\play.c with the MSVC Build Tools INSTALL.bat already requires. #>
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vs = if ($env:SO2_VS) { $env:SO2_VS } else { 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' }   # INSTALL.bat sets SO2_VS (vswhere)
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }
$o = Join-Path $env:TEMP 'so2_playexe'; New-Item -ItemType Directory -Force $o | Out-Null
& cl /nologo /O2 /MT /GS- /utf-8 /wd4996 "/Fo$o\" "/Fe$root\PLAYTHISGAME.exe" "$PSScriptRoot\play.c" /link /SUBSYSTEM:WINDOWS user32.lib kernel32.lib shell32.lib gdi32.lib gdiplus.lib
if ($LASTEXITCODE) { throw 'PLAYTHISGAME.exe build failed' }
"built $root\PLAYTHISGAME.exe"
