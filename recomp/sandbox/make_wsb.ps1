<#
Generates the (gitignored) Windows Sandbox config recomp\sandbox\_run.wsb with this checkout's real paths filled in,
plus the staging and output folders. It does NOT start the sandbox: review _run.wsb and _stage\, then double-click _run.wsb yourself.
Usage: powershell -NoProfile -File recomp\sandbox\make_wsb.ps1
#>
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$ExeSha = 'b93e42462aceec120cfad1bfc3cf08f933196ab24a4e018b5f494c74e213abe7'   # SCUS_944.21 (same on both discs)
$ZipSha = '28da9742385e7ff875b3d9311e8ed89dbdc84f27b6ecba2bc0d0acc11f6d2b4d'   # cmake-clang-v1-windows-x64.zip

$src   = Join-Path $repo 'tools\ref\psxrecomp'
$dl    = Join-Path $repo 'tools\recomp\dl'
$zip   = Join-Path $dl 'cmake-clang-v1-windows-x64.zip'
$exe   = Join-Path $repo 'extracted\SCUS_944.21'
$seeds = Join-Path $repo 'config\seeds.txt'
$stage = Join-Path $here '_stage'
$out   = Join-Path $here '_out'
$wsb   = Join-Path $here '_run.wsb'

function Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
foreach ($p in $src, $zip, $exe, $seeds) { if (-not (Test-Path -LiteralPath $p)) { throw "missing: $p" } }
if ((Sha $zip) -ne $ZipSha) { throw 'toolchain zip SHA-256 mismatch; refusing to continue' }
if ((Sha $exe) -ne $ExeSha) { throw 'SCUS_944.21 SHA-256 mismatch; refusing to continue' }
if (Get-Command git -ErrorAction SilentlyContinue) { Write-Host ('psxrecomp clone HEAD: ' + (& git -C $src rev-parse HEAD)) }

# fresh staging: the boot exe, the (address-only) seed list and the sandbox script
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage, $out | Out-Null
Copy-Item -LiteralPath $exe, $seeds -Destination $stage
Copy-Item -LiteralPath (Join-Path $here 'run_in_sandbox.ps1') -Destination $stage

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
Write-Host 'Review _run.wsb and _stage\, then launch _run.wsb manually. Results appear in _out\ (status.txt, summary.txt, logs).'
