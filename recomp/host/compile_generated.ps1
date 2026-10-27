<#
Compile-only check of the C that build_generate.ps1 generated (boot exe + program A), with the installed VS 2022 Build Tools (cl).
Includes only runtime\include of the pinned clone (the 10 headers psx_runtime.h pulls in, audited for this use:
RECOMP_NOTES.md '2026-10-26 Host compile, A data region, overlay decoder'). Nothing is linked or run; objects stay in <OutName>\obj_<label>\.
Usage: $env:SO2_DATA=<main checkout>; powershell -NoProfile -ExecutionPolicy Bypass -File recomp\host\compile_generated.ps1 [-OutName _out] [-Opt /Od]
  [-Sets label=<dir of .c>,...]  (e.g. the AOT overlays_static*.c dirs; objects and logs then go to <OutName>\obj_<label>, compile_<label>.log)
#>
param([string]$OutName = '_out', [string]$Opt = '/Od', [string[]]$Program = @('boot', 'A', 'A_reach'), [string[]]$Sets = @())
$ErrorActionPreference = 'Stop'
$Program = @($Program -split ',')   # -File passes 'a,b' as one string
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$data = if ($env:SO2_DATA) { (Resolve-Path $env:SO2_DATA).Path } else { $repo }
$inc = Join-Path $data 'tools\ref\psxrecomp\runtime\include'
$O = if ([IO.Path]::IsPathRooted($OutName)) { $OutName } else { Join-Path $here $OutName }
$vs = if ($env:SO2_VS) { $env:SO2_VS } else { 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' }   # INSTALL.bat sets SO2_VS (vswhere)
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }

$list = New-Object System.Collections.ArrayList
foreach ($s in $Sets) { [void]$list.Add(($s -split '=', 2)) }
if (-not $Sets) { foreach ($d in @(@('boot', "$O\proj\generated"), @('A', "$O\gen_A"), @('A_reach', "$O\proj_reach\generated"), @('B', "$O\proj_reach_B\generated"), @('C', "$O\proj_reach_C\generated")) | Where-Object { $Program -contains $_[0] }) { [void]$list.Add($d) } }
foreach ($r in $list) {
  $src = @(Get-ChildItem $r[1] -Filter *.c -File -ErrorAction SilentlyContinue)
  if (-not $src) { continue }
  $obj = "$O\obj_$($r[0])"; New-Item -ItemType Directory -Force $obj | Out-Null
  $log = "$O\compile_$($r[0]).log"
  $t = Measure-Command { & cl /nologo /c $Opt /W3 /MP /bigobj /I $inc /I $r[1] "/Fo$obj\\" @($src.FullName) *> $log }
  $objs = @(Get-ChildItem $obj -Filter *.obj -File | Where-Object { $_.LastWriteTime -gt (Get-Date).Add(-$t) })
  $lines = Get-Content $log
  Write-Host ("compile {0}: exit {1}, {2}/{3} objects, {4} errors, {5} warnings, {6:N0} s" -f $r[0], $LASTEXITCODE, $objs.Count, $src.Count,
    @($lines | Select-String ': (fatal )?error ').Count, @($lines | Select-String ': warning ').Count, $t.TotalSeconds)
}
