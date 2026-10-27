<#
Scripted run of a build dir for the quick-save tests (headless, or a hidden window with -Hidden so SDL key events are pumped).
Usage: qrun.ps1 -Script <input script> -Run <dir name under _run> [-Seconds 120] [-Hidden] [-Env 'NAME=value',...]
       [-Bios <retail BIOS image, e.g. roms\scph5501.bin>] [-Card <.mcd copied in as card 1>]
Inputs: $env:SO2_DATA (main checkout with roms\), $env:SO2_GEN (checkout holding recomp\host\_out\proj\disc\SCUS_944.21).
Run dir _run\<Run>: game.toml, saves\ (memcard dir, states in saves\states\), log.txt, dumps (vram_<tag>.ppm, menu_<tag>.ppm).
#>
param([string]$Script, [string]$Run = 'q', [int]$Seconds = 120, [switch]$Hidden, [string[]]$Env = @(), [switch]$KeepSaves, [string]$BuildDir = '_build_final', [string]$Bios = '', [string]$Card = '')
$here = $PSScriptRoot; $repo = (Resolve-Path (Join-Path $here '..\..')).Path
$data = (Resolve-Path $env:SO2_DATA).Path; $gen = if ($env:SO2_GEN) { (Resolve-Path $env:SO2_GEN).Path } else { $repo }
$B = Join-Path $here $BuildDir; $R = Join-Path $here "_run\$Run"
New-Item -ItemType Directory -Force $R, "$B\bios" | Out-Null
if (-not $KeepSaves -and (Test-Path "$R\saves")) { Remove-Item -Recurse -Force "$R\saves" }
New-Item -ItemType Directory -Force "$R\saves" | Out-Null
Copy-Item (Join-Path $data 'tools\ref\psxrecomp\bios\openbios.bin') "$B\bios\openbios.bin" -Force
if ($Card) { Copy-Item $Card "$R\saves\card1.mcd" -Force }   # a COPY of a memory card image as card 1
$d1 = (Join-Path $data 'roms\disc1\Star Ocean - The Second Story (USA) (Disc 1).cue').Replace([string][char]92, '/')
$d2 = (Join-Path $data 'roms\disc2\Star Ocean - The Second Story (USA) (Disc 2).cue').Replace([string][char]92, '/')
$exe = (Join-Path $gen 'recomp\host\_out\proj\disc\SCUS_944.21').Replace([string][char]92, '/')
$t = Get-Content (Join-Path $repo 'recomp\game.toml') -Raw
$t = $t.Replace('"disc/Star Ocean - The Second Story (USA) (Disc 1).cue"', "`"$d1`"").Replace('"disc/Star Ocean - The Second Story (USA) (Disc 2).cue"', "`"$d2`"").Replace('exe = "disc/SCUS_944.21"', "exe = `"$exe`"")
if (-not $Bios) { $t = $t -replace '(?m)^openbios = false', 'openbios = true ' }   # -Bios <retail image>: keep game.toml's LLE retail BIOS
[IO.File]::WriteAllText("$R\game.toml", $t)
$a = @('--no-launcher', '--game', "$R\game.toml", '--memcard-dir', "$R\saves", '--bios', $(if ($Bios) { (Resolve-Path $Bios).Path } else { "$B\bios\openbios.bin" }))
if (-not $Hidden) { $a = @('--headless') + $a }
$env:SDL_AUDIODRIVER = 'dummy'   # tests must never play through the user's speakers
$env:PSXRT_INPUT_SCRIPT = (Resolve-Path $Script).Path
if ($Hidden) { $env:PSX_HIDDEN_WINDOW = '1' }
foreach ($e in $Env) { $k, $v = $e -split '=', 2; Set-Item "env:$k" $v }
$p = Start-Process -FilePath "$B\psxrecomp-game.exe" -ArgumentList ($a | % { "`"$_`"" }) -WorkingDirectory $R -RedirectStandardOutput "$R\out.txt" -RedirectStandardError "$R\err.txt" -PassThru -NoNewWindow
if (-not $p.WaitForExit($Seconds * 1000)) { $p.Kill(); "timeout after $Seconds s (killed)" } else { "exit code $($p.ExitCode)" }
Get-Content "$R\out.txt", "$R\err.txt" | Set-Content "$R\log.txt"
"log: $R\log.txt"
