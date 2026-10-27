# Status (2026-10-27)

Tested on one machine: Windows 11, RTX 4070 SUPER, built from a fresh clone with the two US discs. A passing build is
not proof that the game plays; only the rows marked with evidence are known to work.

## Works (with evidence)
| Area | Evidence |
|---|---|
| Install from a clean clone + two `.7z` discs | `INSTALL.bat` then `PLAY.bat` (OpenBIOS) reached the first field scene; `install.log` and `play\last.log` of that run |
| Boot, intro, title menu, New Game, settings, name entry, first field (Milokeenia ruins) | scripted-input run, windowed 60 fps with sound |
| All game code as compiled C (boot exe, three main programs, 89 SLZ modules) | AOT run counts in `docs/notes/2026-10-26-aot-static-for-all-89-slz-chunks-our-seeds-msvc-compile.md`; interpreted code after boot is a few hundred instructions per second |
| SLZ decoder | `python -I tests/slz/run_tests.py` (480 round trips, 18 vectors) |
| Quick save/load, slots, pad chords, pause menu (disc swap row) | `recomp/rtfull/input/qs_*.txt` scripted runs (`qrun.ps1`): title screenshot hash identical after F5 then F9 (also via menu, via pad chord, and in a second process after restart); menu screenshots; key/chord/menu logic by `cl /DPSXRT_IO_SELFTEST psxrt_io.c` (selftest OK) |
| Decomp | 460 main-program functions byte-match; 457 pass `scripts/rules.py` |

## Untested (expect bugs)
- Battles.
- Saving to and loading from the memory card in game.
- The disc 2 prompt and everything on disc 2.
- Steam / Steam Deck (no Steam installation available).
- Anything past the first cave; only scripted input has been used.
- The real keyboard and a real gamepad for the new hotkeys/pause menu (events were injected with SDL_PushEvent and script pad words; pad Back+Start menu open, F10 menu open, fullscreen toggle from the menu, Quit and the disc swap row were not exercised end to end).
- Save states taken in the intro movie / mid-field with a real window; only the static title was hash-compared.
- Gamepads and rumble beyond what SDL3 provides; no real-hardware pad matrix.

## Known limits
- A mid-session disc swap is manual: F10 (or Select+Start) opens the pause menu, row Swap disc. Before 2026-10-27 F10 did nothing in this build (the runtime's menu is launcher-only); the pause menu replaces it.
- Closing the window quits without a prompt; the pause menu's Quit asks for a second confirm.
- Tab is both L2 on the keyboard and the runtime's hold-to-fast-forward key.
- Audio peaks sometimes reach full scale (possible clipping).
- Building needs the Visual Studio C++ toolchain; there is no prebuilt download, by design (see NOTICE).
- Other regions are not supported; only the USA discs listed in `recomp/game.toml`.

Update this file, with the date, whenever one of these changes.
