# Incremental build of _build_final (same configure as build.ps1 with the inputs of $env:SO2_GEN); prints errors only.
param([string]$BuildDir = '_build_final')
$vs = if ($env:SO2_VS) { $env:SO2_VS } else { 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' }   # INSTALL.exe sets SO2_VS (vswhere)
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }
$env:Path = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:Path"
$B = Join-Path $PSScriptRoot $BuildDir
cmake --build $B -- -k 0 *> "$B\build.log"
"build exit $LASTEXITCODE"
Get-Content "$B\build.log" | Select-String ': (fatal )?error |LNK\d+' | Select-Object -First 20 | ForEach-Object { $_.Line }
