# Reporting a problem

Use the issue forms: [bug report](../../issues/new?template=bug_report.yml) or
[install problem](../../issues/new?template=install_problem.yml).

## Never attach
- Game files: disc images (`.bin`, `.cue`, `.iso`, `.chd`, `.7z`), `SO2.BIN`, anything extracted from a disc, generated C
  or a built `.exe`. They are copyrighted and will be deleted.
- BIOS images (Sony or otherwise). OpenBIOS is already part of the build.
- Memory cards or save states (`saves\*`). If a save is the problem, describe it (where, which disc, what you did).
- Screenshots or video are fine, but not captures of the full game for long stretches.

## Scrub personal information
Logs contain your Windows user name in paths. Replace your home folder with `<home>` before posting
(for example `C:\Users\<home>\Games\StarOcean` becomes `<home>\Games\StarOcean`). Check for e-mail addresses and
account names too.

## What to include
- Install problems: the last ~60 lines of `install.log` (in the repository folder) and the step it stopped at.
- Game problems: the tail of `play\last.log` and `play\last.err`, which disc, and what you did just before.
- Windows version, GPU and driver version, whether you use OpenBIOS or your own BIOS (say which, do not send it),
  and whether the game folder is on a local drive (not OneDrive or Program Files).
- Whether the problem repeats and what you expected instead.

Check [docs/STATUS.md](docs/STATUS.md) first: battles, saving, disc 2 and Steam are known untested areas, and your
report on them is very welcome.
