<#
Host build of the two vetted psxrecomp targets (psxrecomp-analyze, psxrecomp-game) from the pinned clone, then C generation
for the boot exe (recomp\game.toml) and main program A. Allowed only for the audited commit and targets:
RECOMP_NOTES.md '2026-10-26 psxrecomp safety audit' (verdict SAFE WITH CAVEATS). Never run psxrecomp_cli.py, the codegen
host, tools\*.py or any prebuilt exe of the clone (overlay_toolchain\, .cache\).
Uses the installed VS 2022 Build Tools (MSVC + bundled CMake/Ninja), environment for this process only. Output: recomp\host\_out\ (gitignored).
-Program picks the runs: boot, A (whole-image), A_reach, B, C (the last three in reachable mode; B_reachable.toml, C_reachable.toml).
Usage: $env:SO2_DATA=<main checkout>; powershell -NoProfile -ExecutionPolicy Bypass -File recomp\host\build_generate.ps1 [-OutName _out] [-ASeeds <file>]
Nothing is deleted: reruns overwrite (the generator drops its own stale shards). Use a new -OutName for a clean run.
#>
param([string]$OutName = '_out', [string]$ASeeds = '', [string[]]$Program = @('boot', 'A', 'A_reach'))
$ErrorActionPreference = 'Stop'
$Program = @($Program -split ',')   # -File passes 'a,b' as one string
$Commit = 'f42d2c004b3dd6ef6ccb83101b40ede55f24bf59'   # audited
$ExeSha = 'b93e42462aceec120cfad1bfc3cf08f933196ab24a4e018b5f494c74e213abe7'   # SCUS_944.21
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$data = if ($env:SO2_DATA) { (Resolve-Path $env:SO2_DATA).Path } else { $repo }
$src = Join-Path $data 'tools\ref\psxrecomp'
$O = Join-Path $here $OutName; $B = Join-Path $O 'build'; $P = Join-Path $O 'proj'
$vs = if ($env:SO2_VS) { $env:SO2_VS } else { 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' }   # INSTALL.exe sets SO2_VS (vswhere)
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
# game.toml [recompiler] bios_config = psxrecomp/bios/SCPH5501.toml (+ the SO2 install-slot patch), relative to the project root
New-Item -ItemType Directory -Force "$P\psxrecomp\bios", "$P\psxrecomp\recompiler\seeds" | Out-Null
Copy-Item "$src\bios\SCPH5501.toml" "$P\psxrecomp\bios\SCPH5501.toml" -Force
Copy-Item "$src\recompiler\seeds\phase2_ghidra_seeds_SCPH5501.json" "$P\psxrecomp\recompiler\seeds\" -Force
& git -C $repo apply "--directory=recomp/host/$OutName/proj/psxrecomp" (Join-Path $repo 'recomp\bios\SCPH5501_install_slots.patch')
if ($LASTEXITCODE) { throw 'SCPH5501_install_slots.patch did not apply' }
Set-Content "$P\.gitignore" 'generated/'   # find_project_root stops here
& python -I (Join-Path $repo 'recomp\sandbox\stage_overlays.py') $repo "$O\stage"   # A.exe + A.seeds.txt (and B, C)
if ($LASTEXITCODE) { throw 'stage_overlays.py failed' }
if ($ASeeds) { Copy-Item $ASeeds "$O\stage\A.seeds.txt" }   # alternative A seed list (0x-prefixed)
# programs A, B, C again with [recompiler] discovery = "reachable" (exact-entry walk from the seeds; no data-as-code sweep)
function ReachProj($pg) {
  $d = Join-Path $O "proj_reach$(if ($pg -ne 'A') { "_$pg" })"
  New-Item -ItemType Directory -Force "$d\bios" | Out-Null
  Copy-Item "$O\stage\$pg.exe", "$O\stage\$pg.seeds.txt" $d
  Copy-Item (Join-Path $here "${pg}_reachable.toml") "$d\game.toml"
  Copy-Item "$src\bios\SCPH1001.toml" "$d\bios\SCPH1001.toml"
  Set-Content "$d\.gitignore" 'generated/'
  $d
}
$R = ReachProj 'A'
$allRuns = [ordered]@{
  boot = @('boot', "$P\generated", @('--config', "$P\game.toml", '--project-root', $P))
  A = @('A', "$O\gen_A", @("$O\stage\A.exe", '--seeds', "$O\stage\A.seeds.txt", '--out-dir', "$O\gen_A", '--project-root', $P))
  A_reach = @('A_reach', "$R\generated", @('--config', "$R\game.toml", '--project-root', $R)) }
foreach ($pg in 'B', 'C') { if ($Program -contains $pg) { $d = ReachProj $pg; $allRuns[$pg] = @($pg, "$d\generated", @('--config', "$d\game.toml", '--project-root', $d)) } }
$runs = @($Program | ForEach-Object { ,$allRuns[$_] })
foreach ($r in $runs) {
  & $gen @($r[2]) *> "$O\generate_$($r[0]).log"
  $rc = $LASTEXITCODE
  $f = @(Get-ChildItem $r[1] -File -ErrorAction SilentlyContinue)
  Write-Host ("generate {0}: exit {1}, {2} files, {3:N1} MB; {4}" -f $r[0], $rc, $f.Count, (($f | Measure-Object Length -Sum).Sum / 1MB),
    ((Get-Content "$O\generate_$($r[0]).log" | Select-String 'Generated \d+ functions|Dispatch table written|Wrote \d+ shards') -join ' | '))
}
