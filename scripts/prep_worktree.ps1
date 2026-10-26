# Copies the gitignored local inputs that match.ps1 needs into a git worktree (nothing is tracked, no game data in git):
#   build\main.bin (retail overlay, 280 KB) and tools\maspsx (assembler wrapper, 0.5 MB).
# Usage: scripts\prep_worktree.ps1 <worktree-dir> [-Main <main checkout>]   (main defaults to this repo's primary checkout)
param([Parameter(Mandatory)][string]$Worktree, [string]$Main)
$ErrorActionPreference = "Stop"
if (-not $Main) { $Main = Split-Path -Parent (git -C $PSScriptRoot rev-parse --path-format=absolute --git-common-dir) }
$wt = (Resolve-Path $Worktree).Path
foreach ($f in "build\main.bin") {
  if (-not (Test-Path "$Main\$f")) { throw "missing $Main\$f; run scripts\match.ps1 in the main checkout first" }
  New-Item -ItemType Directory -Force (Split-Path "$wt\$f") | Out-Null
  Copy-Item "$Main\$f" "$wt\$f" -Force
}
New-Item -ItemType Directory -Force "$wt\tools" | Out-Null
Copy-Item "$Main\tools\maspsx" "$wt\tools" -Recurse -Force
Write-Host "prepared $wt"
