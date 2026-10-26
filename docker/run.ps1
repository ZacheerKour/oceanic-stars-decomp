# Usage: .\docker\run.ps1 <command...>   (no args = interactive bash). Mounts project root at /work.
$root = Split-Path -Parent $PSScriptRoot
if ($args.Count -eq 0) { docker run --rm -it -v "${root}:/work" so2-decomp bash }
else { docker run --rm -v "${root}:/work" so2-decomp @args }
