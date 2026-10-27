<#
Headless boot of recomp\rtfull\_build\psxrecomp-game.exe for a bounded time. Run dir recomp\rtfull\_run\ (gitignored): game.toml
is recomp\game.toml with the exe/disc paths pointed at the local dumps. Log: _run\boot_<bios>.log.
Usage: $env:SO2_DATA=<main checkout>; powershell -NoProfile -ExecutionPolicy Bypass -File recomp\rtfull\run.ps1 [-Bios openbios|<path>] [-Seconds 60]
#>
param([string]$Bios = 'openbios', [int]$Seconds = 60, [string]$GenOut = '_out', [string[]]$Extra = @(), [switch]$Window)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$data = if ($env:SO2_DATA) { (Resolve-Path $env:SO2_DATA).Path } else { $repo }
$B = Join-Path $here '_build'; $R = Join-Path $here '_run'
New-Item -ItemType Directory -Force $R, "$B\bios" | Out-Null
Copy-Item (Join-Path $data 'tools\ref\psxrecomp\bios\openbios.bin') "$B\bios\openbios.bin" -Force
$d1 = (Join-Path $data 'roms\disc1\Star Ocean - The Second Story (USA) (Disc 1).cue') -replace '\\', '/'
$d2 = (Join-Path $data 'roms\disc2\Star Ocean - The Second Story (USA) (Disc 2).cue') -replace '\\', '/'
$exe = (Join-Path $repo "recomp\host\$GenOut\proj\disc\SCUS_944.21") -replace '\\', '/'
$t = Get-Content (Join-Path $repo 'recomp\game.toml') -Raw
$t = $t.Replace('"disc/Star Ocean - The Second Story (USA) (Disc 1).cue"', "`"$d1`"").Replace('"disc/Star Ocean - The Second Story (USA) (Disc 2).cue"', "`"$d2`"").Replace('exe = "disc/SCUS_944.21"', "exe = `"$exe`"")
[IO.File]::WriteAllText("$R\game.toml", $t)
New-Item -ItemType Directory -Force "$R\memcard" | Out-Null
$a = @('--no-launcher', '--game', "$R\game.toml", '--memcard-dir', "$R\memcard") + $Extra
if (-not $Window) { $a = @('--headless') + $a }
$tag = 'openbios'
if ($Bios -eq 'openbios') { $a += @('--bios', "$B\bios\openbios.bin") }  # --memcard-dir also moves bios.cfg
else { $a += @('--bios', (Resolve-Path $Bios).Path); $tag = [IO.Path]::GetFileNameWithoutExtension($Bios) }
$log = "$R\boot_$tag.log"
$p = Start-Process -FilePath "$B\psxrecomp-game.exe" -ArgumentList ($a | ForEach-Object { "`"$_`"" }) -WorkingDirectory $R `
     -RedirectStandardOutput "$log.out" -RedirectStandardError "$log.err" -PassThru -NoNewWindow
if (-not $p.WaitForExit($Seconds * 1000)) { $p.Kill(); "timeout after $Seconds s (killed)" } else { "exit code $($p.ExitCode)" }
Get-Content "$log.out", "$log.err" | Set-Content $log
"log: $log ($((Get-Content $log).Count) lines)"
