<#
Starts the built game windowed (PLAY.bat). Settings: settings.ini (created from settings.default.ini on first start).
Memory cards and savestates: saves\. Run files (game.toml, logs): play\. Works from any directory.
#>
param([switch]$Prepare)   # -Prepare (used by PLAY.exe): write play\launch.txt instead of starting the game
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$exe = Join-Path $root 'recomp\rtfull\_build_final\psxrecomp-game.exe'
$psx = Join-Path $root 'tools\ref\psxrecomp'
$name = 'Star Ocean - The Second Story (USA) (Disc {0})'
function Fail($m) {
    if ($Prepare) { New-Item -ItemType Directory -Force (Join-Path $root 'play') | Out-Null; [IO.File]::WriteAllText((Join-Path $root 'play\prepare.err'), $m) }
    else { Write-Host "PLAY: $m" -ForegroundColor Red }
    exit 1 }
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
# Controllers: P1 = first SDL gamepad (hot-plug), digital pad so the left stick folds onto the d-pad. The keyboard stays live.
$dz = if ($set['pad.deadzone']) { [int]$set['pad.deadzone'] } else { 9000 }
$t += "`n[controller]`np1_device = `"auto`"`np1_mode = `"digital`"`ndeadzone = $dz`n"
[IO.File]::WriteAllText((Join-Path $P 'game.toml'), $t)

# Keyboard: key.<button> = <key>[, <alt key>] in settings.ini (SDL key names); defaults below. Rewritten into keybinds.ini each start.
$keys = [ordered]@{ up = 'W, Up'; down = 'S, Down'; left = 'A, Left'; right = 'D, Right'
    cross = 'Space, 1'; circle = 'Q, 2'; square = 'E, 3'; triangle = 'R, 4'
    l1 = 'Left Shift, Z'; r1 = 'F, X'; l2 = 'Tab, C'; r2 = 'G, V'; l3 = 'T'; r3 = 'Y'
    start = 'Escape, Return'; select = 'Backspace, Right Shift' }
$kb = "; written by play.ps1 from settings.ini (key.* lines); edit settings.ini, not this file`n[player1]`n"
foreach ($k in $keys.Keys) { $v = $set["key.$k"]; if (-not $v) { $v = $keys[$k] }; $kb += "$k = $v`n" }
foreach ($k in 'ls_up', 'ls_down', 'ls_left', 'ls_right', 'rs_up', 'rs_down', 'rs_left', 'rs_right') { $kb += "$k = None`n" }
[IO.File]::WriteAllText((Join-Path (Split-Path $exe) 'keybinds.ini'), $kb)

# Odd gamepads: gamecontrollerdb.txt next to PLAY.bat or the game exe (SDL mapping lines; env SDL_GAMECONTROLLERCONFIG also works).
foreach ($d in @($root, (Split-Path $exe))) { $f = Join-Path $d 'gamecontrollerdb.txt'
    if ((Test-Path $f) -and -not $env:SDL_GAMECONTROLLERCONFIG_FILE) { $env:SDL_GAMECONTROLLERCONFIG_FILE = $f } }

foreach ($k in $set.Keys) { if ($k -like 'env.*') { Set-Item "env:$($k.Substring(4))" $set[$k] } }   # env.PSX_VSYNC = 1 etc.
$a = @('--no-launcher', '--game', (Join-Path $P 'game.toml'), '--memcard-dir', $S, '--bios', (Resolve-Path $bios).Path)
if ($set['extra_args']) { $a += $set['extra_args'] -split '\s+' }
if ($Prepare) {   # PLAY.exe starts the game itself (a direct child, so Steam sees the process): exe, args, run dir, env lines
    $env_ = foreach ($k in $set.Keys) { if ($k -like 'env.*') { "$($k.Substring(4))=$($set[$k])" } }
    if ($env:SDL_GAMECONTROLLERCONFIG_FILE) { $env_ += "SDL_GAMECONTROLLERCONFIG_FILE=$env:SDL_GAMECONTROLLERCONFIG_FILE" }
    $argline = ($a | ForEach-Object { "`"$_`"" }) -join ' '
    $lines = @($exe, $argline, $P) + @($env_)
    [IO.File]::WriteAllText((Join-Path $P 'launch.txt'), ($lines -join "`n") + "`n", (New-Object Text.UTF8Encoding $false))
    exit 0
}
Write-Host "Starting Star Ocean: The Second Story (BIOS: $(Split-Path -Leaf $bios), saves: $S)"
Start-Process -FilePath $exe -ArgumentList ($a | ForEach-Object { "`"$_`"" }) -WorkingDirectory $P `
    -RedirectStandardOutput (Join-Path $P 'last.log') -RedirectStandardError (Join-Path $P 'last.err') | Out-Null
