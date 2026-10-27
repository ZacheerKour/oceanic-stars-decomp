<#
Player install: drag your two Star Ocean 2 (USA) disc files onto INSTALL.bat (or pass them as two arguments; with no
arguments a file dialog opens). Result: PLAY.exe in this folder. Console = short plain-ASCII progress; every tool's output
goes to install.log. Re-running skips finished stages (stamps in .install\).
Env flags (maintainer): SO2_VERBOSE=1 echoes the log to the console after each stage, SO2_DEV=1 lets an argument be a
folder holding the disc files. Never installs system software without a Y/N question.
#>
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Discs = @())
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = $PSScriptRoot
Set-Location $root
try { [Console]::OutputEncoding = New-Object Text.UTF8Encoding $false } catch {}
$log = Join-Path $root 'install.log'
$stampDir = Join-Path $root '.install'
New-Item -ItemType Directory -Force $stampDir | Out-Null
[IO.File]::WriteAllText($log, "Star Ocean 2 install log $(Get-Date -Format s)`r`n")
$verbose = [bool]$env:SO2_VERBOSE
$total = 6

$PsxCommit = 'f42d2c004b3dd6ef6ccb83101b40ede55f24bf59'   # audited pin (RECOMP_NOTES '2026-10-26 psxrecomp safety audit')
# Official release URLs. SHA-256 pins: SDL3/zlib = recomp\rtfull\build_deps.ps1; Python 3.11.9 embeddable (python.org, its published
# MD5 6d9aa08531d48fcc261ba667e2df17c4 matches); 7zr.exe 26.05 (github.com/ip7z/7zip release asset digest).
$Archives = [ordered]@{
    'SDL3-3.4.10.tar.gz' = @('https://github.com/libsdl-org/SDL/releases/download/release-3.4.10/SDL3-3.4.10.tar.gz',
                             '12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785')
    'zlib-1.3.1.tar.gz'  = @('https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz',
                             '9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23') }
$PyZip = @('https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip', '009d6bf7e3b2ddca3d784fa09f90fe54336d5b60f0e0f305c37f400bf83cfd3b')
$Z7r = @('https://github.com/ip7z/7zip/releases/download/26.05/7zr.exe', '9bf582b894dfa665f852d7e9112b02290a1c303c2d95a21fcdc41ffcb03143b7')
$DiscSha1 = @{ 'ccd092233b353d2aa7bb80ecdb2b76f456430b41' = 1; '68178c55a430eab3e97eb51ec4a164d36f228aa1' = 2 }   # recomp\game.toml known_sha1
$DiscName = 'Star Ocean - The Second Story (USA) (Disc {0})'

function Say($m) { Write-Host $m }
function Ascii($s) { ($s -replace '[^\x20-\x7e]', '?') }
function LogNote($m) { [IO.File]::AppendAllText($log, "`r`n>>> $m`r`n") }

function Fail($reason) {
    Write-Host ''
    Write-Host 'The install did not finish.' -ForegroundColor Red
    Write-Host (Ascii $reason)
    Write-Host ''
    Write-Host 'Last lines of the log:'
    try { Get-Content $log -Tail 15 | ForEach-Object { Write-Host ('  ' + (Ascii $_)) } } catch {}
    Write-Host ''
    Write-Host "Full log: $log"
    Write-Host 'Run INSTALL.bat again after fixing the problem; finished stages are skipped.'
    exit 1
}

# Runs a command line with all output appended to install.log, with a spinner and elapsed time on one console line.
function Invoke-Logged([string]$label, [string]$cmdline) {
    LogNote $label
    $before = (Get-Item $log).Length
    $p = Start-Process cmd.exe -ArgumentList ('/d /s /c "' + $cmdline + ' >> "' + $log + '" 2>&1 < nul"') -NoNewWindow -PassThru
    $null = $p.Handle
    $sw = [Diagnostics.Stopwatch]::StartNew(); $i = 0; $sub = ''; $spin = '|/-\'
    while (-not $p.HasExited) {
        if ($i % 8 -eq 0) {   # last "== step" line the tool wrote, as a hint
            try { $l = Get-Content $log -Tail 60 | Where-Object { $_ -match '^== ' } | Select-Object -Last 1; if ($l) { $sub = (Ascii $l).Substring(3) } } catch {}
        }
        $t = '  {0} {1:00}:{2:00} {3}' -f $spin[$i % 4], [int][math]::Floor($sw.Elapsed.TotalMinutes), $sw.Elapsed.Seconds, $sub
        Write-Host ("`r" + $t.PadRight(74).Substring(0, 74)) -NoNewline
        $i++; Start-Sleep -Milliseconds 250
    }
    Write-Host ("`r" + ' ' * 74 + "`r") -NoNewline
    $p.WaitForExit()
    if ($verbose) { try { Get-Content $log -Tail 40 | ForEach-Object { Write-Host (Ascii $_) } } catch {} }
    return $p.ExitCode
}
function Q([string]$s) { '"' + $s + '"' }
function Ask($q) { $a = Read-Host "$q [Y/N]"; return ($a -match '^\s*[yY]') }
function Download($url, $sha, $dest) {
    if (-not (Test-Path $dest)) {
        New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
        LogNote "download $url"
        try { Invoke-WebRequest -UseBasicParsing $url -OutFile "$dest.part" } catch { Fail "Could not download $url. Check your internet connection." }
        Move-Item -Force "$dest.part" $dest
    }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $dest).Hash.ToLower() -ne $sha) {
        Fail "The file $dest does not match its expected SHA-256 fingerprint ($sha), so it was not used. Delete that file and run INSTALL.bat again."
    }
}
# Stamp = hash of the tracked inputs (not generated _* folders), so updating the repo rebuilds only what changed.
function Tree-Stamp([string[]]$dirs, [string]$extra = '') {
    $sb = New-Object Text.StringBuilder
    foreach ($d in $dirs) {
        Get-ChildItem (Join-Path $root $d) -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName.Substring($root.Length) -notmatch '(^|\\)_' -and $_.Extension -notin '.log' } | Sort-Object FullName |
            ForEach-Object { [void]$sb.Append($_.FullName.Substring($root.Length)).Append(':').Append((Get-FileHash -Algorithm SHA1 -LiteralPath $_.FullName).Hash).Append("`n") }
    }
    [void]$sb.Append($extra)
    $h = [Security.Cryptography.SHA1]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($sb.ToString()))
    return ([BitConverter]::ToString($h) -replace '-', '')
}
function Stamp-Ok($name, $value) { $f = Join-Path $stampDir $name; (Test-Path $f) -and ((Get-Content $f -Raw).Trim() -eq $value) }
function Stamp-Set($name, $value) { [IO.File]::WriteAllText((Join-Path $stampDir $name), $value) }

function Pick-Files($title) {
    Add-Type -AssemblyName System.Windows.Forms
    $f = New-Object Windows.Forms.Form -Property @{ TopMost = $true; ShowInTaskbar = $false; Opacity = 0 }
    $d = New-Object Windows.Forms.OpenFileDialog
    $d.Title = $title; $d.Multiselect = $true
    $d.Filter = 'Star Ocean disc files|*.7z;*.zip;*.cue;*.bin;*.chd;*.iso|All files|*.*'
    $r = $d.ShowDialog($f); $f.Dispose()
    if ($r -eq [Windows.Forms.DialogResult]::OK) { return @($d.FileNames) } else { return @() }
}

Say 'Star Ocean: The Second Story - PC install'
Say "A detailed log is written to install.log."
Say ''

# ---------------------------------------------------------------- 1. discs
Say "[1/$total] Checking your discs..."
$discOk = { @(1, 2 | Where-Object { Test-Path ("roms\disc$_\$DiscName.cue" -f $_) }).Count -eq 2 }
$z7 = $null
function Get-7z {
    if ($script:z7) { return $script:z7 }
    $c = @("$env:ProgramFiles\7-Zip\7z.exe", "${env:ProgramFiles(x86)}\7-Zip\7z.exe", (Join-Path $root 'tools\7zr.exe')) | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $c) { Download $Z7r[0] $Z7r[1] (Join-Path $root 'tools\7zr.exe'); $c = Join-Path $root 'tools\7zr.exe' }
    $script:z7 = $c; return $c
}
if (& $discOk) { Say '  Both discs are already installed.' }
else {
    $tmp = Join-Path $root 'roms\_unpack'
    $given = @($Discs)
    $interactive = ($given.Count -eq 0)
    $attempt = 0
    while (-not (& $discOk)) {
        $attempt++
        if ($given.Count -eq 0) {
            if (-not $interactive -or $attempt -gt 3) { break }
            $have = @(1, 2 | Where-Object { Test-Path ("roms\disc$_\$DiscName.cue" -f $_) })
            $t = if ($have.Count -eq 0) { 'Select BOTH Star Ocean 2 disc files (hold Ctrl to pick two)' } else { "Select the file for disc $(3 - $have[0])" }
            Say "  $t..."
            $given = Pick-Files $t
            if ($given.Count -eq 0) { Fail 'No disc files were selected. Drag your two Star Ocean 2 (USA) disc files onto INSTALL.bat, or run it again and pick them in the window.' }
        }
        # expand arguments into candidate disc images
        $cands = New-Object Collections.ArrayList
        foreach ($g in $given) {
            if (-not (Test-Path -LiteralPath $g)) { Fail "Cannot find '$g'. Drag the disc files themselves onto INSTALL.bat." }
            $it = Get-Item -LiteralPath $g
            if ($it.PSIsContainer) {
                if ($env:SO2_DEV) { Get-ChildItem -LiteralPath $g -File | ForEach-Object { [void]$cands.Add($_.FullName) }; continue }
                Fail "'$($it.Name)' is a folder. Drag the two disc FILES (.7z, .zip, .bin/.cue or .chd) onto INSTALL.bat, not a folder."
            }
            [void]$cands.Add($it.FullName)
        }
        $images = New-Object Collections.ArrayList
        $k = 0
        foreach ($c in $cands) {
            $ext = [IO.Path]::GetExtension($c).ToLower(); $k++
            $dir = Join-Path $tmp "a$k"
            switch ($ext) {
                { $_ -in '.7z', '.zip' } {
                    Say "  Unpacking $([IO.Path]::GetFileName($c))..."
                    New-Item -ItemType Directory -Force $dir | Out-Null
                    if ($ext -eq '.zip') { try { Expand-Archive -LiteralPath $c -DestinationPath $dir -Force } catch { Fail "Could not open $c as a zip file." } }
                    else { $zz = Get-7z; $rc = Invoke-Logged "7z x $c" ((Q $zz) + ' x -y ' + (Q "-o$dir") + ' ' + (Q $c)); if ($rc) { Fail "Could not unpack $c. The file may be damaged or incomplete." } }
                    Get-ChildItem $dir -Recurse -File | Where-Object { $_.Extension -in '.bin', '.chd' } | ForEach-Object { [void]$images.Add($_.FullName) }
                }
                '.cue' {
                    $bins = @(Select-String -LiteralPath $c -Pattern '^\s*FILE\s+"([^"]+)"' | ForEach-Object { $_.Matches[0].Groups[1].Value })
                    if ($bins.Count -ne 1) { Fail "$([IO.Path]::GetFileName($c)) lists $($bins.Count) data files. Only single-file disc images (one .bin per disc) are supported." }
                    $b = Join-Path (Split-Path $c) $bins[0]
                    if (-not (Test-Path -LiteralPath $b)) { Fail "$([IO.Path]::GetFileName($c)) points at '$($bins[0])', which is not next to it." }
                    [void]$images.Add($b)
                }
                '.bin' { [void]$images.Add($c) }
                '.chd' { [void]$images.Add($c) }
                '.iso' { Fail "$([IO.Path]::GetFileName($c)) is a 2048-byte ISO. The game needs the raw (2352-byte) .bin dump of each disc, not an ISO." }
                default { Fail "$([IO.Path]::GetFileName($c)) is not a disc file I understand. Use the .7z, .zip, .bin/.cue or .chd of each disc." }
            }
        }
        $dups = @{}
        foreach ($im in ($images | Select-Object -Unique)) {
            $name = [IO.Path]::GetFileName($im)
            if ($im.ToLower().EndsWith('.chd')) {
                $cm = (Get-Command chdman.exe -ErrorAction SilentlyContinue)
                if (-not $cm) { Fail "$name is a CHD file and converting it needs chdman.exe (part of MAME), which was not found. Convert it to .bin/.cue first, or use the .7z/.bin files." }
                $d2 = Join-Path $tmp "chd$k"; $k++; New-Item -ItemType Directory -Force $d2 | Out-Null
                Say "  Converting $name..."
                $rc = Invoke-Logged "chdman $im" ((Q $cm.Source) + ' extractcd -i ' + (Q $im) + ' -o ' + (Q "$d2\d.cue") + ' -ob ' + (Q "$d2\d.bin"))
                if ($rc) { Fail "chdman could not convert $name." }
                $im = "$d2\d.bin"
            }
            Say "  Checking $name ($([int]((Get-Item -LiteralPath $im).Length / 1MB)) MB)..."
            $n = $DiscSha1[(Get-FileHash -Algorithm SHA1 -LiteralPath $im).Hash.ToLower()]
            if (-not $n) { Say "  '$name' is not a supported Star Ocean 2 disc. Use the unmodified USA dumps (SCUS-94421 and SCUS-94422); other regions and edited images do not work."; continue }
            if ($dups[$n] -or (Test-Path ("roms\disc$n\$DiscName.cue" -f $n))) { Say "  '$name' is disc $n, which you already gave. Disc $(3 - $n) is the other one."; continue }
            $dups[$n] = 1
            $d = "roms\disc$n"; $dn = $DiscName -f $n
            New-Item -ItemType Directory -Force $d | Out-Null
            if ($im.StartsWith($tmp)) { Move-Item -Force -LiteralPath $im "$d\$dn.bin" } else { Copy-Item -Force -LiteralPath $im "$d\$dn.bin" }
            [IO.File]::WriteAllText("$d\$dn.cue", "FILE `"$dn.bin`" BINARY`r`n  TRACK 01 MODE2/2352`r`n    INDEX 01 00:00:00`r`n")   # single-track MODE2/2352 image
            Say "  Disc $n recognised."
        }
        if (Test-Path $tmp) { try { Remove-Item -Recurse -Force $tmp } catch {} }
        $given = @()
        if (-not $interactive) { break }
    }
    foreach ($n in 1, 2) {
        if (-not (Test-Path ("roms\disc$n\$DiscName.cue" -f $n))) { Fail "Disc $n was not found among the files you gave. Star Ocean 2 (USA) has two discs; give me both (any order), as .7z or .bin/.cue files." }
    }
}

# ---------------------------------------------------------------- 2. tools
Say "[2/$total] Checking tools..."
$miss = $false
$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
function Winget-Install($what, $id, $override, $size) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) { Fail "$what is needed but winget is not available. Install $what yourself (see README), then run INSTALL.bat again." }
    if (-not (Ask "  $what is needed ($size). Install it now with winget? Windows will ask for your approval.")) { Fail "$what is needed to build the game. Install it (see README) and run INSTALL.bat again." }
    $cmd = "winget install --id $id -e --source winget --accept-package-agreements --accept-source-agreements"
    if ($override) { $cmd += " --override `"$override`"" }
    Say "  Installing $what (this can take a while)..."
    $rc = Invoke-Logged "winget $id" $cmd
    if ($rc) { Fail "Installing $what failed (winget exit code $rc)." }
}
if (-not (Test-Path "$vs\VC\Auxiliary\Build\vcvars64.bat")) {
    Winget-Install 'Visual Studio 2022 Build Tools (C++)' 'Microsoft.VisualStudio.2022.BuildTools' '--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended' 'about 8 GB'
    if (-not (Test-Path "$vs\VC\Auxiliary\Build\vcvars64.bat")) { Fail 'The Build Tools install did not finish (a restart may be needed). Restart Windows if asked, then run INSTALL.bat again.' }
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Winget-Install 'Git' 'Git.Git' '' 'about 60 MB'
    $env:Path = "$env:ProgramFiles\Git\cmd;$env:Path"
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail 'Git was installed but is not visible yet. Close this window and run INSTALL.bat again.' }
}
# Python: a private copy (embeddable zip, SHA-256 checked) so nothing needs installing system-wide.
$py = Join-Path $root 'tools\python'
if (-not (Test-Path "$py\python.exe")) {
    Say '  Fetching a private Python...'
    $zip = Join-Path $root 'tools\dl\python-embed.zip'
    Download $PyZip[0] $PyZip[1] $zip
    Expand-Archive -LiteralPath $zip -DestinationPath $py -Force
}
$env:Path = "$py;$env:Path"
$env:SO2_DATA = $root
$env:PYTHONUTF8 = '1'

# ---------------------------------------------------------------- 3. unpack
Say "[3/$total] Reading game data from the discs..."
if (-not (Test-Path extracted\SCUS_944.21)) {
    New-Item -ItemType Directory -Force extracted | Out-Null
    $rc = Invoke-Logged 'boot exe' ((Q "$py\python.exe") + ' -I analysis\extract_boot.py ' + (Q ("roms\disc1\$DiscName.bin" -f 1)) + ' extracted\SCUS_944.21')
    if ($rc) { Fail 'Could not read the boot program from disc 1.' }
}
if (-not (Test-Path analysis\out\d1\0001_312.dec)) {
    $rc = Invoke-Logged 'extract' ((Q "$py\python.exe") + ' -E -s analysis\extract.py')
    if ($rc) { Fail 'Could not unpack the game archive from the discs. The disc images may be damaged.' }
}

# ---------------------------------------------------------------- 4. downloads
Say "[4/$total] Downloading build components (checked against fixed fingerprints)..."
$px = 'tools\ref\psxrecomp'
if (-not (Test-Path "$px\.git")) {
    $rc = Invoke-Logged 'clone psxrecomp' ('git clone -q --filter=blob:none --no-checkout https://github.com/RetroPortingToolKit/psxrecomp ' + (Q $px))
    if ($rc) { Fail 'Could not download psxrecomp from github.com. Check your internet connection.' }
}
if ((& git -C $px rev-parse HEAD 2>$null) -ne $PsxCommit) {
    $rc = Invoke-Logged 'checkout psxrecomp' ('git -C ' + (Q $px) + " -c advice.detachedHead=false checkout -q $PsxCommit")
    if ($rc) { Fail "Could not check out the pinned psxrecomp version $PsxCommit." }
}
foreach ($k2 in $Archives.Keys) { Download $Archives[$k2][0] $Archives[$k2][1] (Join-Path $root "tools\dl\$k2") }

# ---------------------------------------------------------------- 5. build
$bios = if (Test-Path roms\scph5501.bin) { 'ownbios' } else { 'openbios' }
$buildStamp = Tree-Stamp @('recomp', 'analysis', 'config') "$PsxCommit $bios"
$exe = 'recomp\rtfull\_build_final\psxrecomp-game.exe'
if ((Test-Path $exe) -and (Stamp-Ok 'build.stamp' $buildStamp)) { Say "[5/$total] Game code is up to date." }
else {
    Say "[5/$total] Building game code (about 10 minutes)..."
    $rc = Invoke-Logged 'build_all' ('powershell -NoProfile -ExecutionPolicy Bypass -File ' + (Q 'recomp\rtfull\build_all.ps1'))
    if ($rc) { Fail 'Building the game code failed. The log below usually names the cause (common ones: Build Tools missing the C++ workload, low disk space, antivirus blocking the compiler).' }
    if (-not (Test-Path $exe)) { Fail 'The build finished without producing the game program.' }
    Stamp-Set 'build.stamp' $buildStamp
}

# ---------------------------------------------------------------- 6. PLAY.exe
$launchStamp = Tree-Stamp @('recomp\launcher')
if ((Test-Path PLAY.exe) -and (Stamp-Ok 'launcher.stamp' $launchStamp)) { Say "[6/$total] PLAY.exe is up to date." }
else {
    Say "[6/$total] Building PLAY.exe..."
    $rc = Invoke-Logged 'PLAY.exe' ('powershell -NoProfile -ExecutionPolicy Bypass -File ' + (Q 'recomp\launcher\build.ps1'))
    if ($rc) { Fail 'Building PLAY.exe failed.' }
    if (-not (Test-Path PLAY.exe)) { Fail 'The launcher build finished but PLAY.exe does not exist.' }
    Stamp-Set 'launcher.stamp' $launchStamp
}

$playExe = (Resolve-Path PLAY.exe).Path
Say ''
Say "Done. Double-click PLAY.exe:"
Say "  $playExe"
if ([Environment]::UserInteractive -and -not $env:SO2_NOASK) {
    if (Ask 'Create a Desktop shortcut?') {
        try {
            $sh = (New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'Star Ocean 2.lnk'))
            $sh.TargetPath = $playExe; $sh.WorkingDirectory = $root; $sh.Save()
            Say '  Shortcut created on the Desktop.'
        } catch { Say '  Could not create the shortcut; start PLAY.exe from the folder above.' }
    }
}
exit 0
