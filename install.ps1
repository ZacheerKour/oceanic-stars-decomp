param([Parameter(Mandatory=$true)][string]$DiscDir)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot; Set-Location $root
function Step($n){ Write-Host "== $n" }
try {
  Step 'prerequisites'
  foreach($c in 'python','git','docker'){ if(-not (Get-Command $c -ErrorAction SilentlyContinue)){ throw "missing: $c" } }
  $z="C:\Program Files\7-Zip\7z.exe"; if(-not (Test-Path $z)){ throw 'missing: 7-Zip' }
  Step 'extract discs'
  foreach($d in 1,2){
    $a=Join-Path $DiscDir "Star Ocean - The Second Story (USA) (Disc $d).7z"
    if(-not (Test-Path $a)){ throw "not found: $a" }
    $o="roms\disc$d"; if(-not (Test-Path "$o\*.bin")){ New-Item -ItemType Directory -Force $o | Out-Null; & $z x -y "-o$o" $a | Out-Null }
  }
  Step 'tools'
  New-Item -ItemType Directory -Force tools\objdiff | Out-Null
  if(-not (Test-Path tools\maspsx)){ git clone --depth 1 https://github.com/mkst/maspsx tools\maspsx }
  if(-not (Test-Path tools\decomp-permuter)){ git clone --depth 1 https://github.com/simonlindholm/decomp-permuter tools\decomp-permuter }
  if(-not (Test-Path tools\objdiff\objdiff-cli.exe)){ Invoke-WebRequest https://github.com/encounter/objdiff/releases/download/v3.8.3/objdiff-cli-windows-x86_64.exe -OutFile tools\objdiff\objdiff-cli.exe }
  python -m pip install --user splat64
  Step 'docker image'
  docker build -t so2-decomp -f docker\Dockerfile .
  Write-Host 'INSTALL OK'
} catch { Write-Host "INSTALL FAILED: $_"; exit 1 }
