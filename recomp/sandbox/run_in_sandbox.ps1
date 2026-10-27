# Runs INSIDE Windows Sandbox (LogonCommand). Offline. Builds psxrecomp-analyze and psxrecomp-game (the generator) only,
# runs the analyzer on the boot exe and main programs A/B/C, generates C for the boot exe (our game.toml) and program A,
# compiles the generated C with the toolchain's clang, and watches the audited binaries for child processes, network
# endpoints and file writes outside their output folders (safety validation, RECOMP_NOTES.md '2026-10-26 psxrecomp safety audit').
# Inputs (read-only): C:\in\src (psxrecomp clone), C:\in\dl (toolchain zip), C:\in\stage (SCUS_944.21, seeds.txt, game.toml,
# A/B/C.exe + A/B/C.seeds.txt). Output: C:\out.
$ErrorActionPreference = 'Stop'
$ExeSha = 'b93e42462aceec120cfad1bfc3cf08f933196ab24a4e018b5f494c74e213abe7'
$ZipSha = '28da9742385e7ff875b3d9311e8ed89dbdc84f27b6ecba2bc0d0acc11f6d2b4d'
# wrapped main programs A/B/C = SO2.BIN entries 1/2129/2576 (PS-X EXE header added on the host by stage_overlays.py)
$ProgSha = @{ 'A' = 'dc73543e44300e3743cd0ffe49e9fce8469ff6ad553e72d0f82603747bcf3f3d'; 'B' = 'aeb73fd3fde48a4a5e8b770dc29c42f9031f48b736ccc6f54ea729eb728d36a6'; 'C' = '76d6a1a796b9f1b7734a7e47f75f91220f8e6764298904529839e0c9db80fe35' }
$W = 'C:\work'; $O = 'C:\out'
New-Item -ItemType Directory -Force $W, $O | Out-Null
function Log($m) { $l = '{0:HH:mm:ss} {1}' -f (Get-Date), $m; Add-Content -LiteralPath "$O\run.log" -Value $l; Write-Host $l }
function WLog($m) { Add-Content -LiteralPath "$O\watch.log" -Value $m }
function Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
function Run($name, [scriptblock]$b) { Log ">> $name"; $ErrorActionPreference = 'Continue'; $global:LASTEXITCODE = 0; & $b; if ($LASTEXITCODE -ne 0) { throw "$name exit $LASTEXITCODE" } }

# Runs one audited binary and records what it did besides its job: processes started while it ran (WMI start trace,
# children flagged), TCP/UDP endpoints it owned (polled every 100 ms), and files created/changed outside $allowed under
# the user profiles, ProgramData, Windows\Temp, the C:\ root and C:\work. Returns the exit code.
$WatchRoots = 'C:\Users', 'C:\ProgramData', 'C:\Windows\Temp', 'C:\work'
function Watch($name, $exe, [string[]]$argv, $wd, [string[]]$allowed) {
  Log ">> $name"
  Get-Event -SourceIdentifier pstart -ErrorAction SilentlyContinue | Remove-Event
  $t0 = Get-Date
  $p = Start-Process -FilePath $exe -ArgumentList $argv -WorkingDirectory $wd -NoNewWindow -PassThru `
         -RedirectStandardOutput "$O\$name.log" -RedirectStandardError "$O\$name.err.log"
  $null = $p.Handle   # keep the handle so ExitCode survives exit
  $net = New-Object Collections.Generic.HashSet[string]
  while (-not $p.HasExited) {
    Get-NetTCPConnection -OwningProcess $p.Id -ErrorAction SilentlyContinue | ForEach-Object { [void]$net.Add("tcp $($_.LocalAddress):$($_.LocalPort) -> $($_.RemoteAddress):$($_.RemotePort) $($_.State)") }
    Get-NetUDPEndpoint -OwningProcess $p.Id -ErrorAction SilentlyContinue | ForEach-Object { [void]$net.Add("udp $($_.LocalAddress):$($_.LocalPort)") }
    Start-Sleep -Milliseconds 100
  }
  $p.WaitForExit(); $rc = $p.ExitCode
  Start-Sleep -Milliseconds 500   # let late start-trace events arrive
  WLog "== $name  exit=$rc  pid=$($p.Id)  $('{0:HH:mm:ss}' -f $t0) .. $('{0:HH:mm:ss}' -f (Get-Date))"
  WLog "  network endpoints owned: $($net.Count)"; foreach ($n in $net) { WLog "    $n" }
  $ev = @(Get-Event -SourceIdentifier pstart -ErrorAction SilentlyContinue | ForEach-Object { $_.SourceEventArgs.NewEvent })
  $kids = @($ev | Where-Object { $_.ParentProcessID -eq $p.Id })
  WLog "  child processes: $($kids.Count)"; foreach ($k in $kids) { WLog "    $($k.ProcessName) pid=$($k.ProcessID)" }
  WLog "  all process starts in window: $($ev.Count)"; foreach ($e in $ev) { WLog "    $($e.ProcessName) pid=$($e.ProcessID) ppid=$($e.ParentProcessID)" }
  $files = foreach ($r in $WatchRoots) {
    Get-ChildItem -LiteralPath $r -Recurse -Force -File -ErrorAction SilentlyContinue |
      Where-Object { $_.LastWriteTime -ge $t0 -or $_.CreationTime -ge $t0 } |
      Where-Object { $f = $_.FullName; -not ($allowed | Where-Object { $f.StartsWith($_ + '\', [StringComparison]::OrdinalIgnoreCase) }) } |
      ForEach-Object FullName
  }
  $files += Get-ChildItem -LiteralPath 'C:\' -Force -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $t0 -or $_.CreationTime -ge $t0 } | ForEach-Object FullName
  $files = @($files | Where-Object { $_ })   # += of an empty result adds $null (run gen1 counted 1 file per step for it)
  WLog "  files written outside allowed dirs ($($allowed -join ', ')): $($files.Count)"; foreach ($f in $files) { WLog "    $f" }
  return $rc
}

Set-Content "$O\status.txt" 'RUNNING'
try {
  # 1. inputs: copy to sandbox-local storage first, then hash the local copies (no TOCTOU against the host mapping)
  Copy-Item C:\in\stage\SCUS_944.21 $W\SCUS_944.21
  if ((Sha $W\SCUS_944.21) -ne $ExeSha) { throw 'SCUS_944.21 hash mismatch' }
  Copy-Item C:\in\dl\cmake-clang-v1-windows-x64.zip $W\tc.zip
  if ((Sha $W\tc.zip) -ne $ZipSha) { throw 'toolchain zip hash mismatch' }
  foreach ($k in 'A', 'B', 'C') {
    Copy-Item C:\in\stage\$k.exe $W\$k.exe; Copy-Item C:\in\stage\$k.seeds.txt $W\$k.seeds.txt
    if ((Sha $W\$k.exe) -ne $ProgSha[$k]) { throw "program $k hash mismatch" }
  }
  Copy-Item C:\in\stage\seeds.txt $W\seeds.txt
  Log 'hashes ok'

  # 2. extract toolchain to a sandbox-local path; reject entries that escape it. The installer is never run.
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $root = [IO.Path]::GetFullPath("$W\tc") + '\'
  $z = [IO.Compression.ZipFile]::OpenRead("$W\tc.zip")
  try {
    foreach ($e in $z.Entries) {
      $dest = [IO.Path]::GetFullPath([IO.Path]::Combine($root, $e.FullName))
      if (-not $dest.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw "zip entry escapes target: $($e.FullName)" }
      if ($e.FullName.EndsWith('/')) { New-Item -ItemType Directory -Force $dest | Out-Null; continue }
      New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($dest)) | Out-Null
      [IO.Compression.ZipFileExtensions]::ExtractToFile($e, $dest, $true)
    }
  } finally { $z.Dispose() }
  Log 'toolchain extracted'

  # 3. session-only environment, so no toolchain helper goes to releases/latest. We call cmake directly anyway.
  $env:PSXRECOMP_TOOLCHAIN_DIR = "$W\tc"
  $env:Path = "$W\tc\bin;$env:Path"
  Run 'tool versions' { cmake --version | Select-Object -First 1 | ForEach-Object { Log $_ }; ninja --version | ForEach-Object { Log "ninja $_" }; clang++ --version | Select-Object -First 1 | ForEach-Object { Log $_ } }

  # 4. sandbox-local copy of the source (no .git)
  robocopy C:\in\src $W\src /E /XD .git /NFL /NDL /NJH /NJS /NP | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy exit $LASTEXITCODE" }

  # 5. build ONLY the analyzer and the generator. CHD off: that is the only FetchContent (network) dependency of
  #    recompiler/CMakeLists.txt; fmt/rabbitizer/toml11/ELFIO are vendored.
  Run 'configure' { cmake -S $W\src\recompiler -B $W\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSXRECOMP_ENABLE_CHD=OFF -DBUILD_TESTING=OFF -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ *> "$O\cmake_configure.log" }
  Run 'build psxrecomp-analyze psxrecomp-game' { cmake --build $W\build --target psxrecomp-analyze psxrecomp-game *> "$O\cmake_build.log" }
  $an = Get-ChildItem $W\build -Recurse -Filter psxrecomp-analyze.exe | Select-Object -First 1
  $gen = Get-ChildItem $W\build -Recurse -Filter psxrecomp-game.exe | Select-Object -First 1
  if (-not $an -or -not $gen) { throw 'psxrecomp-analyze.exe or psxrecomp-game.exe not found after build' }
  Log "built: analyze $(Sha $an.FullName)  game $(Sha $gen.FullName)"

  # process start trace for Watch (admin in the sandbox; if it is unavailable the log says so)
  try { Register-CimIndicationEvent -ClassName Win32_ProcessStartTrace -SourceIdentifier pstart | Out-Null; WLog 'process start trace: on' }
  catch { WLog "process start trace: UNAVAILABLE ($($_.Exception.Message))" }

  # 6. analyze: (a) default whole-image scan + our seeds, (b) --exact reachability-only for comparison
  # label, image, seed file. Outputs: $O\<label>\{seeded,exact}\ (functions.tsv, symbol_addrs.txt); logs analyze_<label>_<mode>.log
  $targets = @(, @('boot', "$W\SCUS_944.21", "$W\seeds.txt")) + @('A', 'B', 'C' | ForEach-Object { , @($_, "$W\$_.exe", "$W\$_.seeds.txt") })
  foreach ($t in $targets) {
    foreach ($mode in 'seeded', 'exact') {
      $n = "$($t[0])_$mode"; $d = "$W\an_$n"; New-Item -ItemType Directory -Force $d, "$O\$($t[0])\$mode" | Out-Null
      $a = @($t[1], '--out', $d, '--no-refs', '--emit-tsv', "$d\functions.tsv", '--emit-symbol-addrs', "$d\symbol_addrs.txt")
      if ($mode -eq 'seeded') { $a += @('--seeds', $t[2]) } else { $a += '--exact' }
      $rc = Watch "analyze_$n" $an.FullName $a $W @($d, $O)
      if ($rc -ne 0) { throw "analyze $n exit $rc" }
      Copy-Item "$d\functions.tsv", "$d\symbol_addrs.txt" "$O\$($t[0])\$mode"   # addresses/sizes/names only
    }
  }

  # 7. generate. boot: our game.toml (config mode) in a minimal project C:\work\proj (exe/seeds at the paths game.toml
  #    names, the framework's bios\SCPH1001.toml profile, a .gitignore so find_project_root stops there).
  #    A: positional mode (the synthetic PS-X EXE wrapper + A's seeds); no game.toml describes program A.
  $P = "$W\proj"; New-Item -ItemType Directory -Force "$P\disc", "$P\seeds", "$P\bios" | Out-Null
  Copy-Item C:\in\stage\game.toml "$P\game.toml"; Set-Content "$P\.gitignore" 'generated/'
  Copy-Item $W\SCUS_944.21 "$P\disc\SCUS_944.21"; Copy-Item $W\seeds.txt "$P\seeds\seeds.txt"   # 0x-prefixed: strtoul base 16 accepts it
  Copy-Item $W\src\bios\SCPH1001.toml "$P\bios\SCPH1001.toml"
  $genRuns = @(
    @('boot', "$P\generated", @('--config', "$P\game.toml", '--project-root', $P), $P),
    @('A',    "$W\gen_A",     @("$W\A.exe", '--seeds', "$W\A.seeds.txt", '--out-dir', "$W\gen_A", '--project-root', $P), $W))
  $gs = New-Object Collections.Generic.List[string]
  foreach ($g in $genRuns) {
    $rc = Watch "generate_$($g[0])" $gen.FullName $g[2] $g[3] @($g[1], $O)
    $files = @(Get-ChildItem $g[1] -File -ErrorAction SilentlyContinue)
    $gs.Add("== generate $($g[0]): exit $rc, $($files.Count) files, $([math]::Round((($files | Measure-Object Length -Sum).Sum) / 1MB, 1)) MB")
    foreach ($f in $files) { $gs.Add(('  {0,-40} {1,10} B {2,8} lines' -f $f.Name, $f.Length, @(Get-Content $f.FullName).Count)) }
    if ($rc -ne 0) { continue }
    # generated C stays game-derived: copied only to the gitignored output folder
    New-Item -ItemType Directory -Force "$O\gen\$($g[0])" | Out-Null; Copy-Item "$($g[1])\*" "$O\gen\$($g[0])"

    # 8. compile each generated .c with the toolchain clang (-O0, objects stay in the sandbox); errors go to compile_<label>.log
    $obj = "$W\obj_$($g[0])"; New-Item -ItemType Directory -Force $obj | Out-Null
    $ok = 0; $bad = 0
    foreach ($c in $files | Where-Object Extension -eq '.c') {
      $ErrorActionPreference = 'Continue'
      $msg = & clang -c -O0 -w -ferror-limit=50 -I "$W\src\runtime\include" -I "$W\src\recompiler\include" -I $g[1] $c.FullName -o "$obj\$($c.BaseName).o" 2>&1
      $ErrorActionPreference = 'Stop'
      if ($LASTEXITCODE -eq 0) { $ok++ } else { $bad++; Add-Content "$O\compile_$($g[0]).log" "== $($c.Name)"; Add-Content "$O\compile_$($g[0]).log" ($msg | Select-Object -First 60) }
    }
    $gs.Add("  compile (clang -c -O0): $ok ok, $bad failed")
    Log "generate $($g[0]): exit $rc; compile $ok ok, $bad failed"
  }
  Set-Content "$O\generate_summary.txt" $gs

  # 9. summary: counts, indirect-site classes, jump tables, unresolved sites (pc/reg/kind only; json "context" disassembly is not copied out)
  $s = New-Object Collections.Generic.List[string]
  foreach ($n in $targets | ForEach-Object { $l = $_[0]; "${l}_seeded"; "${l}_exact" }) {
    $a = Get-Content -Raw "$W\an_$n\analysis.json" | ConvertFrom-Json
    $i = Get-Content -Raw "$W\an_$n\indirect.json" | ConvertFrom-Json
    $s.Add("== $n")
    $s.Add(('entry {0:X8} load {1:X8} gp {2:X8} image_size {3}' -f [uint32]$a.entry_point, [uint32]$a.load_address, [uint32]$a.initial_gp, $a.image_size))
    foreach ($p in $a.stats.PSObject.Properties) { $s.Add("  $($p.Name) = $($p.Value)") }
    $s.Add('  indirect sites by classification:')
    $i.sites | Group-Object classification | Sort-Object Count -Descending | ForEach-Object { $s.Add("    $($_.Count)  $($_.Name)") }
    $s.Add('  jump-table sites (pc reg base count class):')
    $i.sites | Where-Object { $_.classification -like 'jump_table*' } | ForEach-Object { $s.Add(('    {0:X8} r{1} base {2:X8} n={3} {4}' -f [uint32]$_.pc, $_.reg, [uint32]$_.table_base, $_.table_count, $_.classification)) }
    $s.Add('  unresolved indirect sites (pc reg kind):')
    $i.sites | Where-Object { $_.classification -eq 'unresolved' } | ForEach-Object { $s.Add(('    {0:X8} r{1} {2}' -f [uint32]$_.pc, $_.reg, $_.kind)) }
  }
  Set-Content "$O\summary.txt" $s
  Set-Content "$O\status.txt" 'OK'
  Log 'done'
} catch {
  Log "FAILED: $($_.Exception.Message)"
  Log $_.ScriptStackTrace
  Set-Content "$O\status.txt" "FAILED: $($_.Exception.Message)"
}
