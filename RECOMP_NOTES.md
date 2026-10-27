# Recompilation notes (index)

The full engineering log, split by dated section. Each heading below links to the file that holds the unchanged text. Newest sections last; the split was made 2026-10-27 from a single 135 KB file (nothing was edited).

## 2026-10-26 Tooling research: psxrecomp vs ps1-recomp

[docs/notes/2026-10-26-tooling-research-psxrecomp-vs-ps1-recomp.md](docs/notes/2026-10-26-tooling-research-psxrecomp-vs-ps1-recomp.md) - Researched 2026-10-26 from the GitHub repos (README, docs, trees, selected source headers). Nothing was cloned or built.

## 2026-10-26 First analyze run

[docs/notes/2026-10-26-first-analyze-run.md](docs/notes/2026-10-26-first-analyze-run.md) - Run inside Windows Sandbox (no network, read-only inputs) via recomp/sandbox/; toolchain cmake-clang-v1 v1.0.14

## 2026-10-26 Second analyze run: main programs A/B/C

[docs/notes/2026-10-26-second-analyze-run-main-programs-a-b-c.md](docs/notes/2026-10-26-second-analyze-run-main-programs-a-b-c.md) - Same sandbox harness; images are SO2.BIN entries 1 (A), 2129 (B), 2576 (C) wrapped in a synthetic PS-X EXE header (load 0x8002F810, entry = ...

## 2026-10-26 Indirect call resolution

[docs/notes/2026-10-26-indirect-call-resolution.md](docs/notes/2026-10-26-indirect-call-resolution.md) - Tool: analysis/indirect.py (static, per-site forward symbolic evaluation; reads the local gitignored exe/.dec files and the sandbox ...

## 2026-10-26 Matched functions vs symbols and seeds

[docs/notes/2026-10-26-matched-functions-vs-symbols-and-seeds.md](docs/notes/2026-10-26-matched-functions-vs-symbols-and-seeds.md) - Check: python -I analysis/check_matches.py [<sandbox>/_out_run2/A/seeded/functions.tsv] (no game data; exit 1 on any problem).

## 2026-10-26 game.toml values

[docs/notes/2026-10-26-game-toml-values.md](docs/notes/2026-10-26-game-toml-values.md) - Settled from the psxrecomp source (f42d2c0, read only, nothing run) and the local discs:

## 2026-10-26 SLZ overlays into the AOT pipeline

[docs/notes/2026-10-26-slz-overlays-into-the-aot-pipeline.md](docs/notes/2026-10-26-slz-overlays-into-the-aot-pipeline.md) - Decision: neither the captures file nor an slz_members image method, but a prepared_files decoder. psxrecomp's tools/prepared_files.py ...

## 2026-10-26 Analyzer seeds were never read

[docs/notes/2026-10-26-analyzer-seeds-were-never-read.md](docs/notes/2026-10-26-analyzer-seeds-were-never-read.md) - Runs 1 and 2 (and run 3, same harness) ignored almost all of our seeds. psxrecomp-analyze --seeds parses each token with std::stoul(tok, ...

## 2026-10-26 B/C indirect sites, split starts, toolchain

[docs/notes/2026-10-26-b-c-indirect-sites-split-starts-toolchain.md](docs/notes/2026-10-26-b-c-indirect-sites-split-starts-toolchain.md) - Indirect sites (B 11, C 11): 21 settled, 1 open. Method: every decompressed module (89 unique + boot exe, from disc 1) scanned for jal, ...

## 2026-10-26 psxrecomp safety audit

[docs/notes/2026-10-26-psxrecomp-safety-audit.md](docs/notes/2026-10-26-psxrecomp-safety-audit.md) - Verdict: SAFE WITH CAVEATS, for exactly this scope: building psxrecomp-analyze and psxrecomp-game (the generator) from source at commit ...

## 2026-10-26 Host compile, A data region, overlay decoder

[docs/notes/2026-10-26-host-compile-a-data-region-overlay-decoder.md](docs/notes/2026-10-26-host-compile-a-data-region-overlay-decoder.md) - Host generation (rerun). The earlier host and _out_gen1 outputs were gone, so recomp/host/build_generate.ps1 ran again (clone at f42d2c0, ...

## 2026-10-26 Runtime link audit (boot exe)

[docs/notes/2026-10-26-runtime-link-audit-boot-exe.md](docs/notes/2026-10-26-runtime-link-audit-boot-exe.md) - Verdict: NOT SAFE to link, under this task's rules (audited sources only, no downloads, no prebuilt binaries). Nothing found looks ...

## 2026-10-26 AOT overlay pipeline run (SLZ, program A)

[docs/notes/2026-10-26-aot-overlay-pipeline-run-slz-program-a.md](docs/notes/2026-10-26-aot-overlay-pipeline-run-slz-program-a.md) - The pipeline runs: all 89 SLZ chunks decode and verify, and program A (e1c0) goes through static to C. Work copy of the clone (copied, not ...

## 2026-10-26 Host generation B/C, A orphan starts

[docs/notes/2026-10-26-host-generation-b-c-a-orphan-starts.md](docs/notes/2026-10-26-host-generation-b-c-a-orphan-starts.md) - Host build at the audited f42d2c0 (recomp/host/build_generate.ps1 -Program boot,A,A_reach,B,C, then compile_generated.ps1 -Program ...

## 2026-10-26 Minimal runtime (recomp/rt): linked, boots with the user's BIOS

[docs/notes/2026-10-26-minimal-runtime-recomp-rt-linked-boots-with-the-user-s-bios.md](docs/notes/2026-10-26-minimal-runtime-recomp-rt-linked-boots-with-the-user-s-bios.md) - Verdict: SAFE to link and run on the host. The link set is: the 10 psxrecomp headers (read in full this session, below) + the generated ...

## 2026-10-27 BIOS usage

[docs/notes/2026-10-27-bios-usage.md](docs/notes/2026-10-27-bios-usage.md) - Script: SO2_DATA=<checkout with extracted/, analysis/out/d1/, roms/, tools/ref/psxrecomp> python -I analysis/bios_usage.py (--selftest ...

## 2026-10-26 AOT static for all 89 SLZ chunks, our seeds, MSVC compile

[docs/notes/2026-10-26-aot-static-for-all-89-slz-chunks-our-seeds-msvc-compile.md](docs/notes/2026-10-26-aot-static-for-all-89-slz-chunks-our-seeds-msvc-compile.md) - All 89 chunks go through static to C, in two seed modes, and all of it compiles with MSVC: 0 errors. Same work copy ...

## 2026-10-27 boot bring-up

[docs/notes/2026-10-27-boot-bring-up.md](docs/notes/2026-10-27-boot-bring-up.md) - Runs: recomp\rt\run_rt.ps1 -Name <n> -Roms <home>\star-cean-second\roms -Vblanks N -Frames 60 (logs, ram.bin, vram.ppm, display.ppm, ...

## 2026-10-27 Full psxrecomp runtime (recomp/rtfull): linked with MSVC, boots to the title menu

[docs/notes/2026-10-27-full-psxrecomp-runtime-recomp-rtfull-linked-with-msvc-boots-.md](docs/notes/2026-10-27-full-psxrecomp-runtime-recomp-rtfull-linked-with-msvc-boots-.md) - Result: psxrecomp-game.exe (psxrecomp f42d2c0 full runtime, 164 runtime TUs + OpenBIOS backend + boot exe C) links with MSVC 14.44 and runs ...

## 2026-10-27 overlay link

[docs/notes/2026-10-27-overlay-link.md](docs/notes/2026-10-27-overlay-link.md) - Programs A, B, C and all 89 SLZ chunks now run as compiled C in recomp/rtfull; the interpreter count over a 200 s headless boot drops from ...

## 2026-10-27 SCPH5501 backend

[docs/notes/2026-10-27-scph5501-backend.md](docs/notes/2026-10-27-scph5501-backend.md) - Result: psxrecomp-game.exe boots on the user's SCPH-5501 (recompiled LLE backend linked ahead of time) to the Star Ocean title menu, and ...

## 2026-10-27 interactive

[docs/notes/2026-10-27-interactive.md](docs/notes/2026-10-27-interactive.md) - Result: psxrecomp-game.exe runs windowed (SDL3 + OpenGL 3.3 on the RTX 4070 SUPER, SDL3 audio) at 1.00x, and a scripted pad run gets past ...

## 2026-10-27 overlay seeds

[docs/notes/2026-10-27-overlay-seeds.md](docs/notes/2026-10-27-overlay-seeds.md) - Per-chunk seed lists are now verified code, and the pipeline audit passes for the chunk that failed. Commits: 1c030f6 (make_seeds), 0dde195 ...

## 2026-10-27 disc 2

[docs/notes/2026-10-27-disc-2.md](docs/notes/2026-10-27-disc-2.md) - Result: the game's disc swap works in recomp/rtfull through the runtime's own lid path, and disc 2 is fully reachable: booted from disc 2 ...

## 2026-10-27 video

[docs/notes/2026-10-27-video.md](docs/notes/2026-10-27-video.md) - Title menu artwork restored (GL renderer dropped a 640x480 upload made while GP1 was still 24-bit), no black strip at the right of FMV frames, display options in settings.ini ...

## 2026-10-27 integration

[docs/notes/2026-10-27-integration.md](docs/notes/2026-10-27-integration.md) - One script builds the whole port and it boots on SCPH-5501 into the first field scene, windowed, 60 fps, with audio; a fresh clone + the ...

## 2026-10-27 Audio

[docs/notes/2026-10-27-audio.md](docs/notes/2026-10-27-audio.md) - Logo silence is game-side (no SPU key-ons), STR movies carry no audio (SPU music); fixed crackle after stalls, sound loss while dragging the window, device-change handling.
