> ## ⚠️ AI-assisted project
> This decompilation and recompilation project is being built with **Claude** (Anthropic's AI). Claude wrote the code, scripts and all of this
> README; the maintainer reviews and tests the results. Treat everything here as work in progress.

# Oceanic Stars - Star Ocean: The Second Story (USA, PS1) as a native PC program

An unofficial fan project that turns **Star Ocean: The Second Story** (tri-Ace / Enix, PlayStation, 1998) into a native
Windows program by **static recompilation**: the game's MIPS code is translated to C with
[psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp) and compiled on your own PC. A **decompilation** (byte-matching
C source) runs alongside and feeds the recompiler (see the end of this file).

Nothing of the game is in this repository. You bring your own discs; the installer builds everything locally, and the
generated code never leaves your machine.

## Status (2026-10-27)
**Works** (tested on one Windows 11 PC with an RTX 4070 SUPER):
- Builds with one command from a clean clone plus the two discs (`INSTALL.bat`).
- Boots with the bundled OpenBIOS or your own SCPH-5501 BIOS, through the intro and title menu, *New Game*, the
  settings screens and character name entry, into the first field scene (the ruins on Milokeenia), windowed at 60 fps
  with sound.
  Tested from a fresh clone: `INSTALL.bat` with the two `.7z` files, then `PLAY.bat` (OpenBIOS) reached the first field.
- All game code (boot executable, the three main programs and all 89 compressed code modules) runs as compiled C; the
  remaining interpreted code after boot is a few hundred MIPS instructions per second.

**Not verified yet:** battles, saving to and loading from the memory card in game, anything past the first cave, disc 2.
Only scripted input has been used so far, so expect bugs once people actually play. Reports are welcome.

## Requirements
- Windows 10 or 11, 64-bit; a GPU with OpenGL 3.3; about 15 GB of free disk space (Build Tools ~8 GB, the build ~3 GB,
  your discs).
- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) with the
  *Desktop development with C++* workload (free):
  `winget install Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"`
- Python 3.11 or newer (`winget install Python.Python.3.11`) and Git (`winget install Git.Git`).
- [7-Zip](https://www.7-zip.org/) if your discs are `.7z` files (`winget install 7zip.7zip`).

The installer only checks for these and prints the command for anything missing; it never installs system software.

## You need your own copy of the game
Both discs of **Star Ocean - The Second Story (USA)** (SCUS-94421 / SCUS-94422) as `.bin`/`.cue` images, or the `.7z`
files that contain them. The installer recognises them by SHA-1 (the values are in `recomp\game.toml`), whatever the file
names. This repository contains no game data, no BIOS and no code generated from the game.

BIOS: optional. By default the game runs on OpenBIOS, the open-source BIOS that psxrecomp ships. If you put your own
**SCPH-5501** dump at `roms\scph5501.bin` before installing, it is recompiled locally and used instead (it is checked
against a fixed SHA-256).

## Install
1. `git clone <this repository's URL> oceanic-stars` (or download the ZIP and unpack it).
2. Put your two disc files anywhere and run `INSTALL.bat "<folder with the discs>"`, or drag the disc files onto
   `INSTALL.bat`. The first install downloads psxrecomp (pinned commit `f42d2c0`) and the SDL3/zlib sources (SHA-256
   checked), then builds; on a 16-core PC the whole install took about 4 minutes.
3. Double-click `PLAY.exe` (built by the installer; `PLAY.bat` does the same through PowerShell and is the fallback).

Re-running `INSTALL.bat` is safe: finished steps are skipped. After `git pull`, run it again to rebuild.

## Controls
Keyboard and gamepad both work from the first start (they can be used at the same time). Keyboard keys are rebindable in
`settings.ini` (`key.<button> = <key>[, <second key>]` lines, SDL key names such as `Space`, `Left Shift`, `Tab`, `Return`;
`None` unbinds; applied on every start, see `settings.default.ini`). The table is Star Ocean 2 oriented: Cross confirms,
Circle cancels, Triangle opens the menu, L1/R1 switch characters, L2/R2 are used in battle.

| PlayStation | Keyboard (main / second key) | Gamepad (by position) |
|---|---|---|
| D-pad | W A S D / arrow keys | d-pad and left stick |
| Cross (confirm) | Space / 1 | bottom face button (Xbox A, PlayStation Cross, Switch B) |
| Circle (cancel) | Q / 2 | right face button |
| Square | E / 3 | left face button |
| Triangle (menu) | R / 4 | top face button |
| L1 / R1 | Left Shift / F (second: Z / X) | left / right bumper |
| L2 / R2 | Tab / G (second: C / V) | left / right trigger |
| Start | Esc / Enter | Start (Menu) |
| Select | Backspace / Right Shift | Back (View, Share) |
| L3 / R3 | T / Y | stick clicks |

Gamepads (Xbox, DualShock/DualSense, Switch Pro, 8BitDo, generic XInput/DirectInput pads known to SDL3) are picked up
automatically, also when plugged in while the game runs; the first pad is player 1. The left stick acts as a digital d-pad
(`pad.deadzone` in `settings.ini`, default 9000 of 32767); rumble is not used. SDL3 maps face buttons by position, so a
Nintendo-style pad gets Cross on its bottom button too. A pad SDL does not know: add its line to a `gamecontrollerdb.txt`
(community list: <https://github.com/mdqinc/SDL_GameControllerDB>) next to `PLAY.bat`/`PLAY.exe`, or set the
`SDL_GAMECONTROLLERCONFIG` environment variable.

### Steam Controller / Steam Deck
**Untested** (no Steam installation was available for testing). What the code says: the game cannot tell a Steam Controller
from other pads; it only sees SDL3 gamepads. What to do:
1. Steam > *Add a Non-Steam Game* > *Browse* > pick **`PLAY.exe`** in this folder (Steam lists only `.exe` files; `PLAY.bat` does
   not show up). Set *Start In* to this folder. No launch options are needed. `PLAY.exe` starts the game as its own child
   process and waits for it, so Steam tracks the game and Steam Input and the overlay attach to it.
2. Controller settings for the game: the **Gamepad** template (recommended), or the **Keyboard (WASD) and Mouse** template, which
   matches the WASD map above. With the Gamepad template Steam presents a virtual Xbox-style pad, which is the normal case
   above.
3. With Steam Input off, SDL3 can talk to Steam hardware itself: the SDL3 version built here (3.4.10) contains HIDAPI drivers
   for the Steam Controller (original), the Steam Deck controller, the HORI Steam controller and a newer Steam Controller
   (checked in the source), enabled by default, and this runtime turns SDL's HIDAPI support on. Whether they work on your
   hardware is untested.

## Saves and settings
- Memory cards (`card1.mcd`, `card2.mcd`) and savestates live in `saves\` next to `PLAY.bat`. Back that folder up.
- `settings.ini` (created on the first start from `settings.default.ini`): `bios = auto | openbios | <path>`, `key.*` keyboard binds, `pad.deadzone`,
  `extra_args = ...` for the runtime, and `env.NAME = value` lines for runtime environment options.
- The run directory `play\` holds the generated `game.toml` and the logs of the last start (`last.log`, `last.err`).

## Known issues
- Only the opening has been played (by script). Battles, in-game saving and disc 2 are untested.
- Audio peaks sometimes hit full scale (possible clipping).
- The first cave needs real play: a scripted run stops at the sealed door ("Might there be some way to open the door").
- Building needs the full Visual Studio C++ toolchain; there is no prebuilt download, by design (see Legal).

## Legal
Unofficial, non-commercial fan project, not affiliated with or endorsed by Square Enix or tri-Ace. No game assets,
BIOS or generated code are distributed: the build runs on your PC from discs you own, and its output stays there. Do not
share the built program or anything under `roms\`, `extracted\`, `analysis\out\`, `recomp\**\_*` or `tools\`.
psxrecomp is licensed under PolyForm Noncommercial; it is fetched by the installer at the pinned commit `f42d2c0` and is
never redistributed here. SDL3 (zlib licence) and zlib are fetched from their official releases. Original material in
this repository is MIT licensed ([LICENSE](LICENSE)).

## Credits and links
- [psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp) (RetroPortingToolKit): the recompiler and runtime.
- [SDL](https://libsdl.org), [zlib](https://zlib.net), OpenBIOS (bundled with psxrecomp).
- Install and play flow modelled on the Phantom Dust recompilation (desert-breeze-dust).
- tri-Ace and Enix for the game.

## For developers
- Input checks: `env.PSXRT_INPUT_LOG = 1` in `settings.ini` logs the PS1 pad word per key/pad press to `play\last.err`; `env.PSXRT_VPAD_TEST = 1` attaches a virtual gamepad and prints PASS/FAIL per button.
- `PLAY.exe` source: `recomp/launcher/play.c` (it runs `play.ps1 -Prepare` hidden, then starts the game exe directly).
- Recomp build details: [recomp/README.md](recomp/README.md); research log: [RECOMP_NOTES.md](RECOMP_NOTES.md).
- Decomp tooling (Docker image with the PSX GCC builds, objdiff, maspsx, splat): `INSTALL_DEV.bat "<disc folder>"`, see
  [TOOLCHAIN.md](TOOLCHAIN.md) and [AGENTS.md](AGENTS.md).

**Decomp status**
- The `SO2.BIN` archive (encrypted table of contents, SLZ compression) is understood, documented ([docs/](docs/)) and
  extractable ([ANALYSIS.md](ANALYSIS.md)). Every code overlay has a load address ([config/overlays.tsv](config/overlays.tsv)).
- 460 functions of the main program byte-match with GCC 2.7.2-psx `-O2 -G0` (`scripts\match.ps1`), 457 of them passing
  the project's rule checker (`scripts\rules.py`).

| Path | What |
|---|---|
| `recomp/` | Recomp inputs and build scripts (`rtfull\build_all.ps1`, `game.toml`, AOT profile, BIOS backend) |
| `analysis/` | Python: ISO reader, `SO2.BIN` table decoder, SLZ decompressor, extraction, code scans, seeds |
| `config/` | Overlay map, seeds, symbols, matched functions |
| `src/`, `scripts/`, `docker/` | Decomp source, match/rules scripts, compiler image |
| `docs/`, `tests/slz/` | Format specs, SLZ test vectors |
