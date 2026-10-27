<# Builds INSTALL.exe (repo root, TRACKED: it holds no game code) from install.c and writes INSTALL.exe.sha256 next to this script;
   scripts/repo_guard.py refuses a tracked INSTALL.exe whose hash differs from that file. Needs MSVC (vswhere). #>
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }
$o = Join-Path $env:TEMP 'so2_installexe'; New-Item -ItemType Directory -Force $o | Out-Null
Push-Location $PSScriptRoot
& rc /nologo "/fo$o\install.res" install.rc; if ($LASTEXITCODE) { throw 'rc failed' }
& cl /nologo /O2 /MT /GS- /utf-8 /wd4996 "/Fo$o\" "/Fe$root\INSTALL.exe" install.c "$o\install.res" /link /MANIFEST:NO /SUBSYSTEM:CONSOLE user32.lib shell32.lib ole32.lib uuid.lib comdlg32.lib winhttp.lib bcrypt.lib
if ($LASTEXITCODE) { throw 'INSTALL.exe build failed' }
Pop-Location
(Get-FileHash "$root\INSTALL.exe" -Algorithm SHA256).Hash.ToLower() | Set-Content -NoNewline "$PSScriptRoot\INSTALL.exe.sha256"
"built $root\INSTALL.exe"
