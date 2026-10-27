<#
Builds psxrecomp-game.exe (full psxrecomp runtime + SO2 boot exe C) with VS 2022 Build Tools, offline, then audits its
import table: no Winsock/iphlpapi/WinINet/WinHTTP DLL, no CreateProcess*/ShellExecute*/WinExec/LoadLibrary[A|W|ExA].
Inputs: the pinned clone (read only), recomp\host\<GenOut>\proj\generated (build_generate.ps1), recomp\rtfull\_deps (build_deps.ps1).
Output: recomp\rtfull\_build\ (gitignored); SDL3.dll is copied beside the exe.
Usage: $env:SO2_DATA=<main checkout>; powershell -NoProfile -ExecutionPolicy Bypass -File recomp\rtfull\build.ps1 [-GenOut _out]
#>
param([string]$GenOut = '_out')
$Commit = 'f42d2c004b3dd6ef6ccb83101b40ede55f24bf59'
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$data = if ($env:SO2_DATA) { (Resolve-Path $env:SO2_DATA).Path } else { $repo }
$src = Join-Path $data 'tools\ref\psxrecomp'
$gen = Join-Path $repo "recomp\host\$GenOut\proj\generated"
$deps = Join-Path $here '_deps\install'
$B = Join-Path $here '_build'
if ((& git -C $src rev-parse HEAD) -ne $Commit) { throw "psxrecomp clone is not at $Commit" }
if (& git -C $src status --porcelain --ignored) { throw 'psxrecomp clone is not clean; refusing' }
if (-not (Test-Path "$gen\SCUS_944.21_dispatch.c")) { throw "no generated boot C in $gen (run recomp\host\build_generate.ps1)" }
if (-not (Test-Path "$deps\lib\SDL3.lib")) { throw "no deps in $deps (run recomp\rtfull\build_deps.ps1)" }

$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }
$env:Path = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:Path"
New-Item -ItemType Directory -Force $B | Out-Null

& cmake -S $here -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl `
    "-DPSXRECOMP_ROOT=$src" "-DGAME_GEN_DIR=$gen" "-DDEPS_PREFIX=$deps" *> "$B\configure.log"
if ($LASTEXITCODE) { throw "configure failed (see $B\configure.log)" }
& cmake --build $B -- -k 0 *> "$B\build.log"
$rc = $LASTEXITCODE
$log = Get-Content "$B\build.log"
"build exit $rc; errors: $(@($log | Select-String ': (fatal )?error ').Count)"
if ($rc) { $log | Select-String ': (fatal )?error |LNK\d+' | Select-Object -First 40 | ForEach-Object { $_.Line }; exit 1 }
if (& git -C $src status --porcelain --ignored) { throw 'build wrote into the psxrecomp clone' }

$exe = "$B\psxrecomp-game.exe"
Copy-Item "$deps\bin\SDL3.dll" $B -Force
$imp = & dumpbin /nologo /imports $exe
$badDll = $imp | Select-String -Pattern '^\s+(ws2_32|wsock32|iphlpapi|wininet|winhttp|urlmon)\.dll' -CaseSensitive:$false
$badFn = $imp | Select-String -Pattern '\s(CreateProcess\w*|ShellExecute\w*|WinExec|LoadLibrary(A|W|ExA))$'
if ($badDll -or $badFn) { ($badDll + $badFn) | ForEach-Object { $_.Line }; throw 'import audit FAILED' }
"import audit OK: $(@($imp | Select-String '^\s+\S+\.dll$').Count) DLLs, none of the blocked DLLs/functions"
($imp | Select-String '^\s+\S+\.dll$' | ForEach-Object { $_.Line.Trim() }) -join ' '
