<#
Starts the built game windowed (PLAY.bat). Settings: settings.ini (created from settings.default.ini on first start).
Memory cards and savestates: saves\. Run files (game.toml, logs): play\. Works from any directory.
#>
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$exe = Join-Path $root 'recomp\rtfull\_build_final\psxrecomp-game.exe'
$psx = Join-Path $root 'tools\ref\psxrecomp'
$name = 'Star Ocean - The Second Story (USA) (Disc {0})'
function Fail($m) { Write-Host "PLAY: $m" -ForegroundColor Red; exit 1 }
if (-not (Test-Path $exe)) { Fail 'the game is not built yet: run INSTALL.bat first' }
$cue = foreach ($n in 1, 2) { $c = Join-Path $root ("roms\disc$n\$name.cue" -f $n); if (-not (Test-Path $c)) { Fail "missing $($c): run INSTALL.bat with your discs" }; $c -replace '\\', '/' }

$ini = Join-Path $root 'settings.ini'
if (-not (Test-Path $ini)) { Copy-Item (Join-Path $root 'settings.default.ini') $ini }
$set = @{}
foreach ($l in Get-Content $ini) { if ($l -match '^\s*([A-Za-z_][\w.]*)\s*=\s*(.*?)\s*$') { $set[$Matches[1]] = $Matches[2] } }

$P = Join-Path $root 'play'; $S = Join-Path $root 'saves'
New-Item -ItemType Directory -Force $P, $S, (Join-Path (Split-Path $exe) 'bios') | Out-Null
$bios = "$($set['bios'])"; if (-not $bios) { $bios = 'auto' }
$own = Join-Path $root 'roms\scph5501.bin'
if ($bios -eq 'auto') {   # own dump only when the build linked its backend (build_all saw it)
    $bios = if ((Test-Path $own) -and (Test-Path (Join-Path $root 'recomp\bios\_gen\generated\SCPH5501_full.c'))) { $own } else { 'openbios' } }
$t = Get-Content (Join-Path $root 'recomp\game.toml') -Raw
$t = $t.Replace("`"disc/$($name -f 1).cue`"", "`"$($cue[0])`"").Replace("`"disc/$($name -f 2).cue`"", "`"$($cue[1])`"")
$t = $t.Replace('exe = "disc/SCUS_944.21"', "exe = `"$((Join-Path $root 'recomp\host\_out\proj\disc\SCUS_944.21') -replace '\\', '/')`"")
if ($bios -eq 'openbios') {
    $t = $t -replace '(?m)^openbios = false', 'openbios = true '   # recomp/game.toml pins retail; OpenBIOS must be allowed
    $bios = Join-Path (Split-Path $exe) 'bios\openbios.bin'
    Copy-Item (Join-Path $psx 'bios\openbios.bin') $bios -Force
} elseif (-not (Test-Path $bios)) { Fail "settings.ini bios = $bios does not exist" }
[IO.File]::WriteAllText((Join-Path $P 'game.toml'), $t)

foreach ($k in $set.Keys) { if ($k -like 'env.*') { Set-Item "env:$($k.Substring(4))" $set[$k] } }   # env.PSX_VSYNC = 1 etc.
$a = @('--no-launcher', '--game', (Join-Path $P 'game.toml'), '--memcard-dir', $S, '--bios', (Resolve-Path $bios).Path)
if ($set['extra_args']) { $a += $set['extra_args'] -split '\s+' }
Write-Host "Starting Star Ocean: The Second Story (BIOS: $(Split-Path -Leaf $bios), saves: $S)"
Start-Process -FilePath $exe -ArgumentList ($a | ForEach-Object { "`"$_`"" }) -WorkingDirectory $P `
    -RedirectStandardOutput (Join-Path $P 'last.log') -RedirectStandardError (Join-Path $P 'last.err') | Out-Null
