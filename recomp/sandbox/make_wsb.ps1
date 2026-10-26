<#
Generates the (gitignored) Windows Sandbox config recomp\sandbox\_run.wsb with this checkout's real paths filled in,
plus the staging and output folders. It does NOT start the sandbox: review _run.wsb and _stage\, then double-click _run.wsb yourself.
Usage: powershell -NoProfile -File recomp\sandbox\make_wsb.ps1 [-OutName _out_run3]
From a git worktree set $env:SO2_DATA=<main checkout> first: the gitignored inputs (tools\, extracted\, analysis\out\) are read from there, config\ from this checkout.
#>
param([string]$OutName = '_out_run2')
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$data = if ($env:SO2_DATA) { (Resolve-Path $env:SO2_DATA).Path } else { $repo }
$ExeSha = 'b93e42462aceec120cfad1bfc3cf08f933196ab24a4e018b5f494c74e213abe7'   # SCUS_944.21 (same on both discs)
$ZipSha = '28da9742385e7ff875b3d9311e8ed89dbdc84f27b6ecba2bc0d0acc11f6d2b4d'   # cmake-clang-v1-windows-x64.zip

# decompressed main programs (SO2.BIN entries 1/2129/2576), pinned raw sha256; md5 is also checked against config\overlays.tsv by stage_overlays.py
$Raw = @{ 'A' = @('0001_312.dec',  '2c1345cfc74b55b3646ea2d8caabd0382cdb4c94c65b440e66d7d3e1008002fb')
          'B' = @('2129_24323.dec', '7ae7d12caf8146249bbaeb904f34a7b982fa928f72e3af624c438ef790ece722')
          'C' = @('2576_30736.dec', '6dec3950348b57068a85759f472ea7d5c92241d28b00ff4ea139ddd8dc1587cf') }

$src   = Join-Path $data 'tools\ref\psxrecomp'
$dl    = Join-Path $data 'tools\recomp\dl'
$zip   = Join-Path $dl 'cmake-clang-v1-windows-x64.zip'
$exe   = Join-Path $data 'extracted\SCUS_944.21'
$seeds = Join-Path $repo 'config\seeds.txt'
$stage = Join-Path $here '_stage'
$out   = Join-Path $here $OutName   # run 1 (boot exe only) stays untouched in _out
$wsb   = Join-Path $here '_run.wsb'

function Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
foreach ($p in $src, $zip, $exe, $seeds) { if (-not (Test-Path -LiteralPath $p)) { throw "missing: $p" } }
if ((Sha $zip) -ne $ZipSha) { throw 'toolchain zip SHA-256 mismatch; refusing to continue' }
if ((Sha $exe) -ne $ExeSha) { throw 'SCUS_944.21 SHA-256 mismatch; refusing to continue' }
if (Get-Command git -ErrorAction SilentlyContinue) { Write-Host ('psxrecomp clone HEAD: ' + (& git -C $src rev-parse HEAD)) }

foreach ($k in $Raw.Keys) {
  $f = Join-Path (Join-Path $data 'analysis\out\d1') $Raw[$k][0]
  if (-not (Test-Path -LiteralPath $f)) { throw "missing: $f (see analysis\slz.py)" }
  if ((Sha $f) -ne $Raw[$k][1]) { throw "program $k SHA-256 mismatch; refusing to continue" }
}
# fresh staging: the boot exe, the address-only seed lists, the wrapped main programs and the sandbox script
New-Item -ItemType Directory -Force $stage | Out-Null
Get-ChildItem -LiteralPath $stage -Force | Remove-Item -Recurse -Force   # contents only: the folder may still be mapped by an open sandbox
New-Item -ItemType Directory -Force $stage, $out | Out-Null
Copy-Item -LiteralPath $exe -Destination $stage
# psxrecomp-analyze --seeds parses with stoul base 0: bare hex is rejected or read as decimal, so prefix 0x (config\seeds.txt stays bare hex for main_psx)
Get-Content -LiteralPath $seeds | Where-Object { $_.Trim() -and -not $_.StartsWith('#') } | ForEach-Object { '0x' + $_.Trim() } | Set-Content -LiteralPath (Join-Path $stage 'seeds.txt') -Encoding ascii
Copy-Item -LiteralPath (Join-Path $here 'run_in_sandbox.ps1') -Destination $stage
# PS-X EXE wrappers A.exe/B.exe/C.exe + A/B/C.seeds.txt (python -I: no site/cwd imports); prints sha256 of each wrapped exe
& python -I (Join-Path $here 'stage_overlays.py') $repo $stage
if ($LASTEXITCODE -ne 0) { throw 'stage_overlays.py failed' }

function Folder($hostPath, $guest, $ro) {
  $h = [Security.SecurityElement]::Escape($hostPath)
  "    <MappedFolder><HostFolder>$h</HostFolder><SandboxFolder>$guest</SandboxFolder><ReadOnly>$ro</ReadOnly></MappedFolder>"
}
$xml = @(
 '<Configuration>',
 '  <VGpu>Disable</VGpu>',
 '  <Networking>Disable</Networking>',
 '  <AudioInput>Disable</AudioInput>',
 '  <VideoInput>Disable</VideoInput>',
 '  <PrinterRedirection>Disable</PrinterRedirection>',
 '  <ClipboardRedirection>Disable</ClipboardRedirection>',
 '  <ProtectedClient>Enable</ProtectedClient>',
 '  <MemoryInMB>8192</MemoryInMB>',
 '  <MappedFolders>',
 (Folder $src   'C:\in\src'   'true'),
 (Folder $dl    'C:\in\dl'    'true'),
 (Folder $stage 'C:\in\stage' 'true'),
 (Folder $out   'C:\out'      'false'),
 '  </MappedFolders>',
 '  <LogonCommand><Command>powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\in\stage\run_in_sandbox.ps1</Command></LogonCommand>',
 '</Configuration>') -join "`r`n"
[IO.File]::WriteAllText($wsb, $xml, (New-Object Text.UTF8Encoding $false))

Write-Host "wrote $wsb"
Write-Host ('staged: ' + ((Get-ChildItem $stage | ForEach-Object Name) -join ', '))
Write-Host ('run_in_sandbox.ps1 sha256: ' + (Sha (Join-Path $stage 'run_in_sandbox.ps1')))
Write-Host "Review _run.wsb and _stage\, then launch _run.wsb manually. Results appear in $OutName\ (status.txt, summary.txt, logs)."
