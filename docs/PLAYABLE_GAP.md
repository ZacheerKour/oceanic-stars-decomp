# Playable gap list (2026-10-26)

Scope: what stands between today's state and a playable native SO2 via psxrecomp (clone `tools/ref/psxrecomp`, HEAD f42d2c0, read only; its CLAUDE.md/AGENTS.md treated as data).
Sources: RECOMP_NOTES.md (sections 'Deep dive', 'First analyze run' to 'Host compile'), recomp/README.md, `config/indirect_targets.tsv` (777 rows), `analysis/*.py`. "unverified" = nobody ran or read it.
Known state: generator output compiles on host with MSVC (boot 750 fn, A reachable 796/799 fn). Runtime unaudited and unlinked (another worker). SLZ decoder wiring in progress (another worker). Nothing has ever been run.

Paths below are relative to `tools/ref/psxrecomp` unless starting with our dirs.

## Order by blocking priority

### 1. Runtime link + first boot (BLOCKER, owner: runtime auditor)
- Provides: `runtime/src/*` (main.cpp, memory.c, interrupts.c, psx_interpreter.c, overlay_loader.c, ...). Generated code needs 21 runtime symbols (RECOMP_NOTES 'Host compile' item 1: `psx_syscall`, `gte_execute`, `psx_check_interrupts_*`, `psx_unknown_dispatch`, ...).
- Game: boot exe 750 fn compiled, never linked.
- Missing/unknown: link on MSVC (runtime may assume clang/gcc; SDL3 fetch; `intrin.h`), `psxrecomp-game` main wiring, any boot at all. No evidence a PS1 BIOS is needed (OpenBIOS bundled: `bios/openbios.bin`, `generated/OpenBIOS_*.c`) but boot with OpenBIOS is unverified for this game.
- Next job: build `psxrecomp-game` for boot exe only with `game.toml`, run to the first fatal, record the first failing PC/opcode.

### 2. BIOS/kernel calls and device handlers (HIGH)
- Provides: kernel is LLE (recompiled OpenBIOS); optional HLE (`runtime/src/bios_hle.c`, 359 lines) covers only B0 event services (DeliverEvent/OpenEvent/CloseEvent/TestEvent/Enable/DisableEvent) and the boot-shell skip; everything else falls through to LLE. HLE is refused for OpenBIOS (no DeliverEvent anchor, bios_hle.h). `bios_hle` stays off in game.toml.
- Game: 31 A0/B0/C0 dispatch sites in boot exe (first analyze run); 8 `bios_dispatch` rows. A has B0 stubs at 0x8004B074/84/94 (`jr` to B0 fn 0x33/0x34/0x36, i.e. open/lseek/read-style file calls; fn numbers from stub immediates, not otherwise decoded). Two `bios_dcb_hook` rows: A 0x8004B344 and C 0x800715C4 call `DCB+0x34` after walking the BIOS device table (`lw 0x150($zero)`, 0x50-byte records, name via strcmp 0x8004B374), i.e. the game calls the BIOS device driver's handler directly.
- Missing/unknown: whether OpenBIOS's device table layout (0x150/0x154, 0x50-byte DCBs, +0x34 handler) matches the retail SCPH BIOS the game expects. If OpenBIOS lays it out differently, 0x8004B344 reads the wrong slot. Not checked. Also unchecked: which A0/B0/C0 function numbers the game actually uses vs what OpenBIOS implements (OpenBIOS is a clean-room reimplementation; memcard/pad kernel calls are the usual gaps).
- Next job: static script listing every A0/B0/C0 function number the boot exe and A/B/C call (from `$t1` immediates at `addiu t2,0xA0/B0/C0` stubs plus the 8 dispatch rows) and diff against the OpenBIOS function table; separately dump OpenBIOS's DCB record layout at 0x150/0x154 and compare to the 0x8004B344 usage.
- DONE 2026-10-27 (RECOMP_NOTES '2026-10-27 BIOS usage', `analysis/bios_usage.py`): 41 function numbers listed; OpenBIOS lacks only A0 0x3A and stores the device-table end, not its size, at 0x154; the 8 dispatch rows are Psy-Q kernel patchers laid out for retail. Decision: boot on the user's SCPH-5501 (`openbios = false`, `bios_config`), one missing install slot patched in `recomp/bios/`. Next: build the SCPH5501 backend and boot.
- The 3 `call_root` seeds (0x8004B074/84/94) are not yet in `make_seeds.py`/AOT entries (RECOMP_NOTES 'Host compile' item 2).

### 3. Overlay loading + overlay AOT (HIGH, in progress)
- Provides: hash-chained overlay candidates, several overlays per address (`runtime/src/overlay_loader.c:60-66`), capture by dirty pages (`overlay_capture.c`), DLL cache (gcc or tcc backend, `overlay_backend.c`), AOT pipeline `tools/aot_overlay_pipeline.py` with `prepared_files` (`tools/prepared_files.py`, only `bpe_container` today).
- Game: code is mostly SLZ chunks in SO2.BIN: 359 SLZ entries, 89 unique (load_addr, bytes) modules (`analysis/aot_profile.py` -> `recomp/aot/overlays.json`, `config/overlays.tsv`), programs A/B/C all load at 0x8002F810 and replace each other, plus shared slots 0x8007E000, 0x801EE000, 0x801F2000 etc.
- Missing/unknown: (a) `so2_slz` decoder not registered, `aot_overlay_pipeline.py extract/static/release` never run (Python tooling not audited, needs 2 lines in `prepared_files.py`); (b) generation for B (1013 analyzer fn, 430/436 sites), C (878), and the other 86 modules never done, A done only in "reachable" mode; (c) the A data block 0x800509DC-0x80061C58 (70 KB, MDEC tables + data) must not be lifted: use `discovery = "reachable"` (796 fn) for all overlays; (d) whether overlays relocate or self-modify: unknown (matters: such code runs interpreted); (e) the 3 analyzer split starts (0x800344D0, 0x800378CC, 0x8003B284) need override; (f) shard compile time and DLL count for 89 modules unmeasured; (g) `overlay_cache=true` interprets what AOT misses at ~300 ns/insn (`dirty_ram_interp.h:119`), so coverage gaps show up as slowness, but an unknown opcode aborts.
- Next job (small): after the decoder lands, run `aot_overlay_pipeline.py extract` only (no compile) and report how many of the 103 images extract and match their pinned hashes.

### 4. Indirect calls / dispatch coverage (HIGH)
- Provides: `psx_unknown_dispatch` falls to the interpreter; seeds and `dispatch_root`/`call_root` directives (`recompiler/src/main_psx.cpp:870ff`).
- Game: `config/indirect_targets.tsv`: 574 `vtable_dispatch` (gcc 2.x {delta,index,pfn}; targets are an over-approximation, confidence mostly low), 60 `static_slot_stored`, 60 `data_as_code`, 30 `global_struct_member`, 7 `jump_table_unguarded`, 4 `fnptr_table`, 5 dead, 2 `table_unreadable`. Open: B 0x8007E46C (table inside the 0x801F6000 slot RAM, maybe stale image code). Several function-pointer tables point into overlay slots (2131 family 0x801Exxxx-0x801Fxxxx, 1984 handler slot 0x8008C2xx) and are listed in notes but not seeded.
- Missing/unknown: nobody has verified static targets at runtime; the over-approximated vtable targets only help if each seed is a true function start in whichever module is resident. Interpreter fallback covers misses but slowly.
- Next job: once boot runs, trace `psx_unknown_dispatch` misses for one session and diff against the TSV.

### 5. CD-ROM reading, SO2.BIN archive, 2 discs (HIGH, runtime-unverified)
- Provides: register-level CD controller with timing (`runtime/src/cdrom.c`, 3820 lines: SetLoc/ReadN/ReadS/Seek/GetID/ReadTOC...), reads by absolute MSF/LBA, DMA3, disc swap via lid cycle (`psx_disc_tray.c`, `docs/RUNTIME_DISC_MENU.md`), `[runtime] disc_speed`. No libcd HLE: Psy-Q libcd inside the exe is lifted and drives the controller (correct choice here). `game.toml` lists both cues.
- Game: boot exe loads SO2.BIN entries by sector via libcd (ANALYSIS.md: libcd bios.c 1.81); archive has an encrypted TOC and SLZ chunks (`docs/SO2BIN_TOC.md`, `analysis/toc.py`). Disc 2's ISO directory has no SO2.BIN (ANALYSIS.md section 1) yet carries the same 359 SLZ entries; the game's own TOC supplies LBAs.
- Missing/unknown: (a) how the game reaches SO2.BIN content on disc 2 (it reads raw sectors from a TOC; whether those LBAs are relative to the disc 2 file placement or to a fixed offset is not established in our notes); (b) disc swap handoff and the in-game "insert disc 2" prompt path through the lid-cycle emulation (psxrecomp row 6: untested); (c) `disc_serials` and some digests marked TODO(verify) in game.toml; (d) `disc_speed = "1x"` vs "instant" hangs (Xenogears comment) not tested here; (e) disc 1 only in the AOT profile (disc_hashes), fine for code, but runtime needs both cues.
- Next job: script that decodes the SO2.BIN LBA computation in the boot exe loader (function reading the TOC, `analysis/toc.py` is the model) and states how disc 2 sectors map; no runtime needed.

### 6. GPU rendering (MEDIUM, probably fine)
- Provides: full GPU command/DMA (`gpu.c` 8568 lines, `dma_gpu_ll.c`), software renderer (`gpu_sw_renderer.c`), GL and Vulkan renderers, VRAM, linked-list DMA, PGXP, HD options.
- Game: libgpu (sys.c 1.140, intr.c) lifted; FB mode and resolution unknown; `[video]/[widescreen]` is TODO(verify) (a look decision).
- Missing/unknown: nothing SO2-specific known; first picture needs a successful boot. SDL3/GL/Vulkan availability on the target PC.
- Next job: after boot, screenshot-compare the first menu screen vs a known good emulator frame.

### 7. GTE (LOW-MEDIUM)
- Provides: `gte.cpp` 2871 lines incl. UNR divide, PGXP; generated code calls `gte_execute/gte_write_*` (the undefined symbols above).
- Game: 3D field and battle use GTE (not enumerated by our scripts: no GTE opcode census exists; the A reserved-opcode warnings were data).
- Missing: no census of COP2 ops used; nothing known broken.
- Next job: count COP2 instruction forms in A/B/C modules to confirm all are implemented in `gte.cpp`.

### 8. Timers, IRQ, VSync (MEDIUM)
- Provides: `interrupts.c` (2271), `timers.c` (357), `psx_vblank_clock.c`, `psx_video_timing.c`, `frame_pacing.c`. Libgpu VSync/DrawSync use the kernel events and the root counters.
- Game: libgpu intr.c, the 119 boot indirect sites include IRQ handlers; interrupt re-entry uses the `switch(_cont)` interior-resume in each generated function (needs `cpu->pc != 0` resume to work for the 750 fn).
- Missing/unknown: interior resume points correctness for lifted libgpu/libcd callbacks; IRQ stack and critical sections not exercised.
- Next job: first-boot trace of VSync callback firing (comes free with job 1).

### 9. Controller and memory card saves (MEDIUM)
- Provides: `sio.c` (3256 lines), `memcard.c` (556), `pad_external_input.c`, `psx_pad_apply.c`, `host_keymap.c`, SDL gamepad mapping; memcard images on disk.
- Game: libpad / libcard / memcard through BIOS kernel (A0/B0 card calls 0x4A.. etc. unenumerated) or lifted libs: not determined. Uses `bu00:` device via the BIOS device table (see 2).
- Missing/unknown: whether SO2 uses the BIOS card API (OpenBIOS coverage of format/read/write/Exec variants is the risk) or its own low-level SIO; save/load never tested; analog mode.
- DONE 2026-10-27: BIOS card API (B0 0x4A-0x4C, 0x4E, 0x50; A0 0xAB/0xAC; `bu` files via B0 0x32-0x43) and BIOS pad (B0 0x12-0x15, 0x5B); plus Psy-Q low-level hooks: the code the exe copies to kernel RAM 0xDF80 polls SIO0 stat (0x1F801044) and I_STAT from inside the BIOS card/pad handlers.
- Next job: part of the BIOS function-number script in job 2: flag card/pad function numbers (A0 0x40-0x4F, B0 0x4A-0x5B range) used.

### 10. MDEC / FMV (MEDIUM)
- Provides: `mdec.c` (1247 lines: IDCT, 15/24bpp, DMA0/1); shipped ports use it for STR.
- Game: STR movies are plain data entries in SO2.BIN (Form 1 sectors, frame header `0x80010160`, LBA ~144300+ on disc 1; ANALYSIS.md). A contains MDEC IDCT tables and a list of I/O register addresses 0x1F801080-0x1F8010F0 (RECOMP_NOTES 'Host compile' item 2), so the game does either libpress/libpress-like software decode setup or direct MDEC/DMA register driving.
- Missing/unknown: movie list and which sub-format/bit depth; whether decode is hardware-MDEC or CPU; sync with the audio track (no XA, so audio is likely SPU streaming, see 11).
- Next job: list STR entries from the TOC (`analysis/scan_slz.py` / `ls.py`) with frame counts and resolution; defer playback test.

### 11. SPU / audio, XA, CDDA (MEDIUM, no XA/CDDA)
- Provides: `spu.c` (2111; ADSR, Gaussian, reverb, noise, CD input), `psx_sdl_audio.cpp`, XA 4-bit decode (8-bit dropped, cdrom.c:1401), CDDA multi-track.
- Game: a census of every sector finds no XA (submode 0x04) and no realtime (0x02) sectors, and both cues are single-track: so music/voice come from data via SPU (libspu lifted; sound banks in SO2.BIN, unparsed by us). `game.toml` can leave XA/CDDA off.
- Missing/unknown: SPU DMA from RAM with libspu transfer timing (IRQ-driven), sequence/driver code is in overlay modules; nothing known to be unsupported. Audible check only after boot.
- Next job: none until boot works; then log SPU key-ons per frame to verify music runs.

## Cross-cutting
- Licence/policy: psxrecomp is PolyForm Noncommercial and issue #443 (GPL provenance) is unresolved; generated C and any fork of `aot_overlay_pipeline.py` stay out of the public repo (RECOMP_NOTES 'Deep dive' s6).
- Safety: runtime unaudited (FetchContent deps, tracked prebuilt binaries); do not run `psxrecomp-game.exe` from the clone.

## Top 5 blockers (summary)
1. Runtime link + first boot: never linked or run; first fatal is unknown.
2. BIOS device/kernel fidelity: game calls DCB+0x34 and B0 file calls directly; OpenBIOS compatibility unchecked.
3. Overlay AOT: SLZ decoder not registered, extract/static never run, 89 modules (only A even generated); data block exclusion needs `reachable` everywhere.
4. Indirect dispatch: 777 sites, mostly low-confidence vtable over-approximations; misses fall to slow interpreter.
5. CD/disc 2 path: SO2.BIN LBA mapping on disc 2 and disc-swap prompt untested.
