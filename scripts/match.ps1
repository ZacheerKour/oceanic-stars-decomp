param([string]$Gcc="gcc-2.7.2-psx")
# Byte-compares src\*.c (listed in config\matches.tsv) against the retail overlay in Docker.
# Needs analysis\out\d1\0001_312.dec (see analysis\extract.py); copied to the gitignored build\main.bin.
$root = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force "$root\build" | Out-Null
if (-not (Test-Path "$root\build\main.bin")) { Copy-Item "$root\analysis\out\d1\0001_312.dec" "$root\build\main.bin" }
docker run --rm -v "${root}:/work" -w /work so2-decomp bash scripts/match.sh $Gcc
