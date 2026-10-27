<#
Ahead-of-time SCPH-5501 BIOS backend: builds psxrecomp-bios from the pinned clone (read only) with MSVC, then emits
SCPH5501_full.c / SCPH5501_dispatch.c from the user's own dump into recomp\bios\_gen\generated (gitignored; it is code
generated from a Sony BIOS and must never be committed). recomp\rtfull\build.ps1 links it when present.
Audit (RECOMP_NOTES '2026-10-27 SCPH5501 backend'): the --config path runs run_emit_full only (reads ROM + seeds, writes
out_dir); std::system is used only by the legacy boot-slice form (compiler probe + self-check), which is never invoked here.
Usage: $env:SO2_DATA=<main checkout>; powershell -NoProfile -ExecutionPolicy Bypass -File recomp\bios\gen_scph5501.ps1
#>
$ErrorActionPreference = 'Stop'
$Commit = 'f42d2c004b3dd6ef6ccb83101b40ede55f24bf59'
$Pin = '11052b6499e466bbf0a709b1f9cb6834a9418e66680387912451e971cf8a1fef'   # bios/SCPH5501.toml [program.image] sha256
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$data = if ($env:SO2_DATA) { (Resolve-Path $env:SO2_DATA).Path } else { $repo }
$src = Join-Path $data 'tools\ref\psxrecomp'
$dump = Join-Path $data 'roms\scph5501.bin'
$G = Join-Path $here '_gen'; $B = "$G\build"; $P = "$G\proj"; $O = "$G\generated"
if ((& git -C $src rev-parse HEAD) -ne $Commit) { throw "psxrecomp clone is not at $Commit" }
if (& git -C $src status --porcelain --ignored) { throw 'psxrecomp clone is not clean; refusing' }
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $dump).Hash.ToLower() -ne $Pin) { throw "$dump is not the pinned SCPH-5501 image" }

$vs = if ($env:SO2_VS) { $env:SO2_VS } else { 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' }   # INSTALL.bat sets SO2_VS (vswhere)
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }
$env:Path = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:Path"
New-Item -ItemType Directory -Force $B, "$P\bios", "$P\recompiler\seeds", $O | Out-Null

# CHD off = no FetchContent (network); same flags as recomp\host\build_generate.ps1. Writes only into $B.
& cmake -S "$src\recompiler" -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSXRECOMP_ENABLE_CHD=OFF -DBUILD_TESTING=OFF `
    -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl *> "$G\configure.log"
if ($LASTEXITCODE) { throw "configure failed (see $G\configure.log)" }
& cmake --build $B --target psxrecomp-bios *> "$G\build.log"
if ($LASTEXITCODE) { throw "build failed (see $G\build.log)" }

# Profile project = the clone's layout (find_project_root stops at .gitignore), profile + SO2 install slot patch.
Set-Content "$P\.gitignore" '*'
Copy-Item "$src\bios\SCPH5501.toml" "$P\bios\SCPH5501.toml" -Force
Copy-Item "$src\recompiler\seeds\phase2_ghidra_seeds_SCPH5501.json" "$P\recompiler\seeds\" -Force
& git -C $repo apply --directory=recomp/bios/_gen/proj (Join-Path $here 'SCPH5501_install_slots.patch')
if ($LASTEXITCODE) { throw 'SCPH5501_install_slots.patch did not apply' }
cmd /c "`"$B\psxrecomp-bios.exe`" --config `"$P\bios\SCPH5501.toml`" --rom `"$dump`" --out-dir `"$O`" >`"$G\emit.log`" 2>&1"   # cmd: its stderr WARNINGs are not PS errors
$rc = $LASTEXITCODE
if (& git -C $src status --porcelain --ignored) { throw 'psxrecomp-bios wrote into the psxrecomp clone' }
"psxrecomp-bios exit $rc; $((Get-Content "$G\emit.log" | Select-String 'EMIT OK|FATAL|WARNING') -join ' | ')"
if ($rc) { exit 1 }
Get-ChildItem $O -File | ForEach-Object { '{0} {1:N1} MB' -f $_.Name, ($_.Length / 1MB) }
