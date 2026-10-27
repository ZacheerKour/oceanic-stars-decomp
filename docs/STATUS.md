# Status (2026-10-27)

Tested on one machine: Windows 11, RTX 4070 SUPER, built from a fresh clone with the two US discs. A passing build is
not proof that the game plays; only the rows marked with evidence are known to work.

## Works (with evidence)
| Area | Evidence |
|---|---|
| Install from a clean clone + two `.7z` discs | `INSTALL.bat` then `PLAY.bat` (OpenBIOS) reached the first field scene; `install.log` and `play\last.log` of that run |
| Boot, intro, title menu, New Game, settings, name entry, first field (Milokeenia ruins) | scripted-input run, windowed 60 fps with sound |
| All game code as compiled C (boot exe, three main programs, 89 SLZ modules) | AOT run counts in `docs/notes/2026-10-26-aot-static-for-all-89-slz-chunks-our-seeds-msvc-compile.md`; interpreted code after boot is a few hundred instructions per second |
| Title menu artwork, full-width FMV, display options (`settings.ini` window_width, render_scale, scale_mode, filter) | window captures of a scripted run before/after, `docs/notes/2026-10-27-video.md` |
| SLZ decoder | `python -I tests/slz/run_tests.py` (480 round trips, 18 vectors) |
| Decomp | 460 main-program functions byte-match; 457 pass `scripts/rules.py` |

## Untested (expect bugs)
- Battles.
- Saving to and loading from the memory card in game.
- The disc 2 prompt and everything on disc 2.
- Steam / Steam Deck (no Steam installation available).
- Anything past the first cave; only scripted input has been used.
- Resizing, maximising and fullscreen by hand (only a scripted resize of a hidden window was checked).
- Gamepads and rumble beyond what SDL3 provides; no real-hardware pad matrix.

## Known limits
- A mid-session disc swap is manual: press F10 to change disc.
- Audio peaks sometimes reach full scale (possible clipping).
- Building needs the Visual Studio C++ toolchain; there is no prebuilt download, by design (see NOTICE).
- Other regions are not supported; only the USA discs listed in `recomp/game.toml`.

Update this file, with the date, whenever one of these changes.
