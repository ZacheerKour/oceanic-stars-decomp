## Deep dive: ps1-recomp (2026-10-26)

Source: shallow clone at `tools/ref/ps1-recomp` (HEAD ab4f0e0, 2026-09-20). Paths below are relative to that clone.
Repo: github.com/PS1Recomp/ps1-recomp (Dellareti), ~211 commits, 1 contributor, 22 stars, last push 2026-10-12,
GPLv3 (LICENSE is the GPL text; GitHub reports NOASSERTION). It is also an AI-assisted ("Claude Code") project.

### 1. Architecture and build
- Three separate binaries talking via files (ARCHITECTURE.md:31-48): `ps1Analyzer` (disc/ELF -> TOML), `ps1Recomp`
  (TOML -> one `recompiled_out.cpp`), `ps1Runtime` (SDL2/OpenGL host linked with that .cpp). Plus optional ImGui studio.
- CLI: `ps1Analyzer <boot.exe|elf> out.toml`; `ps1Analyzer --scan-overlays <disc.bin> [out.toml]` (ps1Analyzer/src/main.cpp:30-46);
  `ps1Recomp config.toml out.cpp` (ps1Recomp/src/main.cpp:~175); `ps1Runtime --config game.toml`.
- Input is an ELF-style load via ELFIO (`binary.path`), PS-X EXE only handled for overlays (main.cpp:~575) and via cdrom/exe_extractor.cpp.
- Emission is 1 giant file, one C++ function per MIPS function, `void f(uint8_t* rdram, recomp_context* ctx)`; 1:1 instruction translation
  (instruction_emitter.cpp), GTE via gte_emitter.cpp. Author lists single-file emission as an open scaling issue (ARCHITECTURE.md:165).
- Config keys actually read by ps1Recomp/src/main.cpp (no schema doc exists; this is reverse-engineered from code):
  `[binary] path, entry_point(hex string)`; `[[functions]] name, address(hex string), size(int), source, leaf`;
  `[[stubs]] name,address`; `[[hle_functions]] address,name,hle(bool)`; `[hle_overrides] "0xADDR"="hle_Name"`;
  `[missing_functions] "0xADDR"="C++ body"`; `[[overlays]] name, ram_base(int), size(int), binary_path|disc_path, functions=[int..]`;
  `[disc] cue_path`; `[paths] memcard_dir` (runtime).
- Minimal example (derived from the readers, not shipped anywhere):
```toml
[binary]
path = "SLUS_009.99.exe"      # relative to the cwd, not the config
entry_point = "0x80010000"
[[functions]]
name = "func_80010000"
address = "0x80010000"
size = 64
[hle_overrides]
"0x80012345" = "hle_VSync"    # calls ps1::psyq::hle_VSync(ctx); name must exist in the runtime
[disc]
cue_path = "game.cue"
```
- Toolchain: C++20, CMake 3.20+, SDL2, OpenGL 3.3, submodules (ELFIO, toml11, fmt, gtest). CI is ubuntu-latest only
  (.github/workflows/ci.yml). README documents Ubuntu/macOS only. The generated code includes `<execinfo.h>` (main.cpp:~220),
  which is glibc/macOS only, so native MSVC fails; Windows would need WSL or MSYS2 plus a patch. Not verified by building.
- Rayman validation: "~59 fps with correct VRAM" achieved with imperative patch script `tools/patch_rayman.py`, which was
  deleted before open-sourcing; the clean regen path has a brightness=0 / dma2=5 bug and has not been re-validated (README Status, ISSUES.md #2).
  So validation = pixel/VRAM checks (`tools/validate_game.py`, `run_and_report.py`) on one game, now regressed.
  Crash Bandicoot 1 boots but needs env bypass `PS1_SKIP_31BF8=1` (ISSUES.md #1); a Crash-specific GOOL interpreter is in-tree (ps1Runtime/src/gool).

### 2. Overlays / dynamic code
- Static, ahead-of-time per-overlay recompile only. `[[overlays]]` entries are read from a file (`binary_path`), PS-X EXE header
  auto-detected, functions either listed or auto-found by `addiu $sp,$sp,-N` scan (main.cpp:551-640). Names: `overlay_<name>__<ADDR>`.
- BUT all functions go into one `addr_to_name` map -> one `std::unordered_map<uint32_t, fn>` dispatch table (main.cpp:~700-730).
  Two overlays with the same load address overwrite each other: last wins. The per-overlay `OverlayHandler::emitDispatchTable()`
  (`lookup_overlay_func(addr, active_overlay)`, overlay_handler.cpp:106) is only used by unit tests; never called by main.cpp.
- Runtime `OverlayManager` (overlay_manager.cpp) tracks overlap and fires an activate/deactivate hook on a RAM write overlapping a
  region, fed by `file_io.cpp:107` notifyMemWrite. Nothing instantiates it in main_host.cpp and `setOverlayManager` is never called; the hook is never bound
  to the dispatch table. Effectively dead scaffolding. No detection by content hash, no swap of code identity.
- Unknown jump targets: `recomp_dispatch` (dispatch_emitter.cpp) tries table, KSEG normalise, BIOS A0/B0/C0, `jr ra` trampolines, then
  aborts with host backtrace unless `PS1_DISPATCH_PERMISSIVE=1`. No interpreter fallback.
- Jump tables: pattern matcher walking backwards from `jr` (jump_analysis.cpp): ADDU/SUBU of LUI+ADDIU base and a scaled index, stride recovery
  through shift/add chains, bound from SLTIU, targets clipped to the function; only tables whose base is inside the same function. Also
  `lui/addiu $ra` internal-return detection. Notes in-source admit a misdetected base (+4) for one Crash table.
- For shared-address overlays you would need: per-overlay function namespaces (exists), a dispatch table keyed on (active overlay id, addr)
  (partly written, unused), a hook at the SLZ-load/CD-read site that sets active id (OverlayManager notifyMemWrite is the right shape), plus
  unmapped-target handling. i.e. the design is roughly what we need but 100% of the wiring is missing.

### 3. Runtime
- GPU: software rasteriser into a 1024x512 16-bit VRAM array (gpu.cpp 1886 lines: flat/gouraud/textured tris+quads, rects, dither, blend,
  GP0 copies); OpenGL 3.3 only uploads the whole VRAM texture and draws the display area through a shader (renderer_opengl.cpp:176-236).
  No Vulkan, no resolution scaling, no hardware polygon path. Accurate in the sense of PS1 VRAM semantics, slow/limited for upscaling.
- CD-ROM: drive state machine (cdrom_controller.cpp) reading raw 2352-byte sectors by LBA from CUE/BIN via VirtualFs, with Setloc, ReadN/ReadS,
  SetFilter, XA filter fields. PsyQ HLE `CdRead` completes synchronously (psyq_libcd.cpp:141-175). Only first data track ISO9660; multi-disc swap not seen.
- XA: SPU has an XA/CD-DA mixing path (spu.cpp:353, 793); depth unverified.
- SPU: 24-voice ADPCM, reverb config present; docs say envelope/reverb/pitch-mod are approximations.
- MDEC: real IDCT/YCbCr decode code exists (mdec.cpp:209-410) and is wired to DMA (main_host.cpp:565-618), but ARCHITECTURE.md says FMV is stubbed/skipped; stale doc or incomplete, unverified. No STR demux seen.
- GTE: RTPS/RTPT, NCLIP, OP, DPCS/DPCT, INTPL, MVMVA, NCDS/NCDT, CDP, NCCS/NCCT, CC, NCS/NCT, SQR, DCPL, AVSZ3/4, GPF/GPL all present (gte.cpp); looks complete in op list, flag/overflow accuracy not checked.
- HLE: BIOS A/B/C tables, heap, events, file I/O in `bios/`; PsyQ functions detected by SHA-256 of function bodies (3463 hashes, 14 SDK versions,
  ps1Analyzer/data/psyq_signatures.toml 38k lines) and dispatched by name via `psyq_register/psyq_dispatch` (psyq_registry.cpp:31-136).
  Many registered as `hle_noop_stub`. The SHA matching is the one genuinely novel piece; it only helps if the game's libs match a known PsyQ build (SO2 was GCC 2.7.2 psx, plausible).

### 4. Function replacement
- `[hle_overrides] "0xADDR" = "hle_X"` emits `void func_ADDR(rdram, ctx){ ps1::psyq::hle_X(ctx); }` (main.cpp:451-460). The target must be a
  `ps1::psyq::` function taking only `recomp_context*`; it reads args from `ctx->r[A0..A3]`, writes `ctx->r[V0]`. No `rdram` argument (use ctx->memory),
  no namespace choice, no way to add user C files without editing the runtime. `[missing_functions]` is the open variant: an arbitrary one-line C++ body string.
  ABI is "emulated MIPS register file in ctx", i.e. same as ours would be: fine for custom code, awkward for clean C.

### 5. Quality, tests, activity
- 557 tests claimed, ~714 TEST macros in ps1Test (analyzer/recompiler/runtime/pipeline); CI runs them in stub mode with no game.
- Code is readable and comment-rich but obviously agent-generated; ARCHITECTURE.md contradicts code (MDEC, overlays). Only Crash/Rayman exercised.
- Issues/PRs: #1 and #2 (closed, Crash) were merged PRs by the author; #5 open PR (Crash audio/saves/gamepad); #4 open issue from a user porting another game saying the AI agent mis-places code.
  1 contributor, 22 stars, last push 2026-10-12.
- License: GPLv3. Linking, vendoring or deriving code puts our code under GPLv3. Reading it, citing file:line, and reimplementing ideas
  independently in our MIT repo is fine. Do not copy code/tables (including the signatures TOML, which is derived from PsyQ libs anyway).
  GPL benefit for modding: none that psxrecomp lacks (the generated output is ours either way).

### 6. Verdict
- Better than psxrecomp: (a) SHA-256 PsyQ library function identification (could reproduce independently with our own scanner), (b) fail-loud
  unmapped dispatch with backtrace, (c) jump-table stride recovery through shift/add chains (jump_analysis.cpp), (d) test count and the studio GUI.
  Idea worth borrowing: the dispatch table keyed on (active overlay id, addr) with the activation hook on the file-read path.
- Not better: GPU backend (software raster + GL texture blit, no upscaling), overlays unwired, FMV unresolved, Linux/macOS only, one-person AI-generated, one game barely working.
- Config: TOML but ad hoc and undocumented; hex-as-string addresses mixed with int addresses. Not cleaner than a purpose-built schema.
- Fallback viability: low. For SO2 you would be writing the overlay switching, multi-disc, XA/STR, Windows build and SLZ loader yourself, while carrying a GPLv3 obligation.
  Use it as a reference to read (and a source of ideas), not as a base.

Not verified: no build or run was attempted; XA/MDEC depth and Windows build are inferred from source reading only.
