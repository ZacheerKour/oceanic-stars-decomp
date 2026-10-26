> ## ⚠️ AI-assisted project
> This decompilation is being built with **Claude** (Anthropic's AI). Claude wrote the code, scripts and all of this
> README; the maintainer reviews and tests the results. Treat everything here as work in progress.

# Oceanic Stars Decomp - Star Ocean: The Second Story (USA, PS1)

A work-in-progress byte-matching decompilation of **Star Ocean: The Second Story** (tri-Ace / Enix, PlayStation, 1998),
with a native PC port planned once the decompilation is complete. It is not finished and nothing is playable yet.

**This repository contains no game data and no code generated from the game.** You need your own legally obtained copy
of both USA discs. The scripts here read them locally.

## Status
- Both discs and the boot executable analysed; the `SO2.BIN` archive format (encrypted table of contents, SLZ
  compression) is understood and can be extracted. See [ANALYSIS.md](ANALYSIS.md).
- Probable compiler: GCC 2.7.2 (Sony "psx" build), about 75% confident. No function has been byte-matched yet.
- The game code lives mostly in overlays loaded from `SO2.BIN`, not in the 128 KB boot executable.
- No existing Star Ocean 2 (PS1) decompilation was found when searching ([TOOLCHAIN.md](TOOLCHAIN.md)).

## Requirements
- Windows 10/11, Docker Desktop, Python 3.10+, git, [7-Zip](https://www.7-zip.org/).
- Your discs: **Star Ocean - The Second Story (USA)** Disc 1 and Disc 2 (`.7z` containing `.bin`/`.cue`).

## Install
```
git clone https://github.com/ZacheerKour/oceanic-stars-decomp
cd oceanic-stars-decomp
INSTALL.bat "C:\path\to\folder\with\the\two\.7z\files"
```
The installer checks prerequisites, extracts the discs into `roms\`, fetches objdiff, maspsx and decomp-permuter into
`tools\`, and builds the `so2-decomp` Docker image (old GCC PSX builds + binutils + splat). Ghidra 12.1.4 with
[ghidra_psx_ldr](https://github.com/lab313ru/ghidra_psx_ldr) is optional and listed in [TOOLCHAIN.md](TOOLCHAIN.md).
**The installer has not been tested on a clean machine yet.**

## Layout
| Path | What |
|---|---|
| `analysis/` | Python scripts: ISO reader, `SO2.BIN` table decoder, SLZ decompressor, extraction, code scans |
| `docs/` | Format specs: `SLZ.md`, `SO2BIN_TOC.md`, `overlay_schema.md` |
| `tests/slz/` | Synthetic SLZ test vectors + round-trip runner (`python -I tests/slz/run_tests.py`) |
| `config/` | `overlays.tsv`/`overlays.json` (overlay map), `seeds*.txt`, `symbols.toml`, `matches.tsv` |
| `recomp/` | psxrecomp scaffold (`game.toml`, AOT profile, first-run steps); not yet run |
| `docker/` | `Dockerfile` for the compiler image and `run.ps1` wrapper |
| `ANALYSIS.md` | Disc, archive, overlay and compiler findings |
| `TOOLCHAIN.md` | Tools, versions, sources and related projects |
| `AGENTS.md` | Project guide for the AI agents working on the decompilation |

## Legal
Unofficial fan project, not affiliated with or endorsed by Square Enix or tri-Ace. No game assets or code are
distributed; use only with discs you own. Original material here is MIT licensed ([LICENSE](LICENSE)).
