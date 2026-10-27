## 2026-10-26 game.toml values
Settled from the psxrecomp source (f42d2c0, read only, nothing run) and the local discs:
- `exe = "disc/SCUS_944.21"` with `[prepare_disc] out_dir = "disc"`, `boot_exe = "SCUS_944.21"`: prepare_disc.py writes extracted files to `<out_dir>/<name>` and defaults `out_dir` to `prepared_disc` (load_config); probe_disc.py renders `exe` as `<out_dir>/<boot_exe>` with default `disc`. `bin_name`/`cue_name` are set because prepare_disc otherwise takes `Path([game] disc).name`, which is the `.cue` name.
- `disc` + `discs`: the C++ loader prefers `discs` (config_loader.cpp); the Python tools (prepare_disc.py, aot_overlay_pipeline.py `game_disc`) read only `disc`, so `disc` names disc 1. The file names are those of the local single-track MODE2/2352 dumps.
- `seeds`: main_psx.cpp parses each non-`#` line with `strtoul(..., 16)`; our bare-hex `config/seeds.txt` fits. `strict = true` is the loader default ("currently always true"). `discovery = "whole-image"`: `reachable` follows `jal` only and leaves `jalr` targets to the interpreter; `--exact` found 140 of 570 boot functions.
- `overlay_cache = true`: probe_disc.py's default, and GAME_PROJECT_SETUP.md says the packager refuses without it; it records the overlays the runtime interprets.
- **No XA, no CD-DA:** a census of every sector's mode 2 subheader finds no XA audio (submode 0x04) and no realtime video (0x02) on either disc (disc 1: 236155 data, 216 Form 2, 160 other); both cues are single-track. STR movies are ordinary data entries in SO2.BIN.
- Still `TODO(verify)` (4): `disc_speed`, `bios_hle` (runtime behaviour, need a boot test), `[video]/[widescreen]` (a look decision); in the AOT profile `expected_records` (it counts the generic extractor's recipes, known only after one `extract` run).

