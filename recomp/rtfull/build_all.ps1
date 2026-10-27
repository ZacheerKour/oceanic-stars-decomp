<#
One reproducible build of psxrecomp-game.exe from a clean checkout plus the local dumps (all output gitignored):
  1. SDL3 + zlib (build_deps.ps1; skipped when _deps\install has them; extracts the pinned archives if needed)
  2. boot exe C (recomp\host\build_generate.ps1 -Program boot)
  3. SCPH-5501 BIOS C from roms\scph5501.bin if present (recomp\bios\gen_scph5501.ps1)
  4. overlay C for all 89 SLZ chunks, seeds config\seeds_final, ONE combined profile (recomp\aot\run_static_all.py
     --mode final --combined ALL) in a work copy recomp\aot\_psxrecomp = git clone of the pinned clone +
     recomp\aot\psxrecomp_overlay.patch + analysis\slz.py (the clone itself is only read)
  5. MSVC build + import audit (build.ps1 -OverlayDir ... -BuildDir _build_final)
Needs: $env:SO2_DATA = main checkout with tools\ref\psxrecomp (f42d2c0, clean), tools\dl\{SDL3-3.4.10,zlib-1.3.1}.tar.gz,
roms\disc1\*.cue, optionally roms\scph5501.bin (else OpenBIOS only); VS 2022 Build Tools; python 3.11 on PATH.
Usage: powershell -NoProfile -ExecutionPolicy Bypass -File recomp\rtfull\build_all.ps1 [-BuildDir _build_final] [-OverlayOpt /Od]
Then:  run.ps1 -BuildDir _build_final -Bios <SO2_DATA>\roms\scph5501.bin -Window
#>
param([string]$BuildDir = '_build_final', [string]$OverlayOpt = '/O1')
# /O1 overlay C: inlines the psx_cyc.h helpers, +15% headless fps at the title vs /Od (RECOMP_NOTES 2026-10-27 integration)
$ErrorActionPreference = 'Stop'
$Commit = 'f42d2c004b3dd6ef6ccb83101b40ede55f24bf59'
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
if (-not $env:SO2_DATA) { $env:SO2_DATA = $repo }
$data = (Resolve-Path $env:SO2_DATA).Path
$src = Join-Path $data 'tools\ref\psxrecomp'
$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
$env:Path = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:Path"   # vcvars64 needs vswhere
$cmake = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$cue = Get-ChildItem (Join-Path $data 'roms\disc1') -Filter '*.cue' | Select-Object -First 1
if (-not $cue) { throw "no disc 1 cue in $data\roms\disc1" }
function Step($name, [scriptblock]$b) {
    $t = [Diagnostics.Stopwatch]::StartNew(); "== $name"
    & $b
    "== $name done ($([int]$t.Elapsed.TotalSeconds) s)"
}
function Run-Child($file, [string[]]$a) {   # child PowerShell so each script keeps its own vcvars environment
    & powershell -NoProfile -ExecutionPolicy Bypass -File $file @a
    if ($LASTEXITCODE) { throw "$file failed ($LASTEXITCODE)" }
}

Step 'deps' {
    if (Test-Path "$here\_deps\install\lib\SDL3.lib") { 'present, skipped'; return }
    $rd = Join-Path $data 'tools\ref-deps'
    foreach ($n in 'SDL3-3.4.10', 'zlib-1.3.1') {   # build_deps.ps1 checks the archive sha256 pins first
        if (-not (Test-Path "$rd\$n\CMakeLists.txt")) { New-Item -ItemType Directory -Force $rd | Out-Null; & tar -xzf "$data\tools\dl\$n.tar.gz" -C $rd }
    }
    Run-Child "$here\build_deps.ps1" @('-Dl', "$data\tools\dl", '-Src', $rd)
}
Step 'boot exe C' { Run-Child "$repo\recomp\host\build_generate.ps1" @('-Program', 'boot') }
Step 'SCPH-5501 BIOS C' {   # optional: without the user's own dump the exe links OpenBIOS only (play.ps1 then uses OpenBIOS)
    if (-not (Test-Path (Join-Path $data 'roms\scph5501.bin'))) { 'no roms\scph5501.bin: OpenBIOS only'; return }
    Run-Child "$repo\recomp\bios\gen_scph5501.ps1" @()
}

$cp = Join-Path $repo 'recomp\aot\_psxrecomp'
$patch = Join-Path $repo 'recomp\aot\psxrecomp_overlay.patch'
Step 'overlay C (final seeds, 89 chunks combined)' {
    if (-not (Test-Path "$cp\.git")) {
        & git clone -q --no-checkout $src $cp; & git -C $cp checkout -q $Commit
        & git -C $cp apply $patch; if ($LASTEXITCODE) { throw 'psxrecomp_overlay.patch did not apply' }
    }
    if ((& git -C $cp rev-parse HEAD) -ne $Commit) { throw "$cp is not at $Commit" }
    & git -C $cp apply --reverse --check $patch 2>$null; if ($LASTEXITCODE) { throw "$cp does not carry exactly psxrecomp_overlay.patch" }
    Copy-Item "$repo\analysis\slz.py" "$cp\tools\slz.py" -Force
    New-Item -ItemType Directory -Force "$cp\_work\proj" | Out-Null
    Copy-Item "$repo\recomp\game.toml" "$cp\_work\proj\game.toml" -Force
    # the positional overlay path ignores [recompiler] bios_config and probes <project-root>/bios/SCPH1001.toml; its address
    # model only folds BIOS-ROM windows, which no overlay (all in user RAM) touches
    New-Item -ItemType Directory -Force "$cp\_work\proj\bios" | Out-Null
    Copy-Item "$cp\bios\SCPH1001.toml" "$cp\_work\proj\bios\SCPH1001.toml" -Force
    & python -E -s "$repo\recomp\aot\run_static_all.py" --mode final --combined ALL --copy $cp `
        --recompiler "$repo\recomp\host\_out\build\psxrecomp-game.exe" --cue $cue.FullName --cmake $cmake
    if ($LASTEXITCODE) { throw 'run_static_all failed' }
    $sum = Get-Content "$cp\_work\final\summary.tsv"
    if ($sum[1] -notmatch "^ALL`t0`t") { $sum; throw "overlay pipeline failed (see $cp\_work\final\ALL\run.log)" }
}
Step 'MSVC build + import audit' {
    Run-Child "$here\build.ps1" @('-OverlayDir', "$cp\_work\static_final\ALL", '-BuildDir', $BuildDir, '-OverlayOpt', $OverlayOpt)
}
"exe: $here\$BuildDir\psxrecomp-game.exe"
