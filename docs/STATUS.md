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
| Audio output (2026-10-27) | stats/WAV runs in `docs/notes/2026-10-27-audio.md`: 150 s boot run 26 dropout gaps before, 2 after; simulated 6 s window drags keep full speed with 0 underruns; forced device reopens on WASAPI with 0 underruns |
| Decomp | 460 main-program functions byte-match; 457 pass `scripts/rules.py` |

## Untested (expect bugs)
- Battles.
- Saving to and loading from the memory card in game.
- The disc 2 prompt and everything on disc 2.
- Steam / Steam Deck (no Steam installation available).
- Anything past the first cave; only scripted input has been used.
- Real title-bar drags/resizes with the mouse and real audio device switches (KVM, unplug, sleep): only simulated (`SC_MOVE` modal loop, forced reopen).
- Gamepads and rumble beyond what SDL3 provides; no real-hardware pad matrix.

## Known limits
- A mid-session disc swap is manual: press F10 to change disc.
- Audio peaks sometimes reach full scale (possible clipping).
- The disc load after the PlayStation logo runs the game at 15-25% speed for about 1.5 s: the logo jingle has a ~1 s gap.
- The Enix/tri-Ace logos are silent because the game plays nothing there (no SPU key-ons); not compared with a real PS1.
- Building needs the Visual Studio C++ toolchain; there is no prebuilt download, by design (see NOTICE).
- Other regions are not supported; only the USA discs listed in `recomp/game.toml`.

Update this file, with the date, whenever one of these changes.
