<#
Player install: from this checkout + your own two USA discs to a built recomp\rtfull\_build_final\psxrecomp-game.exe.
  INSTALL.bat <folder with the discs>   or drag the disc files (.7z or .bin/.cue) onto INSTALL.bat
Steps: prerequisites (only checked, never installed) -> discs into roms\ (identified by SHA-1, see recomp\game.toml)
-> boot exe + SO2.BIN entries extracted locally -> psxrecomp at the pinned commit, SDL3/zlib archives (SHA-256 pinned)
-> recomp\rtfull\build_all.ps1. Everything it writes is gitignored and stays on this machine.
Re-running is safe: finished steps are skipped.
#>
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Discs = @())
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
Set-Location $root
$PsxCommit = 'f42d2c004b3dd6ef6ccb83101b40ede55f24bf59'   # audited pin (RECOMP_NOTES '2026-10-26 psxrecomp safety audit')
$Archives = [ordered]@{   # official release URLs; the hashes are the same pins as recomp\rtfull\build_deps.ps1
    'SDL3-3.4.10.tar.gz' = @('https://github.com/libsdl-org/SDL/releases/download/release-3.4.10/SDL3-3.4.10.tar.gz',
                             '12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785')
    'zlib-1.3.1.tar.gz'  = @('https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz',
                             '9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23') }
$DiscSha1 = @{ 'ccd092233b353d2aa7bb80ecdb2b76f456430b41' = 1; '68178c55a430eab3e97eb51ec4a164d36f228aa1' = 2 }   # recomp\game.toml known_sha1
$DiscName = 'Star Ocean - The Second Story (USA) (Disc {0})'
function Step($n) { Write-Host "== $n" -ForegroundColor Cyan }
function Fail($m) { Write-Host "INSTALL FAILED: $m" -ForegroundColor Red; exit 1 }

try {
    Step 'prerequisites'
    $miss = @()
    $vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
    if (-not (Test-Path "$vs\VC\Auxiliary\Build\vcvars64.bat")) {
        $miss += 'Visual Studio 2022 Build Tools (C++ workload):  winget install Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"' }
    $py = Get-Command python -ErrorAction SilentlyContinue
    if (-not $py -or -not ((& python -c "import sys; print(sys.version_info >= (3, 11))" 2>$null) -eq 'True')) {
        $miss += 'Python 3.11 or newer:  winget install Python.Python.3.11' }
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { $miss += 'Git:  winget install Git.Git' }
    if ($miss) { $miss | ForEach-Object { Write-Host "  missing: $_" }; Fail 'install the programs above, open a new window, run INSTALL.bat again' }

    Step 'discs'
    $have = @(1, 2 | Where-Object { Test-Path ("roms\disc$_\$DiscName.cue" -f $_) })
    if ($have.Count -lt 2) {
        if (-not $Discs) { Fail 'give the folder with your two discs: INSTALL.bat "<folder>" (or drag the files onto INSTALL.bat)' }
        $files = @($Discs | ForEach-Object { if (Test-Path $_ -PathType Container) { Get-ChildItem $_ -File } else { Get-Item $_ } })
        $z7 = @("$env:ProgramFiles\7-Zip\7z.exe", "${env:ProgramFiles(x86)}\7-Zip\7z.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
        $tmp = Join-Path $root 'roms\_unpack'
        foreach ($a in $files | Where-Object { $_.Extension -in '.7z', '.zip' }) {
            if (-not $z7) { Fail "$($a.Name) is an archive and 7-Zip is not installed:  winget install 7zip.7zip  (or extract it yourself and pass the .bin files)" }
            Write-Host "  extracting $($a.Name)"
            & $z7 x -y "-o$tmp" $a.FullName | Out-Null
            if ($LASTEXITCODE) { Fail "7-Zip could not extract $($a.FullName)" }
        }
        $bins = @($files | Where-Object Extension -eq '.bin') + @(Get-ChildItem $tmp -Recurse -Filter *.bin -ErrorAction SilentlyContinue)
        foreach ($b in $bins) {
            Write-Host "  checking $($b.Name) ($([int]($b.Length / 1MB)) MB)"
            $n = $DiscSha1[(Get-FileHash -Algorithm SHA1 -LiteralPath $b.FullName).Hash.ToLower()]
            if (-not $n) { Write-Host "    not a known Star Ocean 2 USA disc image (SHA-1 differs), skipped"; continue }
            $d = "roms\disc$n"; $name = $DiscName -f $n
            New-Item -ItemType Directory -Force $d | Out-Null
            if ($b.FullName.StartsWith($tmp)) { Move-Item -Force $b.FullName "$d\$name.bin" } else { Copy-Item -Force $b.FullName "$d\$name.bin" }
            # single-track MODE2/2352 image: the cue is just this
            Set-Content -Encoding ascii "$d\$name.cue" "FILE `"$name.bin`" BINARY`r`n  TRACK 01 MODE2/2352`r`n    INDEX 01 00:00:00"
            Write-Host "    disc $n OK"
        }
        foreach ($n in 1, 2) { if (-not (Test-Path ("roms\disc$n\$DiscName.cue" -f $n))) { Fail "disc $n not found among the given files" } }
    } else { Write-Host '  both discs already in roms\' }
    if (Test-Path roms\scph5501.bin) { Write-Host '  roms\scph5501.bin present: your own SCPH-5501 BIOS will be used' }
    else { Write-Host '  no roms\scph5501.bin: the bundled OpenBIOS will be used' }

    Step 'local extraction (boot exe, SO2.BIN entries)'
    if (-not (Test-Path extracted\SCUS_944.21)) {
        New-Item -ItemType Directory -Force extracted | Out-Null
        & python -I -c @"
import sys; sys.path.insert(0, 'analysis'); import iso
d = iso.Disc(r'roms\disc1\Star Ocean - The Second Story (USA) (Disc 1).bin')
f = {n.upper(): (l, s) for n, l, s, fl in d.walk()[1]}
l, s = f['/SCUS_944.21;1']
open(r'extracted\SCUS_944.21', 'wb').write(d.read(l, s))
"@
        if ($LASTEXITCODE) { Fail 'could not read SCUS_944.21 from disc 1' }
    }
    if (-not (Test-Path analysis\out\d1\0001_312.dec)) {
        & python -E -s analysis\extract.py   # not -I: it imports its siblings; paths are relative to the repo root
        if ($LASTEXITCODE) { Fail 'analysis\extract.py failed' }
    }

    Step 'psxrecomp (pinned) and SDL3/zlib sources'
    $px = 'tools\ref\psxrecomp'
    if (-not (Test-Path "$px\.git")) {
        & git clone -q --filter=blob:none --no-checkout https://github.com/RetroPortingToolKit/psxrecomp $px
        if ($LASTEXITCODE) { Fail 'git clone of psxrecomp failed (network?)' }
    }
    if ((& git -C $px rev-parse HEAD 2>$null) -ne $PsxCommit) {
        & git -C $px -c advice.detachedHead=false checkout -q $PsxCommit
        if ($LASTEXITCODE) { Fail "psxrecomp checkout of $PsxCommit failed" }
    }
    New-Item -ItemType Directory -Force tools\dl | Out-Null
    foreach ($k in $Archives.Keys) {
        $f = "tools\dl\$k"; $url, $sha = $Archives[$k]
        if (-not (Test-Path $f)) { Write-Host "  downloading $url"; Invoke-WebRequest -UseBasicParsing $url -OutFile "$f.part"; Move-Item -Force "$f.part" $f }
        if ((Get-FileHash -Algorithm SHA256 $f).Hash.ToLower() -ne $sha) { Fail "$f does not match its SHA-256 pin $sha (delete it and retry)" }
    }

    Step 'build (recomp\rtfull\build_all.ps1; first run takes a few minutes)'
    $env:SO2_DATA = $root
    & powershell -NoProfile -ExecutionPolicy Bypass -File recomp\rtfull\build_all.ps1
    if ($LASTEXITCODE) { Fail 'build failed (logs under recomp\rtfull\_build_final\ and recomp\host\_out\)' }
    if (-not (Test-Path recomp\rtfull\_build_final\psxrecomp-game.exe)) { Fail 'no exe after the build' }
    Step 'PLAY.exe (Steam-friendly launcher)'
    & powershell -NoProfile -ExecutionPolicy Bypass -File recomp\launcheruild.ps1
    if ($LASTEXITCODE) { Write-Host 'PLAY.exe build failed; PLAY.bat still works' -ForegroundColor Yellow }
    Write-Host 'INSTALL OK - double-click PLAY.exe (or PLAY.bat)' -ForegroundColor Green
} catch { Fail "$_" }
