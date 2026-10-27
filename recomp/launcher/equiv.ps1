<# Equivalence check: play.ps1 -Prepare vs PLAYTHISGAME.exe --prepare on the same settings.ini, in two throwaway trees under %TEMP%.
   Run: powershell -ExecutionPolicy Bypass -File recomp\launcher\equiv.ps1   (needs PLAYTHISGAME.exe built). Prints DIFF lines; empty = identical.
   Env lines of launch.txt are sorted before comparing (play.ps1's hashtable order is unspecified). #>
$ErrorActionPreference = 'Continue'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$name = 'Star Ocean - The Second Story (USA) (Disc {0})'
$cases = [ordered]@{
  defaults = (Get-Content (Join-Path $repo 'settings.default.ini') -Raw)
  bad_values = "window_width = 99`nrender_scale = 9`nfilter = sharp`nscale_mode = zoom`n"
  good_mixed_case = "  WINDOW_WIDTH = 1280  `r`nRender_Scale = 3`r`nFilter = NEAREST`r`nscale_mode = Integer`r`n; c = d`r`nkey.cross = K, J`r`nkey.l3 =`r`n"
  env_extra = "env.PSX_VSYNC = 1`nenv.X.Y = a b`nextra_args =   --foo   bar  `nbios = openbios`npad.deadzone = 5000`naudio.buffer_ms = 250`nwindow_width = 7680`n"
  last_wins = "window_width = 800`nwindow_width = 1024`nbios = nope.bin`n"
  audio_out_of_range = "audio.buffer_ms = 9999`npad.deadzone = abc`n"   # intentional difference
}
foreach ($c in $cases.Keys) {
  $out = @{}
  foreach ($mode in 'ps', 'exe') {
    $r = Join-Path $env:TEMP "so2_equiv_${mode}"
    New-Item -ItemType Directory -Force $r | Out-Null
    Remove-Item (Join-Path $r 'play'), (Join-Path $r 'recomp\rtfull\_build_final\*.ini') -Recurse -Force -ErrorAction SilentlyContinue
    foreach ($f in @{ "recomp\rtfull\_build_final\psxrecomp-game.exe" = 'x'; "roms\disc1\$($name -f 1).cue" = 'x'; "roms\disc2\$($name -f 2).cue" = 'x'; 'tools\ref\psxrecomp\bios\openbios.bin' = 'b' }.GetEnumerator()) {
      $p = Join-Path $r $f.Key; New-Item -ItemType Directory -Force (Split-Path $p) | Out-Null; [IO.File]::WriteAllText($p, $f.Value) }
    New-Item -ItemType Directory -Force (Join-Path $r 'recomp') | Out-Null
    Copy-Item (Join-Path $repo 'recomp\game.toml') (Join-Path $r 'recomp\game.toml') -Force
    Copy-Item (Join-Path $repo 'settings.default.ini') $r -Force; Copy-Item (Join-Path $repo 'play.ps1') $r -Force
    [IO.File]::WriteAllText((Join-Path $r 'settings.ini'), $cases[$c])
    if ($mode -eq 'ps') { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $r 'play.ps1') -Prepare 2>$null >$null; $rc = $LASTEXITCODE }
    else { $pr = Start-Process (Join-Path $repo 'PLAYTHISGAME.exe') -ArgumentList '--prepare', '--root', "`"$r`"" -Wait -PassThru -WindowStyle Hidden; $rc = $pr.ExitCode }
    $txt = "rc=$rc`n"
    foreach ($f in 'play\game.toml', 'play\settings.warn', 'play\prepare.err', 'play\launch.txt', 'recomp\rtfull\_build_final\keybinds.ini', 'recomp\rtfull\_build_final\config.ini') {
      $p = Join-Path $r $f; $b = if (Test-Path $p) { [IO.File]::ReadAllText($p) } else { '<absent>' }
      if ($f -eq 'play\launch.txt') { $l = $b -split "`n"; $b = (($l[0..2]) + (($l[3..($l.Count - 1)] | Where-Object { $_ }) | Sort-Object)) -join "`n" }
      $txt += "--- $f`n" + $b.Replace($r, "<ROOT>").Replace($r.Replace("\","/"), "<ROOT>") + "`n" }
    $out[$mode] = $txt -split "`n"
  }
  $d = Compare-Object $out.ps $out.exe
  if ($d) { "DIFF [$c]"; $d | ForEach-Object { "  $($_.SideIndicator) $($_.InputObject)" } } else { "SAME [$c]" }
}
