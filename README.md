> ## ⚠️ AI-assisted project
> This decompilation and recompilation project is being built with **Claude** (Anthropic's AI). Claude wrote the code, scripts and all of this
> README; the maintainer reviews and tests the results. Treat everything here as work in progress.

# Oceanic Stars - Star Ocean: The Second Story (USA, PS1) decomp + recomp

A work-in-progress **decompilation and static-recompilation** project for **Star Ocean: The Second Story** (tri-Ace /
Enix, PlayStation, 1998). Two tracks run side by side:

- **Decomp:** byte-matching C source for the game, compiled with the original compiler (GCC 2.7.2) and checked against
  the retail bytes.
- **Recomp:** a native PC port built by static recompilation with [psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp).
  The decomp feeds it function boundaries, names and the overlay map, and its matched C serves as a regression check
  on the recompiled output ([RECOMP_NOTES.md](RECOMP_NOTES.md)).

It is not finished and nothing is playable yet.

**This repository contains no game data and no code generated from the game.** You need your own legally obtained copy
of both USA discs. The scripts here read them locally.

## Status
**Decomp**
- Both discs and the boot executable analysed; the `SO2.BIN` archive (encrypted table of contents, SLZ compression) is
  understood, documented ([docs/](docs/)) and extractable. See [ANALYSIS.md](ANALYSIS.md).
- 264 functions of the main overlay (program A) byte-match with GCC 2.7.2-psx `-O2 -G0` (`scripts\match.ps1`), all passing
  the project's rule checker (`scriptsules.py`, 0 failures, 264 of 264 files); that compiler is about 95% confident. Four loop functions
  are parked as walls. Only leaf and simple frame/call/loop functions so far; switch tables, `.rodata` and larger frames are untested.
- Every code overlay has a load address ([config/overlays.tsv](config/overlays.tsv)), found by static analysis only.
- The game code lives mostly in overlays loaded from `SO2.BIN`, not in the 128 KB boot executable.
- No existing Star Ocean 2 (PS1) decompilation was found when searching ([TOOLCHAIN.md](TOOLCHAIN.md)).

**Recomp**
- Inputs for psxrecomp exist: overlay profile, seeds, symbols and a `game.toml` scaffold in [recomp/](recomp/).
- Not yet run: psxrecomp's analyzer has not been tried on the game, and psxrecomp has no SLZ extraction method, so that
  has to be added first. Several values in `game.toml` are marked `TODO(verify)`.

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
| `recomp/` | Recomp track: psxrecomp scaffold (`game.toml`, AOT profile, first-run steps); not yet run |
| `docker/` | `Dockerfile` for the compiler image and `run.ps1` wrapper |
| `ANALYSIS.md` | Disc, archive, overlay and compiler findings |
| `TOOLCHAIN.md` | Tools, versions, sources and related projects |
| `RECOMP_NOTES.md` | Research on psxrecomp and ps1-recomp, gaps and checklist |
| `AGENTS.md` | Project guide for the AI agents working on the project |

## Legal
Unofficial fan project, not affiliated with or endorsed by Square Enix or tri-Ace. No game assets or code are
distributed; use only with discs you own. Original material here is MIT licensed ([LICENSE](LICENSE)).
