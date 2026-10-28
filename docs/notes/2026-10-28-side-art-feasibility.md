## 2026-10-28 Side-panel art from the player's own discs

Goal: pictures for the wide-screen side panels, taken from the player's disc images on their PC only (local, gitignored, never committed or shipped). Tool: `scripts/extract_side_art.py` (stdlib only, PNG written with zlib).

### What is on disc 1 (survey of all 4155 SO2.BIN entries, nested SLZ streams included)
| Source | Format | Count | Verdict |
|---|---|---|---|
| Field/scene backgrounds | entries 3207..4115 (type `03000000` container, ~100 sectors each). Item 0 holds an SLZ stream: 3208-byte camera header (320x240 view), then **256x256 RGB555 pages (131072 bytes)**, 1-2 per scene; further streams hold the tile/poly data (not decoded) | ~800 scenes | **used**. Pages are atlases: pre-rendered scene pieces with unused cells (pure black). Holes are filled by push-pull before saving; pages with >30 % holes are dropped |
| Title art (Earth + ship) | entry 18 (19, 20 = copies), chunk 0 = **24-bit TIM 440x330** | 1 | **used**, clean |
| Logo / intro stills | entry 17 (TIM 24-bit 440x102 logo, transparent), entry 11 (3 x 320x240 RGB555 with a 10-byte header: Enix-style splash) | few | not used (logo is mostly black, splash is plain) |
| Misc TIMs | 2980.. (4-bit 256x128 font/UI), 4465.. (16-bit 160x160 sprites) | ~20 | no |
| STR movies | raw MODE2 Form1 sectors, 20 movies on disc 1 (sector header magic `60 01 01 80`, 320x240, 10 sectors/frame, BS version 2, 32-byte header with the 8-byte BS header at +20 and again at +32) | thousands of frames | **not feasible here**: my BS v2 / MDEC decoder (standard VLC table, LE u16 MSB-first) fails on every real frame (checked LE/BE/bit-reversed/word-swapped variants), so the stream is not plain libpress BS or my reading is wrong; the game decodes it in its own code. Left out rather than ship a guess. Would be the best source (clean full-frame CG) if someone traces the game's decoder |
| Disc 2 | same entry layout (4136 of the entries are identical); not read by the tool (it would add nothing) | | |

### How the tool reads the discs
`analysis/toc.py` (SO2.BIN table of contents, `docs/SO2BIN_TOC.md`) + `analysis/slz.py` (`docs/SLZ.md`) are reused; raw 2352-byte BIN sectors, user data at +24. Disc images are found as `*.bin` under `--roms` (default `roms\`, read-only; the BIOS `scph*` is skipped). 22 s at low priority on this PC for 40 pictures.

### Selection
Every 11th scene entry (even spread over the game), plus the title picture. Dropped: near-blank/too dark or bright (luma stddev < 22, mean outside 20..235), near-duplicates (8x8 average hash, Hamming <= 8), pages with > 30 % unused area. Output `play\sidefill\side_NN_<tag>.png` (RGB, 256x256 scenes, 440x330 title); old `side_*.png` in the folder are replaced.

### Hook for INSTALL.bat / the launcher
```
tools\python\python.exe -I scripts\extract_side_art.py --roms roms --out play\sidefill --count 40
```
Exit 0 = wrote pictures, 1 = nothing usable, 2 = no `.bin` found. Safe to re-run; needs no build step. `repo_guard.py` now fails if any png/tim/str/jpg under `play/` is tracked (`/play/` is also gitignored).

### Unverified / limits
Scene pages are atlas pieces, not assembled 320x240 frames (tile table not decoded); some pages show tile jumbles or smeared fills, fine once blurred. Only disc 1 was run. Visual check: 20 of the 40 outputs viewed on a contact sheet.
