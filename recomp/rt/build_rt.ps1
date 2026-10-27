<#
Links the minimal runtime (recomp\rt\*.c, ours) with the boot-exe objects that
recomp\host\compile_generated.ps1 produced. MSVC (VS 2022 Build Tools) only.
Headers: the 10 audited files in the pinned clone's runtime\include (read only).
Output: recomp\rt\_out\rt_boot.exe (gitignored). No network, no downloads.
#>
param(
  [string]$Clone = (Join-Path $PSScriptRoot "..\..\tools\ref\psxrecomp"),
  [string]$Objs  = "$PSScriptRoot\..\host\_out\obj_boot"
)
$ErrorActionPreference = 'Stop'
$head = (& git -C $Clone rev-parse HEAD)
if ($head -ne 'f42d2c004b3dd6ef6ccb83101b40ede55f24bf59') { throw "clone not at audited commit: $head" }
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"  # vcvars64 calls vswhere by name
$vs = & vswhere.exe -products * -latest -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }
$out = "$PSScriptRoot\_out"
New-Item -ItemType Directory -Force $out | Out-Null
$src = 'rt.c','cdrom.c','gpu.c','host_headless.c' | ForEach-Object { "$PSScriptRoot\$_" }
$gen = Get-ChildItem "$Objs\*.obj" | ForEach-Object { $_.FullName }
& cl /nologo /O2 /W3 -D_CRT_SECURE_NO_WARNINGS "-I$Clone\runtime\include" "/Fo$out\\" "/Fe$out\rt_boot.exe" @src @gen /link /INCREMENTAL:NO
if ($LASTEXITCODE) { throw "link failed ($LASTEXITCODE)" }
Get-Item "$out\rt_boot.exe" | Select-Object FullName, Length, LastWriteTime
