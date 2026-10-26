# Runs INSIDE Windows Sandbox (LogonCommand). Offline. Builds only psxrecomp-analyze and runs it on the boot exe and on main programs A/B/C.
# Inputs (read-only): C:\in\src (psxrecomp clone), C:\in\dl (toolchain zip), C:\in\stage (SCUS_944.21, seeds.txt, A/B/C.exe + A/B/C.seeds.txt). Output: C:\out.
$ErrorActionPreference = 'Stop'
$ExeSha = 'b93e42462aceec120cfad1bfc3cf08f933196ab24a4e018b5f494c74e213abe7'
$ZipSha = '28da9742385e7ff875b3d9311e8ed89dbdc84f27b6ecba2bc0d0acc11f6d2b4d'
# wrapped main programs A/B/C = SO2.BIN entries 1/2129/2576 (PS-X EXE header added on the host by stage_overlays.py)
$ProgSha = @{ 'A' = 'dc73543e44300e3743cd0ffe49e9fce8469ff6ad553e72d0f82603747bcf3f3d'; 'B' = 'aeb73fd3fde48a4a5e8b770dc29c42f9031f48b736ccc6f54ea729eb728d36a6'; 'C' = '76d6a1a796b9f1b7734a7e47f75f91220f8e6764298904529839e0c9db80fe35' }
$W = 'C:\work'; $O = 'C:\out'
New-Item -ItemType Directory -Force $W, $O | Out-Null
function Log($m) { $l = '{0:HH:mm:ss} {1}' -f (Get-Date), $m; Add-Content -LiteralPath "$O\run.log" -Value $l; Write-Host $l }
function Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
function Run($name, [scriptblock]$b) { Log ">> $name"; $ErrorActionPreference = 'Continue'; $global:LASTEXITCODE = 0; & $b; if ($LASTEXITCODE -ne 0) { throw "$name exit $LASTEXITCODE" } }
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

  # 2. extract toolchain to a sandbox-local path; reject entries that escape it. install.ps1 is never run.
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

  # 5. build ONLY the analyzer. CHD off: that is the only FetchContent (network) dependency; fmt/rabbitizer/toml11/ELFIO are vendored.
  Run 'configure' { cmake -S $W\src\recompiler -B $W\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSXRECOMP_ENABLE_CHD=OFF -DBUILD_TESTING=OFF -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ *> "$O\cmake_configure.log" }
  Run 'build psxrecomp-analyze' { cmake --build $W\build --target psxrecomp-analyze *> "$O\cmake_build.log" }
  $an = Get-ChildItem $W\build -Recurse -Filter psxrecomp-analyze.exe | Select-Object -First 1
  if (-not $an) { throw 'psxrecomp-analyze.exe not found after build' }

  # 6. analyze: (a) default whole-image scan + our seeds, (b) --exact reachability-only for comparison
  # label, image, seed file. Outputs: $O\<label>\{seeded,exact}\ (functions.tsv, symbol_addrs.txt); logs analyze_<label>_<mode>.log
  $targets = @(, @('boot', "$W\SCUS_944.21", "$W\seeds.txt")) + @('A', 'B', 'C' | ForEach-Object { , @($_, "$W\$_.exe", "$W\$_.seeds.txt") })
  foreach ($t in $targets) {
    foreach ($mode in 'seeded', 'exact') {
      $n = "$($t[0])_$mode"; $d = "$W\an_$n"; New-Item -ItemType Directory -Force $d, "$O\$($t[0])\$mode" | Out-Null
      $a = @($t[1], '--out', $d, '--no-refs', '--emit-tsv', "$d\functions.tsv", '--emit-symbol-addrs', "$d\symbol_addrs.txt")
      if ($mode -eq 'seeded') { $a += @('--seeds', $t[2]) } else { $a += '--exact' }
      Run "analyze $n" { & $an.FullName @a *> "$O\analyze_$n.log" }
      Copy-Item "$d\functions.tsv", "$d\symbol_addrs.txt" "$O\$($t[0])\$mode"   # addresses/sizes/names only
    }
  }

  # 7. summary: counts, indirect-site classes, jump tables, unresolved sites (pc/reg/kind only; json "context" disassembly is not copied out)
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
