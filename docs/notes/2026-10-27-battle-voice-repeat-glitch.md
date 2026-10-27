## 2026-10-27 Battle voice repeat / animation skips back ~1 s (bead bat17): NOT root-caused

Report: first battle, voice "burn" repeats and the animation jumps back about 1 s.

### Tooling added (opt-in)
`recomp/rtfull/stubs/psxrt_evlog.c`: in-memory ring of CD commands (C), coalesced sector runs (R), SPU key-on/off (K),
per-frame interpreted-insn / AOT dispatch deltas (F), battle/field overlay residency (O) and guest frame-counter steps
backwards (B). Off unless `PSXRT_EVLOG=1` (`=2` also echoes to stderr). When on: F12 (no Ctrl/Alt/Win) or process exit
writes `<state dir>\logs\evlog_<date>_<time>_<tag>.txt` (the scripted `KEY F12` does the same). When off: no ring writes,
no atexit dump, F12 is not consumed. `qrun.ps1` gained `-Bios` (keep game.toml retail BIOS) and `-Card` (copy of a .mcd
as card 1; always pass a COPY). `input/continue_walk.txt`: title -> Continue -> walk left/right, F12 every ~2100 frames.

### Evidence gathered
- User card copy loads via Continue (field with bridge and waterwheel on screen by frame ~20300, OpenBIOS hidden run).
- 25 000 guest frames of field idle: evlog shows no `B` line (no guest frame rewind/savestate load), CD traffic only the
  initial read set, SPU key-on lines continuous (ambient). So the field path shows no replay.
- Rewind (`psx_rewind.c`) is off by default and not enabled by our game.toml, so it is not the cause of a 1 s jump back.

### Dead ends
- Scripted LEFT/RIGHT walking from the loaded save did not move the character (frames 20400-41500, 20 screenshots
  identical), so no random encounter was reached and the battle itself was never traced. Needs either a pad script that
  first clears whatever holds the field (dialog/event) or a savestate taken just before the battle (F5 in a real play).
- Hypotheses NOT tested for lack of a battle trace: CD sector re-read/seek while emulation is slow (check `R` runs
  repeating the same LBA span and a repeated `C 02`/`06` around the glitch), duplicated event delivery, vsync drift.

### Next step
Play to the battle with `PSXRT_EVLOG=1`, press F12 right after the glitch, and look in the dump for: an `R` run whose
first_lba repeats an earlier run within ~60 frames, a repeated SeekL/ReadN pair, `B` lines, or an `F` line with huge
`interp` (AOT overlay miss => slow frames).
