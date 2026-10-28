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
$vs = if ($env:SO2_VS) { $env:SO2_VS } else { 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' }   # INSTALL.exe sets SO2_VS (vswhere)
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
    function Test-Clone($d) { if (-not (Test-Path "$d\.git")) { return $false }; $h = & git --git-dir="$d\.git" rev-parse --verify -q 'HEAD^{commit}'; return ($LASTEXITCODE -eq 0 -and "$h" -match '^[0-9a-f]{40}$') }
    function Set-Aside($d, $why) {   # never deletes: renames and lists the leftover for the player
        $n = 1; while (Test-Path "${d}_broken_$n") { $n++ }
        Move-Item $d "${d}_broken_$n"
        $rel = "${d}_broken_$n".Substring($repo.Length + 1)
        New-Item -ItemType Directory -Force (Join-Path $repo '.install') | Out-Null
        Add-Content (Join-Path $repo '.install\CLEANUP_LATER.md') "- $rel ($why)"
        "$why. Kept it as $rel and making a new copy."
    }
    $good = (Test-Clone $cp) -and ((& git --git-dir="$cp\.git" rev-parse HEAD) -eq $Commit)
    if ($good) { & git -C $cp apply --reverse --check $patch; $good = ($LASTEXITCODE -eq 0) }   # carries exactly the patch
    if (-not $good -and (Test-Path $cp)) {
        $why = if (-not (Test-Clone $cp)) { 'the earlier overlay work copy is incomplete (interrupted clone)' } else { "the earlier overlay work copy is not psxrecomp $($Commit.Substring(0,8)) plus psxrecomp_overlay.patch" }
        Set-Aside $cp $why
    }
    if (-not (Test-Path $cp)) {
        # the pinned clone must hold the pinned commit (the installer fetches it; a hand-made clone may predate it)
        & git --git-dir="$src\.git" cat-file -e "$Commit^{commit}"
        if ($LASTEXITCODE) { "psxrecomp at $src lacks $($Commit.Substring(0,8)); fetching it"; & git -C $src fetch -q origin $Commit; if ($LASTEXITCODE) { throw "cannot fetch psxrecomp $Commit into $src (offline?)" } }
        & git clone -q --no-checkout $src $cp; if ($LASTEXITCODE) { throw "git clone of $src into $cp failed" }
        & git -C $cp checkout -q $Commit; if ($LASTEXITCODE) { throw "checkout of $Commit in $cp failed" }
        & git -C $cp apply $patch; if ($LASTEXITCODE) { throw 'psxrecomp_overlay.patch did not apply' }
    }
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
