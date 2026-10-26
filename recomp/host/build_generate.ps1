<#
Host build of the two vetted psxrecomp targets (psxrecomp-analyze, psxrecomp-game) from the pinned clone, then C generation
for the boot exe (recomp\game.toml) and main program A. Allowed only for the audited commit and targets:
RECOMP_NOTES.md '2026-10-26 psxrecomp safety audit' (verdict SAFE WITH CAVEATS). Never run psxrecomp_cli.py, the codegen
host, tools\*.py or any prebuilt exe of the clone (overlay_toolchain\, .cache\).
Uses the installed VS 2022 Build Tools (MSVC + bundled CMake/Ninja), environment for this process only. Output: recomp\host\_out\ (gitignored).
Usage: $env:SO2_DATA=<main checkout>; powershell -NoProfile -ExecutionPolicy Bypass -File recomp\host\build_generate.ps1 [-OutName _out] [-ASeeds <file>]
Nothing is deleted: reruns overwrite (the generator drops its own stale shards). Use a new -OutName for a clean run.
#>
param([string]$OutName = '_out', [string]$ASeeds = '')
$ErrorActionPreference = 'Stop'
$Commit = 'f42d2c004b3dd6ef6ccb83101b40ede55f24bf59'   # audited
$ExeSha = 'b93e42462aceec120cfad1bfc3cf08f933196ab24a4e018b5f494c74e213abe7'   # SCUS_944.21
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$data = if ($env:SO2_DATA) { (Resolve-Path $env:SO2_DATA).Path } else { $repo }
$src = Join-Path $data 'tools\ref\psxrecomp'
$O = Join-Path $here $OutName; $B = Join-Path $O 'build'; $P = Join-Path $O 'proj'
$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
function Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }

# the clone must be exactly the audited commit with no local changes
if ((& git -C $src rev-parse HEAD) -ne $Commit) { throw "psxrecomp clone is not at the audited commit $Commit" }
if (& git -C $src status --porcelain) { throw 'psxrecomp clone has local changes; refusing' }

# MSVC environment for this process only (vcvars64 prints the variables; nothing is persisted)
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }
$env:Path = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:Path"

New-Item -ItemType Directory -Force $O | Out-Null
# CHD off: the only FetchContent (network) step of recompiler/CMakeLists.txt. The build writes only into $B.
& cmake -S "$src\recompiler" -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSXRECOMP_ENABLE_CHD=OFF -DBUILD_TESTING=OFF -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl *> "$O\cmake_configure.log"
if ($LASTEXITCODE) { throw "configure failed (see $O\cmake_configure.log)" }
& cmake --build $B --target psxrecomp-analyze psxrecomp-game *> "$O\cmake_build.log"
if ($LASTEXITCODE) { throw "build failed (see $O\cmake_build.log)" }
if (& git -C $src status --porcelain --ignored) { throw 'the build wrote into the psxrecomp clone' }
$gen = Join-Path $B 'psxrecomp-game.exe'

# minimal project for the boot exe, same layout as the sandbox run (run_in_sandbox.ps1 step 7)
New-Item -ItemType Directory -Force "$P\disc", "$P\seeds", "$P\bios", "$O\stage" | Out-Null
$exe = Join-Path $data 'extracted\SCUS_944.21'
if ((Sha $exe) -ne $ExeSha) { throw 'SCUS_944.21 SHA-256 mismatch' }
Copy-Item $exe "$P\disc\SCUS_944.21"
Copy-Item (Join-Path $repo 'config\seeds.txt') "$P\seeds\seeds.txt"
Copy-Item (Join-Path $repo 'recomp\game.toml') "$P\game.toml"
Copy-Item "$src\bios\SCPH1001.toml" "$P\bios\SCPH1001.toml"
Set-Content "$P\.gitignore" 'generated/'   # find_project_root stops here
& python -I (Join-Path $repo 'recomp\sandbox\stage_overlays.py') $repo "$O\stage"   # A.exe + A.seeds.txt (and B, C)
if ($LASTEXITCODE) { throw 'stage_overlays.py failed' }
if ($ASeeds) { Copy-Item $ASeeds "$O\stage\A.seeds.txt" }   # alternative A seed list (0x-prefixed)
# program A again with [recompiler] discovery = "reachable" (exact-entry walk from the seeds; no data-as-code sweep)
$R = Join-Path $O 'proj_reach'
New-Item -ItemType Directory -Force "$R\bios" | Out-Null
Copy-Item "$O\stage\A.exe", "$O\stage\A.seeds.txt" $R
Copy-Item (Join-Path $here 'A_reachable.toml') "$R\game.toml"
Copy-Item "$src\bios\SCPH1001.toml" "$R\bios\SCPH1001.toml"
Set-Content "$R\.gitignore" 'generated/'

$runs = @(@('boot', "$P\generated", @('--config', "$P\game.toml", '--project-root', $P)),
          @('A', "$O\gen_A", @("$O\stage\A.exe", '--seeds', "$O\stage\A.seeds.txt", '--out-dir', "$O\gen_A", '--project-root', $P)),
          @('A_reach', "$R\generated", @('--config', "$R\game.toml", '--project-root', $R)))
foreach ($r in $runs) {
  & $gen @($r[2]) *> "$O\generate_$($r[0]).log"
  $rc = $LASTEXITCODE
  $f = @(Get-ChildItem $r[1] -File -ErrorAction SilentlyContinue)
  Write-Host ("generate {0}: exit {1}, {2} files, {3:N1} MB; {4}" -f $r[0], $rc, $f.Count, (($f | Measure-Object Length -Sum).Sum / 1MB),
    ((Get-Content "$O\generate_$($r[0]).log" | Select-String 'Generated \d+ functions|Dispatch table written|Wrote \d+ shards') -join ' | '))
}
