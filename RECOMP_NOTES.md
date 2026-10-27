# Recompilation tooling research (psxrecomp vs ps1-recomp)

Researched 2026-10-26 from the GitHub repos (README, docs, trees, selected source headers). Nothing was cloned or built.
Unverified items are marked **(unverified)**.

## 1. How each works

### RetroPortingToolKit/psxrecomp (https://github.com/RetroPortingToolKit/psxrecomp)
- **Status:** "beta" framework, ~370 stars, active (pushed 2026-10-25), maintainer mstan. License: PolyForm Noncommercial 1.0.0 (`LICENSE`).
- **Pipeline:** MIPS R3000A boot EXE (and a real or OpenBIOS BIOS) -> C (`generated/*_full.c`, `*_dispatch.c`) -> native exe linked with a shared PS1 hardware runtime (`runtime/src`: `cdrom.c`, `mdec.c`, `spu.c`, `gte.cpp`, GPU with GL/Vulkan/software renderers, SIO, memory card). Per-game repos pin psxrecomp as a submodule and commit `generated/` (`docs/GAME_PROJECT_SETUP.md`).
- **Kernel:** the BIOS is itself recompiled (LLE) as the oracle. An optional HLE layer sits on top (`docs/ARCHITECTURE.md`, `docs/HLE_EXECUTION.md`).
- **Three execution tiers** (`docs/EXECUTION_MODEL.md`):
  1. Static AOT code for the boot EXE and BIOS.
  2. Native overlays: the runtime records code DMA'd from disc into `overlay_captures.json` (bytes plus executed PCs). `tools/compile_overlays.py` recompiles these into DLL "shards" (gcc, or bundled TinyCC at runtime), cached by content hash in `<game>/<compiler>/<os-arch>/cg<N>_<hash>/<phys>_<crc>.dll`. Shards are guarded by the current bytes.
  3. A small MIPS interpreter handles dirty-RAM code that is not yet native, so the game stays correct.
- **AOT overlays without playing the game** (`docs/AOT_SHARDING.md`): `tools/aot_overlay_pipeline.py` takes a JSON profile (schema `psxrecomp AOT methods v1`) that declares disc hashes, image extraction methods, load addresses and pinned SHA-256s, then extracts, compiles, audits and stages shards.
  - Methods include `fixed_address_files`, `fixed_address_extents`, `indexed_lzss_members` and `aligned_lzss_banks`. Compressed containers are supported only through declared, parameterized transforms. "Arbitrary compression is not inferred." Our SLZ format would need a new method or a pre-decoded `prepared_files` entry.
  - `cross_image_call_roots = true` makes a `jal` into an overlay (or boot code) a discovery root.
  - Dynamic dispatch is handled in `docs/dynamic_handler_install.md` and `docs/OVERLAY_XREF.md` **(unverified)**.
- **Jump tables / indirect calls** (`docs/FUNCTION_DISCOVERY.md`, `recompiler/src/analysis_db.cpp`):
  - Strict resolver `resolve_exact_bounded_jump_table()`, plus `recover_jump_table()` (backward slice) labelled `proven`, `guarded` (sltiu bound) or `scanned` (hypothesis).
  - `jalr` through a static pointer slot is resolved as `call_ptr:immediate` or `call_ptr:static_slot`.
  - Anything unresolved falls to the interpreter (`discovery = "reachable"` fails closed). Seeds and `dispatch_root` seeds fix this.
- **Analyzer:** `psxrecomp-analyze <EXE> --symbols symbols.toml --seeds f --out dir --emit-tsv --emit-ghidra --emit-symbol-addrs`. Outputs `analysis.json`, `edges.json`, `indirect.json`, `refs.json`, `functions.tsv`, `symbol_addrs.txt` (splat format) and a Ghidra import script. It names BIOS A0/B0/C0 thunks and scores each function's confidence. It is static-only and reports its coverage gap.
- **GTE:** emitted inline against the runtime GTE. Optional PGXP gives sub-pixel precision and widescreen.
- **Substituting hand-written C for lifted code** (`docs/GUEST_IMPLEMENTATIONS.md`): `psxrecomp_select_guest_implementation()` in CMake binds an HLE `.c` source to generated symbols (e.g. `func_80000100`).
  - The tool renames the generated original to `<prefix>_func_...`; the HLE source defines the same symbol and signature.
  - HLE and LLE are build-time choices, and each family gets a `SNAPSHOT_TAG` for savestates. Generated code is never edited by hand.
  - Other hooks: `[[recompiler.patch]]` (exact-word instruction patch, guarded by expected word), host mod hooks at function entry, and `symbols.toml` entries mapped to `PSX_FN_*` macros (`docs/SYMBOLS.md`).
  - This is the natural place for our matched C. A matched function has the same `(args in a0-a3, v0 return)` contract as the lifted one **(unverified until tried)**.

### PS1Recomp/ps1-recomp (https://github.com/PS1Recomp/ps1-recomp)
- **Status:** ~22 stars, last commits 2026-09-21, no releases, GPLv3 (`LICENSE`). `ARCHITECTURE.md` says one title is fully validated (Rayman). Crash Bandicoot is experimental and blocked (`ISSUES.md`).
- **Pipeline:** `ps1Analyzer` (ELF/PS-EXE + disc reader, prologue scan, PsyQ signature DB of 3463 hashes for HLE stubs) -> `game.toml` -> `ps1Recomp` (1:1 per-instruction C++, `recompiled_out.cpp`, one giant file; N64Recomp-style `(rdram, ctx)` signature) -> `ps1Runtime` (SDL2/OpenGL, BIOS HLE, PsyQ HLE, GPU, SPU, CD, DMA, timers, GTE).
- **MDEC is stubbed** (FMV skipped). SPU is approximate. There is no interpreter fallback.
- **Overlays** are static only: `OverlaySection{name, romOffset, ramBase, size, functions[]}`, qualified names `overlay_<name>__func_<addr>` and a runtime `OverlayManager` that activates dispatch tables when RAM is written (`ps1Recomp/include/ps1recomp/overlay_handler.h`, `ps1Runtime/include/runtime/overlay_manager.h`). Overlays sharing an address are only handled as "conflicts".
- **Jump tables:** pattern match on LUI/ADDIU/ADDU/LW/JR (`jump_analysis.cpp`) emitted as a C++ `switch`.

## 2. Inputs each needs from a game project

### psxrecomp (`docs/config_schema.md`, `docs/GAME_PROJECT_SETUP.md`)
- `game.toml`:
  - `[game]`: `exe`, `load_address`, `entry_pc`, `text_size`, `stack_base`, `discs` (multi-disc) and `disc_serials`.
  - `[recompiler]`: `seeds` (text file of function addresses), `bios_thunks`, `bios_config`, `out_dir`, `discovery`, and `[[recompiler.patch]]`.
  - Also `[prepare_disc]` digests, `[netplay]` and `[runtime]`.
- `symbols.toml` with `[[func]] pc, name, emit, status, note`, synced to `psx_symbols.h` by `tools/sync_symbols.py`.
- Overlay profile JSON (`aot/overlays.json`, AOT methods v1): disc hashes, file aliases, per-image `load_addr`, size, SHA-256, and call roots.
- The user's own disc (Redump cue/bin). The scaffold `tools/new_project_layout/setup_project.ps1 -Disc game.cue` generates a first-pass seed list.
- No BIOS data is shipped. OpenBIOS is the default.

### ps1-recomp (`ARCHITECTURE.md`, `ps1Recomp/src/main.cpp`)
- A TOML with `[binary] path/entry_point`, `[disc] cue_path`, `[hle_overrides]` (addr -> stub name), `[missing_functions]` (addr -> name), an overlay table, and a function list from the analyzer. `ps1Interface` can import a Ghidra CSV.
- It expects the boot EXE only. Overlay code must be supplied as extracted raw sections with RAM base, and there is no decompression hook.

## 3. Fit for Star Ocean 2

**psxrecomp is the better fit.**
- It treats overlays as first-class: capture, AOT, a cache, and an interpreter fallback.
- Our boot EXE is only ~128 KB and most code is in SLZ overlays that reuse load addresses. ps1-recomp's static overlay model would need every overlay pre-extracted and has no runtime safety net.
- It ships FMV (MDEC), CD and SPU in the runtime (`mdec.c`, `cdrom.c`, `spu.c`). It has many shipped ports: Mega Man X4/5/6, Tomba 1/2, Ape Escape, Jersey Devil, Toshinden, Dragon Ball GT Final Bout, and community Xenogears (a similar overlay-heavy JRPG on multiple discs), Mega Man Legends and Gran Turismo 2.
- It supports multi-disc configs, and discs are verified against the player's own copy.
- It exports `symbol_addrs.txt` in splat format, which maps directly to our decomp symbols.
- Caveats:
  - The license is noncommercial (compatible with a public fan project, but not with selling a port).
  - Whether XA-ADPCM streaming and STR via MDEC are complete for SO2's usage is **unverified**; check `runtime/src/cdrom.c` and `mdec.c`, and Xenogears or Final Fantasy-style titles.
  - SLZ is not a built-in transform.
- ps1-recomp is GPLv3 and a useful reference, but it has one validated game, a stubbed MDEC, no overlay capture or JIT, and a single giant output file. I would not build on it.

## 4. Ordered checklist for our project

1. **Overlay inventory as machine data.** Promote `config/overlays.tsv` to a stable schema: overlay id, SO2.BIN TOC entry, compressed and decompressed size, `load_addr`, SHA-256 of decompressed bytes, entry points, and which overlays can coexist at the same address. Resolve the unplaced overlays first (AGENTS.md next step 1).
2. **SLZ as a declared transform.** Provide a reference decoder spec (parameters and a test vector) and either contribute an `slz_members` method to `tools/aot_overlay_pipeline.py`, or pre-decode overlays into `prepared_files` with pinned hashes.
3. **Bring up the boot EXE alone.** Run the `psxrecomp-analyze SCUS_944.21 --emit-symbol-addrs --emit-tsv` (cheap, static) and compare against our splat output for the 29 matched functions. This validates function boundaries and gives the baseline coverage gap, expected to be large (the Klonoa precedent: 45 KB EXE, 71% coverage).
4. **Seed and symbol files.** Convert our splat `symbol_addrs.txt` / matched function list into psxrecomp `seeds` (one address per line) and `symbols.toml` (`name`, `status = confirmed` for byte-matched, `guessed` otherwise). Also add `dispatch_root` seeds for overlay entry thunks and function-pointer tables found by our analysis.
5. **Scaffold a title repo** (`setup_project.ps1 -Disc`) with a multi-disc `game.toml` (`discs`, `disc_serials`) and an AOT overlay profile. Run extract-only first to check coverage.
6. **First boot milestone** with the interpreter and overlay capture enabled to find what static analysis misses (jalr through overlay tables); feed the captures back as seeds.
7. **Substitute matched C.** Move each matched function into a `.c` file with the original symbol name and signature, then bind it with `psxrecomp_select_guest_implementation()` in an LLE-vs-HLE build. Keep the LLE build as the oracle, and diff HLE output against LLE for each matched function in tests. Keep any matched code that touches registers or the stack outside its calling convention in LLE until verified.
8. **Decide the licensing and legal posture** (PolyForm Noncommercial vs. our LICENSE) and keep generated C out of the public repo, as the project README already promises.
9. **Verify the media path:** check XA, STR and CD-DA handling in `runtime/src/cdrom.c` and `mdec.c` and test SO2 FMV and voice early.

## Risks and unknowns
- SLZ and the encrypted TOC need a bespoke extraction path.
- Self-modifying or runtime-relocated overlays (hash-guarded shards can miss).
- A GCC 2.7.2-compiled function matching the lifted function's register contract is likely but untested. Matched functions must keep the same externally visible behaviour, including timing and GTE use.
- Savestate tags and snapshots for HLE'd functions.
- psxrecomp is beta, with a noncommercial license and fast-changing APIs (pin a submodule SHA).
- XA and STR completeness is unverified, and nothing was built or run, so these findings come from documentation only.

## Deep dive: psxrecomp (2026-10-26)

Source read from a shallow clone at `tools/ref/psxrecomp` (HEAD f42d2c0, 2026-10-25). Paths below are relative to that repo. Nothing was built or run. Unverified items are marked **(unverified)**. This corrects the first-pass sections above where it conflicts.

### 1. Pipeline, CLI, configs
- **Repo layout:** the framework ships no game dirs. Each game is its own repo that pins psxrecomp as a submodule (`README.md:61-86`, `docs/GAME_PROJECT_SETUP.md`). Only `generated/OpenBIOS_*.c` is committed here.
- **Windows first boot** (`docs/GETTING_STARTED.md:10-60`):
  - Needs Git, Python 3 (3.11+ for the AOT tool), and the `cmake-clang-v1` toolchain ZIP (compiler, CMake, Ninja) from RetroPorting-Toolchains.
  - Alternative is MSYS2 MinGW-w64 or MSVC (`docs/BUILDING.md:19-21`). Python is also needed at runtime for the overlay compiler.
  - SDL3 is fetched automatically. OpenBIOS is bundled, so no BIOS dump is needed.
  - Scaffold: `powershell -File tools\new_project_layout\setup_project.ps1 -Disc game.cue -Dir <dir> -Name .. -Yes -NoNetplay -NoGithub -Generate -EnableBuild`.
- **CLI** (`psxrecomp_cli.py`): `verify-disc | generate | rebuild | ensure-toolchain | ensure-emitters | analyze | pgo-train`.
  - `generate` runs the emitters, prepares the disc, writes `generated/`, then runs the AOT `static` step if the profile declares `static_output` (`docs/AOT_SHARDING.md:255-270`).
  - `analyze --config game.toml --emit-symbol-addrs --emit-ghidra`.
  - Offline overlay compile: `tools/aot_overlay_pipeline.py {extract|static|release} --profile aot/overlays.json --game-toml game.toml --recompiler <psxrecomp-game>`. `compile_overlays.py --captures` is the lower level.
  - Recompiler tests need no disc (`docs/TESTING.md`).
- **`game.toml`:**
  - `[game]`: `name, id, exe, load_address, entry_pc, text_size, stack_base, discs[], disc_serials[]` (`docs/config_schema.md:114-130`).
  - `[recompiler]`: `seeds, bios_thunks, out_dir, strict, discovery("whole-image"|"reachable"), [[patch]]{id,address,expected,replacement}` (`:154-200`).
  - `[prepare_disc]` (digests), `[runtime]` (`overlay_cache`, `overlay_backend`, `disc_speed`, `bios_hle`), `[video]`, `[widescreen]`.
- **Seeds file** (`recompiler/src/main_psx.cpp:1001-1031`): one hex address per line. Prefixes are `interior`, `dispatch_root`, `call_root`. `tools/new_project_layout/probe_disc.py:218-250` writes a first pass from boot-EXE `jal` targets.
- **`symbols.toml`** (`tools/new_project_layout/templates/symbols.toml.in`): `[[func]] pc, name, emit, status, note`. `tools/sync_symbols.py` turns it into `PSX_FN_*` macros.
- **Minimal 2-disc `game.toml`** (modelled on the Xenogears port and the template):

```toml
[game]
name = "Star Ocean 2 (USA)"
id = "SCUS-94421"            # TODO confirm from SYSTEM.CNF
exe = "game/SCUS_944.21"
load_address = "0x80010000"
entry_pc = "0x80010650"      # TODO verify against the PS-X header
text_size = "0x1F800"        # exe image 129024 B; confirm header size
stack_base = "0x801FFFF0"
discs = ["disc/SO2 (Disc 1).cue", "disc/SO2 (Disc 2).cue"]
disc_serials = ["SCUS-94421", "SCUS-94422"]   # TODO confirm both serials
[recompiler]
seeds = "seeds/scus_94421_seeds.txt"
out_dir = "generated"
strict = true
discovery = "whole-image"
[runtime]
overlay_cache = false        # Xenogears pattern: AOT only, a miss must fail
disc_speed = "1x"
```

  The overlay profile `aot/overlays.json` (schema `psxrecomp AOT methods v1`) carries `game_id, disc_hashes{sha256} or disc_set[], expected_records, images[{method, ...}], static_output, checks[]`. A real example is Tomba2Recomp `aot/usa.json` (fetched with `gh api`). Xenogears keeps its own TOML identity manifest, see section 5.

### 2. Overlay handling
- **Runtime capture:** `runtime/src/overlay_capture.c`.
  - Triggered by CD DMA channel 3 completions (`runtime/include/overlay_capture.h:27-30`). The JSON content, however, is assembled from the **dirty-page bitmap** (CPU-written pages, `overlay_capture.c:323-360`).
  - So SLZ-decompressed code written by the CPU is captured as post-decompression bytes. Schema `psxrecomp overlay capture v3`: `load_addr, size, bytes_b64, executed_pcs, dispatch_entry_pcs, ...` (`:413-479`).
  - The capture window is `overlay_cache_window_contains()` (`runtime/include/dirty_ram_interp.h:102-205`): everything at or above the end of the boot EXE text, plus kernel RAM. The floor is a runtime value derived from the exe size, so our 0x8002f810 "main program A" falls inside it.
- **Residency detection is by content hash, not by address or signature:**
  - Each native function is a `Candidate` with a CRC32 over its code ranges. Dispatch rehashes live RAM (`runtime/src/overlay_loader.c:582-590`) and runs native only on a match.
  - Rehashing is gated by a per-page write-generation counter (`:4001-4040`).
  - Overwriting the code invalidates the candidate. Self-modified or per-load-relocated bytes fall to the interpreter.
- **Several overlays at one address are explicitly supported:** "different overlays can reuse the same RAM address ... each PC maps to a chain of candidates; dispatch picks the one whose code hash matches live RAM" (`overlay_loader.c:60-66`). `PSX_OVERLAY_CANDIDATE_CAP` = 32768 (`overlay_api.h:138`). Identical copies dedupe by `(base, bytes)` (`aot_overlay_pipeline.py:425-440`).
- **DLL cache:** `<cache>/gcc/<os-arch>/cg<codegen-ver>_<hash>_gc.._f../<phys>_<crc>.dll` plus a `.ranges` manifest (`overlay_loader.c:1952-2147`, `docs/OVERLAY_CACHE_V2.md`). The loader rejects ABI-mismatched DLLs (ABI is at v22+). Backend is gcc or bundled tcc (`overlay_backend.c:47-60`).
- **Self-relocating overlays:** handled only offline, by declaring a relocation method (`tagged_relocated_files`, `counted_relocated_members` with explicit placements; `docs/AOT_SHARDING.md:69-71`, `docs/COUNTED_MIPS_RELOCATIONS.md`). Unplaced relocations run interpreted.
- **Interpreter cost:** `dirty_ram_interp.h:119` quotes about 300 ns/insn. It is block-based and strict (an unknown opcode aborts fatally, `docs/dynamic_handler_install.md:110`).
- **Offline extraction methods that exist** (`docs/AOT_SHARDING.md:62-80`, `aot_overlay_pipeline.py:393-500`): `fixed_address_files, fixed_address_extents, psx_exe, packed_sector_members, sector_extent_members, aligned_lzss_banks, indexed_lzss_members, tagged_relocated_files, counted_relocated_members`. Plus `prepared_files` (declared decoders; registry at `tools/prepared_files.py:51`, only `bpe_container` today). "Arbitrary compression is not inferred."
- **Adding SLZ:**
  - (a) A new method branch in `spec_sources()` (`aot_overlay_pipeline.py:444`), modelled on `pack_sources()` (`:322-366`). It yields `dict(name, base, body, spec, source_offset, source_file, aliases)` per member, pinned by `decoded_sha256`, with an inventory of members and excluded members that carry reasons.
  - (b) Registering an `slz` decoder in `prepared_files.METHODS` fits poorly: one output per source offset, and it cannot decode the TOC.
  - The existing `indexed_lzss_members` does not fit: its container is `{count, offset/size/stored}` with MSB-first bit LZSS (`indexed_lzss_pack.py:1-25`), while SLZ has LSB-first flag bytes, 2-byte refs and a 16-byte header.
  - SO2.BIN also needs the TOC decrypt. Port `analysis/toc.py` and `analysis/slz.py` into the new method. There is no upstream plugin hook, so this means a fork or a PR.
  - Alternative that avoids touching their tools: write our own `overlay_captures.json` (schema v3) from our extractors and feed it to `compile_overlays.py --captures` (`docs/COMPILING_OVERLAYS.md:1-6`: disc-derived records are accepted). It needs per-overlay `dispatch_entry_pcs`. **(unverified)**

### 3. CD / XA / MDEC / SPU / GTE / Psy-Q
- **CD:** the controller is emulated at register level with timing (`runtime/src/cdrom.c`, 3820 lines). Commands: GetStat, SetLoc, ReadN, ReadS, Play, SeekL/P, Pause, Stop, Init, Mute, Demute, SetFilter, SetMode, GetlocL/P, GetTN, GetTD, GetID, ReadTOC, Test (`:2129-2505`).
  - Reads are by absolute MSF/LBA, so a libcd-style loader works unchanged. The BIOS kernel is LLE (recompiled) and the Psy-Q libraries inside our exe are simply lifted.
  - There is no libcd HLE (no psyq/libcd references in `runtime/src`). The optional `bios_hle` only covers kernel A0/B0/C0 services (`runtime/include/bios_hle.h`). This is the safe choice for our custom loader.
  - Disc swap is a faithful lid cycle via a runtime menu (`docs/RUNTIME_DISC_MENU.md`, `runtime/include/psx_disc_tray.h`).
- **XA:** 4-bit ADPCM decode (mono and stereo), 37.8/18.9 kHz, resample to 44.1 kHz, the CD volume matrix, file/channel filter and EOF handling are all present (`cdrom.c:1213-1480`). **8-bit XA (depth != 0) is dropped** (`:1401`). CD-DA multi-track works (`:299`, `iso_cdda_*`). Audio goes to the SPU CD input (`spu_cd_audio_push`). Whether SO2's XA is 4-bit is **(unverified)**.
- **MDEC / STR:** command, quant, scale, IDCT, 15/24 bpp output, 4/8 bpp texture path, DMA0/1, SSE2/NEON (`mdec.c`, 1247 lines). Shipped ports use it for FMV. No stub markers were found. DMA channels 0-6 are handled; any other channel aborts (`dma.c:1128`).
- **SPU:** ADSR, Gaussian interpolation, reverb, noise, sweep, CD input (header of `spu.c`). **GTE:** `gte.cpp`, 2871 lines, including the UNR divide and PGXP hooks.
- Hardware code was written against Beetle/Mednafen; see the licence section.

### 4. Guest-function substitution (two mechanisms)
- **ABI of generated code:** `void func_XXXXXXXX(CPUState* cpu)` (`generated/OpenBIOS_full.c:37`). `CPUState` holds `gpr[32], pc, hi, lo, cop0, gte_data, gte_ctrl`, memory accessors and cycle bookkeeping (`runtime/include/cpu_state.h:23-67`). Args are `gpr[4..7]` plus the guest stack; return value is `gpr[2]`. `cpu->pc != 0` on entry means an interior resume (interrupt re-entry), see the `switch(_cont)` at the top of each function.
- **(A) Build-time `psxrecomp_select_guest_implementation()`** (`runtime/guest_implementation.cmake`, `docs/GUEST_IMPLEMENTATIONS.md`):
  - The generated source must contain exactly one `void <symbol>(CPUState*) {` definition per symbol (regex at `:46`). The tool renames the original with a compile definition (`REFERENCE_PREFIX`), and your `HLE_SOURCES` define the same symbol. A nonzero `SNAPSHOT_TAG` is required.
  - It works on **static C shards only** (boot EXE, `overlays_static.c`), not DLL overlays. There is no count limit.
- **(B) Runtime `func_override_add[_guarded]()`** (`runtime/include/func_override.h`):
  - Signature `int fn(CPUState*)`. Return 1 means handled, as if the guest had run `jr $ra`. Return 0 means the original runs.
  - Hooked at the top of `psx_dispatch_impl`, before every game backend, and in the interpreter's JAL/JALR tiers.
  - Requires a cycle `credit` (>=0, or `FO_CREDIT_SELF`). `_guarded` takes 1-4 expected words, so it works at overlay addresses.
  - **Limits:** `FO_MAX_OVERRIDES = 128` (`:119`). It is **call-site keyed**: `j`/`jr` tail transfers into the function bypass it (`:20-26`). Returned pointers must be guest addresses. Mutable state must live in guest RAM (rollback). `func_override_call_original()` gives a wrap.
- **Our matched C:** GCC 2.7.2 compiles it to the same bytes psxrecomp lifts, so it adds no information to the lifter and there is nothing to feed in. Its value is as a regression oracle (compare a substitute against it on test inputs). A host-C rewrite needs guest-address accessors (host pointers are 64-bit), so matched C cannot be dropped in unchanged.
- **Recommendation:** lift everything (LLE), keep the matched C as documentation and an oracle, and use (B) only for a few hot or enhancement functions. Do not plan to substitute hundreds.

### 5. Closest shipped analog: Xenogears (OpokXeno/xenogears-recomp)
- A 2-disc JRPG with overlays at one fixed address (24 images, several at `0x8006faf0`), LZSS-stored on disc, with STR and XA. The same overlay bytes sit at different sectors on disc 2.
- **Repo contents:**
  - `game.toml` (11 KB): `disc_serials`, `[runtime] overlay_cache = false`, `disc_speed = "1x"` (their comment: "instant" hangs), `[[runtime.warm_cd_routes]]`.
  - `annotations/overlays/disc1-images.toml` and `disc2-images.toml`: per image `source_sector, stored_size, stored_sha256, storage="lzss", sha256, crc32, size, load_address`.
  - `annotations/overlays/aot-dispatch-roots.toml`: identity-qualified `dispatch_entry_pcs` per image.
  - A hand-annotated symbol CSV per overlay, and `CMakeLists.txt` (55 KB).
- **Hacks:** `bios_hle = true`, `mod_function_entry_funcs` (function-entry hooks), and a large native renderer. The widescreen cull sites are presentation-only; we skip those.
- **Takeaway:** copy the hash-pinned image manifest approach, the per-disc sector tables, and `overlay_cache = false`.
- Other analogs: Tomba2Recomp (28 raw images plus MAIN.EXE, multi-track), Ape Escape (packed sector table).

### 6. Quality, issues, licence
- **Tests:** 366 files in `runtime/tests`, 53 in `recompiler/tests`, 44 in `tools/tests`, registered through `ctest`. Only 3 GitHub workflows exist, all release or nightly packaging (`.github/workflows`); **none runs the tests on PRs**.
- **Open issues** (25, via `gh api`): #387 `setup_project.ps1` fails with "The Try statement is missing its Catch or Finally block" (a Windows setup bug, relevant to us), #721 game start clears RAM 0x00-0x0F, #365 dirty RAM growth at startup (Parasite Eve), #248 low FPS on weak hardware, plus renderer and netplay items (#751, #750, #719, #636, #475). **#443 (GPL provenance)** is the notable one.
- **Licence:** PolyForm Noncommercial 1.0.0. Per issue #443, runtime files describe themselves as ported from Beetle/Mednafen (GPL) and DuckStation (189/28/18 mentions). The maintainers have not published a resolution, so the effective licence is **unsettled**.
- **For our public repo:** reference it as an external tool (`tools/ref` is gitignored, nothing vendored) and do not publish generated C. If we fork `aot_overlay_pipeline.py` to add SLZ, that fork carries the noncommercial terms, so keep it outside the repo or contribute it upstream.

### 7. Gaps for Star Ocean 2 (severity)
| # | Gap | Severity |
|---|---|---|
| 1 | No SLZ / SO2.BIN encrypted-TOC extraction method (new `spec_sources` branch, or the captures workaround) | **High**, but small (~100 lines; we already have the decoders) |
| 2 | The boot EXE is only 128 KB, so most code is overlay code ("main program A" 283 KB at `0x8002f810`, battle and field overlays at shared addresses). Static analysis of the EXE alone covers little; AOT depends on gap 1 and on load addresses from `config/overlays.tsv` | **High** |
| 3 | Many same-address modules (0x8007e000, 0x801ee000): supported by hash-chained candidates, but variant count and compile time are unmeasured | Medium |
| 4 | Self-relocating or self-modifying overlays run interpreted until declared; we do not yet know which SO2 overlays relocate | Medium **(unverified)** |
| 5 | XA depth (8-bit unsupported) and STR coverage unverified for SO2 | Medium to low |
| 6 | Disc swap works (lid cycle); story-state handoff across discs untested | Low |
| 7 | Substituting matched C: 128-override cap, call-site-only hook, static-shard-only (A) | Low (we do not need it) |
| 8 | Licence ambiguity (#443, noncommercial) for redistributing a port | Medium (policy) |
| 9 | Windows setup bug #387, no PR CI upstream, fast-moving API (pin a commit) | Low to medium |

**Suggested order:** (1) run `analyze` on `SCUS_944.21` and compare against our 29 matched functions; (2) write the SLZ/TOC AOT method and an inventory against `config/overlays.tsv`; (3) first boot with `overlay_cache = true` on a dev box to collect captures of what static analysis misses.

## First analyze run (2026-10-26)
Run inside Windows Sandbox (no network, read-only inputs) via `recomp/sandbox/`; toolchain cmake-clang-v1 v1.0.14
(CMake 3.31.12, Ninja 1.13.2, Clang 22.1.8), `psxrecomp-analyze` built from the pinned clone with `-DPSXRECOMP_ENABLE_CHD=OFF`.
Build and both analyze runs completed in about 20 s after the toolchain extracted. Input: boot exe `SCUS_944.21` (129,024 bytes).

| | `--seeds config/seeds.txt` | `--exact` (reachability only) |
|---|---|---|
| functions | 570 | 140 |
| reachable / orphans | 261 / 309 | 140 / 0 |
| jump tables resolved (guarded) | 15 (273 targets) | 8 |
| unresolved indirect sites | 119 (mostly `jalr $v0`, function pointers) | 16 |
| BIOS dispatch sites identified | 31 (A0/B0/C0 calls) | - |

- Our 545 boot-exe seeds vs analyzer (seeded): 486 found, 59 of ours not found, 84 analyzer-only. All 140 `--exact` functions are in our seed list.
- None of the 29 matched functions is in the boot exe (they are in the main overlay at 0x8002F810+), so they could not be cross-checked yet. Next: run the analyzer on the decompressed main overlay.
- The 119 unresolved `jalr` sites are function-pointer calls; they will need seeds/overrides (overlay entry points, dispatch tables).
- Not yet done: overlay analysis, any recompile/build of generated code, runtime test. The 13 remaining `TODO(verify)` values still need those.

## Second analyze run: main programs A/B/C (2026-10-26)
Same sandbox harness; images are SO2.BIN entries 1 (A), 2129 (B), 2576 (C) wrapped in a synthetic PS-X EXE header (load 0x8002F810, entry = first seed, an assumption).

| image | seeded functions | orphans | jump tables | unresolved indirect | `--exact` functions |
|---|---|---|---|---|---|
| A (entry 1) | 830 | 828 | 26 | 84 | 2 |
| B (entry 2129) | 1001 | 736 | 28 | 436 | 2 |
| C (entry 2576) | 873 | 868 | 67 | 136 | 5 |

- **Matched functions vs analyzer (A, seeded):** 28 of our 29 matched functions are analyzer function starts with exactly the matched size. The one difference is `func_800344D0`: the analyzer cuts it into a 4-byte function at 0x800344D0 plus a 96-byte function at 0x800344D4 (a seed or jal target at +4); our matched size is 100. Not yet resolved which view is right.
- **Seeds:** 751 of our 784 A seeds are analyzer function starts; the analyzer adds 79 starts we did not have.
- **`--exact` finds almost nothing** (2 to 5 functions): reachability from a single entry point cannot follow the overlay's indirect dispatch, so the program entries and tables must come from seeds. The `entry = first seed` assumption means this mode says little about real reachability.
- Takeaway: seeded analysis of the overlays works and agrees with the decomp; recompiled overlays will depend on good seeds and on resolving the 84 to 436 unresolved indirect calls per program.

## Indirect call resolution (2026-10-26)
Tool: `analysis/indirect.py` (static, per-site forward symbolic evaluation; reads the local gitignored exe/`.dec` files and the sandbox `summary.txt` + `functions.tsv`) -> `config/indirect_targets.tsv` (site, program, kind, targets, confidence, note; addresses only). `analysis/make_seeds.py` folds the call-type targets into `config/seeds.txt` (+22 boot) and `config/seeds_overlays.txt` (A +29, plus B/C), idempotent. Of the 775 unresolved sites (boot 119, A 84, B 436, C 136): targets found for 661, classified with no code target 70, still unresolved 44.

| program | sites | with targets | no target (bios / data decoded as code / null / kernel) | unresolved |
|---|---|---|---|---|
| boot | 119 | 94 | 7 | 18 |
| A | 84 | 81 | 1 | 2 |
| B | 436 | 424 | 1 | 11 |
| C | 136 | 62 | 61 | 13 |

Kinds (sites): **C++ virtual call** (`lw v0,4(obj); lh a0,d(v0); lw v0,f(v0); jalr`, gcc 2.x `{delta,index,pfn}` vtables) A 81, B 419, C 69, boot 4; resolved to the union of slot `+f` over every vtable address stored at `+4` in the program (over-approximation, confidence `low`, but every target is a real function pointer, so seeding them is sound). **Global struct of callbacks** (boot, `lw v0,slot; lw v0,off(v0)`) 30; **setter-filled static slot** (boot, 60: const stores, or the constant a caller passes to the setter) medium; **indexed function-pointer table** 2 (boot 1, B 1, low: no `sltiu` guard); **jump tables the analyzer's guard check missed** (`jr`, intra-function; B 3, C 1; not seeded); **BIOS dispatch** `jalr` to 0xA0/0xB0/0xC0 with the function number in $t1 (boot 5, A/B/C 1 each); **data decoded as code** (C 60: the analyzer's 29 KB pseudo-function `func_800715F4` at the tail of C is data, tagged syscall+gte); null / kernel-RAM constant 2. Overlay entry via the loader is not a `jalr` pattern here: the overlay loads go through `jal` to the loader, so no site was an overlay entry. Not resolved: argument-register callbacks whose caller passes a non-constant (boot 1), `lw` from a global that nothing in the image assigns (zero/bss, boot 7, A 2, B 3, C 2: set from another overlay or by the BIOS), table bases in RAM (B 3, boot 1), vtables whose constructor lies in an overlay we did not load (C 10).
- **func_800344D0 (100 bytes, A) is one function; our seed list is right.** Nothing in A, B, C or any other decompressed module references 0x800344D4 (no jal, branch or pointer); the only reference to 0x800344D0 is `jal` at 0x8006F490, and 0x800344D0 is in `config/seeds_overlays.txt`. The analyzer's split is its own heuristic: `find_preprologue_setup_start` (function_analysis.cpp) backs a found `addiu sp,sp,-0x18` (0x800344DC) up over `lui`/`lw` global-load setup only, and the leading `move $a1,$a0` is not allowed in that window, so the function start becomes 0x800344D4 and our 0x800344D0 seed is left as a 4-byte stub. Consequence for the recomp: treat 0x800344D4 as a bogus entry (override it, or merge it into 0x800344D0); the same pattern (a 1-2 instruction non-terminating stub) shows up for boot 1, A 11, B 5, C 6 analyzer functions (not all real, several are data), listed by size <= 16 with no jump/branch.

## 2026-10-26 Matched functions vs symbols and seeds
Check: `python -I analysis/check_matches.py [<sandbox>/_out_run2/A/seeded/functions.tsv]` (no game data; exit 1 on any problem).
- All 235 rows of `config/matches.tsv`: name = `func_<addr>`, 4-aligned, inside e1c0 (0x8002F810 + 283604), no two overlap. Before: `symbols.toml` had only 29 of them as `confirmed`, and 18 matched starts (e.g. 0x8004ADD4, 0x8006D324, 0x800733A8, 0x80073D44) were not e1c0 seeds (no `jal` caller and no `addiu sp` prologue: leaves reached by pointer).
- `analysis/make_seeds.py` now writes every match as `confirmed` with `size = <bytes>` (an extra key: psxrecomp `load_symbols`, analysis_db.cpp, reads only pc/name/status/note/emit, so it is ignored there) and adds every matched start to the e1c0 seeds. Result: 439 symbols (235 confirmed + 203 remaining splat labels + BootEntry); e1c0 seeds 813 -> 818. `config/seeds.txt` regenerates byte-identical. From a worktree run it with `SO2_DATA=<main checkout>` (new: the gitignored inputs are read from there).
- **Prologue seeds inside matched bodies are false entries; 13 dropped:** 800344dc 80036bc4 800378d0 8003b190 8003b1e8 8003b258 8003b2a8 8003b338 8003b3a8 8003c37c 8004ad54 8006d3f8 8006dff0. gcc 2.7.2 scheduled the frame `addiu sp,sp,-8` into the body (0x800378CC: `lh v0,0x7c4(a1)`, then the `addiu` at 0x800378D0; 0x8003B284: the `addiu` at 0x8003B2A8 after the multiply setup). The byte match proves one function per span, and the filter keeps any `jal` target (none of the 13 is one).
- Analyzer (sandbox run 2, program A, seeded, run with the old seeds): 232/235 matched functions are analyzer functions of equal size. The 3 others follow this pattern: `func_800344D0` (analyzer 4 B + 0x800344D4, see 'Indirect call resolution'), `func_800378CC` (4 B + 0x800378D0), `func_8003B284` (36 B + 0x8003B2A8). The last two splits came from our old seeds; a rerun with the new seeds should leave only 0x800344D4 (the analyzer's own pre-prologue heuristic). Not rerun (needs the sandbox). **Wrong:** runs 1-3 never read our seeds; all three splits are the analyzer's own (see '2026-10-26 Analyzer seeds were never read').
- Caveat: symbols are keyed by pc only. Programs B, C and e3107c0 also load at 0x8002F810, so pass `symbols.toml` only when analysing program A.

## 2026-10-26 game.toml values
Settled from the psxrecomp source (f42d2c0, read only, nothing run) and the local discs:
- `exe = "disc/SCUS_944.21"` with `[prepare_disc] out_dir = "disc"`, `boot_exe = "SCUS_944.21"`: prepare_disc.py writes extracted files to `<out_dir>/<name>` and defaults `out_dir` to `prepared_disc` (load_config); probe_disc.py renders `exe` as `<out_dir>/<boot_exe>` with default `disc`. `bin_name`/`cue_name` are set because prepare_disc otherwise takes `Path([game] disc).name`, which is the `.cue` name.
- `disc` + `discs`: the C++ loader prefers `discs` (config_loader.cpp); the Python tools (prepare_disc.py, aot_overlay_pipeline.py `game_disc`) read only `disc`, so `disc` names disc 1. The file names are those of the local single-track MODE2/2352 dumps.
- `seeds`: main_psx.cpp parses each non-`#` line with `strtoul(..., 16)`; our bare-hex `config/seeds.txt` fits. `strict = true` is the loader default ("currently always true"). `discovery = "whole-image"`: `reachable` follows `jal` only and leaves `jalr` targets to the interpreter; `--exact` found 140 of 570 boot functions.
- `overlay_cache = true`: probe_disc.py's default, and GAME_PROJECT_SETUP.md says the packager refuses without it; it records the overlays the runtime interprets.
- **No XA, no CD-DA:** a census of every sector's mode 2 subheader finds no XA audio (submode 0x04) and no realtime video (0x02) on either disc (disc 1: 236155 data, 216 Form 2, 160 other); both cues are single-track. STR movies are ordinary data entries in SO2.BIN.
- Still `TODO(verify)` (4): `disc_speed`, `bios_hle` (runtime behaviour, need a boot test), `[video]/[widescreen]` (a look decision); in the AOT profile `expected_records` (it counts the generic extractor's recipes, known only after one `extract` run).

## 2026-10-26 SLZ overlays into the AOT pipeline
**Decision: neither the captures file nor an `slz_members` image method, but a `prepared_files` decoder.** psxrecomp's `tools/prepared_files.py` already lets a
profile derive virtual files from disc bytes through a registered decoder (`METHODS`, today only `bpe_container`). Each output is pinned by `consumed`,
`source_sha256`, `size` and `sha256`, and is then an ordinary file for `fixed_address_files`. One SLZ chunk is exactly such a transform.

| | captures file (`compile_overlays.py --captures`) | `slz_members` method (fork of `aot_overlay_pipeline.py`) | **`prepared_files` method `so2_slz` (chosen)** |
|---|---|---|---|
| upstream change | none | new `spec_sources` branch + TOC decryption + pinning, in a 1234-line file | 2 lines in `prepared_files.py`: `import slz` and `'so2_slz': (slz.prepared, ())` |
| gates kept | none of the pipeline's (no disc hash, image hash, inventory, audit or staging); records are hand-built | all, if written right | all: disc hash, per-chunk input and output hashes, inventory, audit, staging |
| game bytes | a JSON holding the decompressed code (`bytes_b64`) to keep privately | none stored | none stored: rebuilt from the user's disc on each run; the profile holds offsets, sizes, hashes |
| TOC | needed | needed (port `toc.py`) | not needed: each chunk's byte offset in SO2.BIN is pinned and its bytes are hash-checked |

Implementation:
- `analysis/slz.py` `prepared(data, spec)`: the decoder contract (`data` starts at the chunk header; returns `(output, consumed = 16 + csize)`). Strict: bad magic, a broken stream, output size != header `dsize`, or a stream that reads past the declared payload raise `ValueError` (the one exception `prepare()` turns into `PreparedError`). `dec`/`dec_all` behave as before.
- `analysis/aot_profile.py` writes `recomp/aot/overlays.json`: 89 unique (load_addr, bytes) chunks as `prepared_files` (`SLZ/E<entry>C<chunk>`, `source {file: SO2.BIN, file_offset}`), 52 identical copies listed in `x_aliases`, 14 `fixed_address_files` images (one per load address, `allow_missing: true` because the generic extractor cannot see compressed code), plus `psx_exe` for the boot exe. It decodes every entry again from the local disc and checks it the way `prepare()` does, and against `config/overlays.json`; it also self-tests `prepared` on the 18 synthetic vectors and 4 malformed inputs. Output: `89 prepared SLZ chunks at 14 load addresses, 52 aliases; all re-decoded and hash-checked`. `python -I tests/slz/run_tests.py`: `OK 0`.
- **Disc 1 only** (`disc_hashes`, not `disc_set`): disc 2's ISO directory has no SO2.BIN (ANALYSIS.md section 1), so `Disc.read('SO2.BIN')` would fail there. All 141 code chunks are byte-identical on disc 2, and shards are keyed and guarded by bytes, so shards built from disc 1 serve both discs.
- This supersedes the `slz_members` profile of `scripts/make_psxrecomp_overlays.py`. Do not rerun that script: it would overwrite the new profile (editing `scripts/` was outside this task).

Open: nothing of psxrecomp was run (project rule: no psxrecomp code on the host; the sandbox harness covers only the analyzer). To use it in the private title repo: put `analysis/slz.py` next to `psxrecomp/tools/prepared_files.py` (or on its import path), add the two lines, then run `aot_overlay_pipeline.py extract --profile aot/overlays.json --game-toml game.toml --disc "<Disc 1>.cue" ...`. Unverified: `expected_records`; whether the images at medium-confidence load addresses (0x801F9800 slot family, e3107c0) compile cleanly; per-overlay entry points (none declared; the pipeline finds prologues, `jal` roots and frameless leaves itself; `config/seeds_overlays.txt` can become `entries` once a run shows what it misses).

## 2026-10-26 Analyzer seeds were never read
**Runs 1 and 2 (and run 3, same harness) ignored almost all of our seeds.** `psxrecomp-analyze --seeds` parses each token with `std::stoul(tok, &idx, 0)` unless it starts with `0x` (`load_seed_file`/`parse_addr`, main_analyze.cpp in f42d2c0). Bare hex with a letter is rejected (`idx != size`), and all-digit bare hex is read as **decimal** (`80010000` -> 0x04C4B400, outside the image). Evidence: the run-3 logs print `with 136 / 157 / 231 / 167 seed(s)` for boot/A/B/C, exactly the number of all-digit lines in the staged lists (`grep -cE '^[0-9]+$'`: 136, 157, 231, 167). Function lists of run 3 (new seeds, bare hex) equal run 2 (old seeds) address for address and size for size. So every earlier "seeded" count is really the analyzer's own scan (`jr ra` scan + `jal` targets), and "486 of 545 seeds found" meant "the analyzer finds these by itself".
Fix (harness only; `config/seeds*.txt` stay bare hex, which `main_psx.cpp` reads with `strtoul(...,16)`): `make_wsb.ps1` writes the staged `seeds.txt` with a `0x` prefix, and `stage_overlays.py` writes `A/B/C.seeds.txt` the same way. `make_wsb.ps1` also takes `-OutName` and reads the gitignored inputs from `$env:SO2_DATA` (main checkout), so it runs from a worktree.

**Bad seeds, once read, did damage (run 4, 0x prefix, seeds before the fix below).** Logs: `with 567 / 818 / 965 / 799 seed(s)`; every seed became a function. Boot: 630 functions, but `reachable` fell from 261 to 214 because seeds split real functions.
The 60 boot seeds that the no-seed analyzer does not have as starts (earlier "59 not found" + one of the 22 indirect seeds): 59 lie inside an analyzer function, 1 in uncovered bytes. 56 of them have no reference (no `jal` in the exe or any decompressed module, no pointer word, not in `indirect_targets.tsv`) and are `addiu sp,sp,-N` prologue seeds after gcc's pre-prologue code: `lui;lw` (36), `lui;lbu` (10), `lui;lw;lui;lw` (4), `lui;lw;lui;lhu` (1), or an early-out test with a branch (5). Same pattern as the 13 A seeds dropped earlier. The other 4 are real: 0x800100C4, 0x8001FD80, 0x80026A94 (`jal` targets), 0x80014648 (indirect target).

**Seed fix (`analysis/make_seeds.py` `pre_prologue`):** an `addiu sp` seed that is not a `jal` target moves back to the instruction after the previous `jr ra` + delay slot (skipping alignment nops) when that is at most 12 instructions up and nothing between is a `j`/`jal`/`jr`/`jalr`/`syscall`/`break`. Result: boot 567 -> 527 seeds, A 818 -> 799, B 965 -> 925, C 799 -> 750; overlays 5015 seeds in total. Against the no-seed analyzer starts, the seeds that are not starts fall from 60/20/53/53 to 8/0/12/5. The new non-start seeds were checked by hand: 0x80013508, 0x80014474, 0x800274CC, 0x8001FA80 (boot) and 0x8007A634 (B) begin right after a `jr ra; nop` (+ pad) in bytes no analyzer function covers.

**Run 5 (fixed seeds, `0x` prefix; `recomp/sandbox/_out_run5`, status OK).** Logs: `with 527 / 799 / 925 / 750 seed(s)`; every seed is an analyzer function start.

| image | run 2/3 (seeds unread) | run 4 (bad seeds read) | **run 5** | run 5 vs run 3 |
|---|---|---|---|---|
| boot | 570 fn, 261 reachable | 630, 214 | **578, 262** | +8 starts, 0 lost, 3 resized |
| A | 830 | 850 | **830** | identical |
| B | 1001, 265 reachable | 1054, 233 | **1013, 265** | +12, 0 lost, 11 resized |
| C | 873 | 926 | **878** | +5, 0 lost, 5 resized |

Jump tables (15/26/28/67) and unresolved indirect sites (119/84/435/136) are unchanged from run 2 except B 436 -> 435. Analyzer-only starts (no seed): boot 51, A 31, B 88, C 128.

**Matched functions vs run 5 (`python -I analysis/check_matches.py recomp/sandbox/_out_run5/A/seeded/functions.tsv`):** 353/359 are analyzer functions of equal size.
- 0x800344D0, 0x800378CC, 0x8003B284: the analyzer's own `find_preprologue_setup_start` split (start at the `addiu sp` + 4/8 bytes). None of 0x800344D4/0x800378D0/0x8003B2A8 is a seed any more. Our boundaries are right (byte match); the recomp must override these three analyzer starts.
- 0x80047690, 0x800489DC, 0x80048CEC: matched 60 B, analyzer 64 B. **The analyzer is right**: each ends `addiu sp,sp,0x18; jr ra; nop` at +52/+56/+60, and the next function's prologue is at +64. The `jr ra` delay-slot nop at +60 belongs to the function. The gate still passes because `scripts/match.sh` accepts zero bytes past `size` as padding. Fix: set these three rows of `config/matches.tsv` to 64 (outside this task's paths; then rerun `make_seeds.py`).

**Remaining A/C unresolved indirect sites (all 4 are `static_slot_unassigned`; now classified, still no code target):**
- A 0x8003C498 / C 0x8003D3F4, `static_slot_dead`: slots 0x8004E170 / 0x80074368 are set only by the setters 0x8003C56C (`func_8003C56C`, `D_8004E170 = a`) / 0x8003D4C8. Neither setter has a `jal`, a pointer word or a `lui 0x8004` + `addiu` pair in the boot exe or any of the 359 decompressed modules. `func_8003BA74` clears the slot at init, and `func_8003C434` guards the call with `if (D_8004E170)`. So the call is dead in the shipped code.
- A 0x8004B344 / C 0x800715C4, `bios_dcb_hook`: the slot 0x8004EFE4 / 0x80078880 receives `lw v0,0x34(s0)` while `s0` walks the BIOS device table (`lw s0,0x150($zero)`, count = `lw 0x154 / 0x50`, 0x50-byte records, name compared by `jal 0x8004B374` with a runtime-filled bss string). The game saves a device's handler at DCB+0x34, installs its own (`sw s3,0x34(s0)`), and calls the saved BIOS handler through this `jalr`. The target is BIOS kernel code, not game code. The psxrecomp runtime must handle it through its BIOS path.
- Written to `config/indirect_targets.tsv` (4 rows; kind, confidence `high`, note) and into `analysis/indirect.py` as a `VERIFIED` override, so a rerun keeps them. Program A now has 0 unexplained sites (84 = 81 vtable + 1 BIOS dispatch + 2 dead/DCB).

**`analysis/check_matches.py`** now also fails when a matched function's `src/<name>.c` is missing or does not define it, or when any symbol sits inside a matched body, and its stale-symbol message names the fix (`SO2_DATA=<main checkout> python -I analysis/make_seeds.py`). Tested: a fake `matches.tsv` row prints 3 PROBLEM lines (symbols, src, seed); the real file passes. `config/symbols.toml` was regenerated: 480 symbols = 359 confirmed (with `size`) + 120 splat labels + BootEntry; e1c0 seeds 818 -> 799.

Open: the 3 sizes above; the 3 analyzer split starts need an override in the title repo's config; the 8/12/5 seeds that remain non-starts in a no-seed scan are referenced or gap code, and only a runtime trace can confirm them. The other unresolved sites of B (11) and C (11) were not re-examined here. Nothing was generated or run beyond `psxrecomp-analyze`.

## 2026-10-26 B/C indirect sites, split starts, toolchain
**Indirect sites (B 11, C 11): 21 settled, 1 open.** Method: every decompressed module (89 unique + boot exe, from disc 1) scanned for `jal`, pointer words and `lui`+`addiu/lw/sw` references; vtable stores read in the storing module. Recorded as hand-verified overrides in `analysis/indirect.py` (`VERIFIED`, so a rerun keeps them) -> `config/indirect_targets.tsv` (22 rows changed, nothing else; rerun before the change was byte-identical). `indirect.py` now reads its gitignored inputs from `SO2_DATA` like `make_seeds.py`. Input is still `_out_run2/summary.txt` (B 436 sites); `_out_run5` is not on disk any more, so run 5's B 435 could not be re-diffed.

| site | kind | evidence / targets |
|---|---|---|
| B 800443b0, 8004e058 | `static_slot_dead` | slot 8006fe04: only store in any module is `sw $zero` at 80074dc4; both calls `beqz`-guarded |
| B 8006a038 | `static_slot_dead` | slot 80070b2c set only by setter 8006a10c (`sw a0`), which nothing references; `beqz`-guarded |
| B 800545b0 | `fnptr_table` medium | table 8006fea8 indexed by object type (`lhu 0x108`), 52 words ([0]=0, [52] is data); 32 B function starts listed, 14 more entries point into overlay slots (2131 family 801efxxx/801f0xxx, 1984 handler slot 8008c2xx, 80088cb0, 8008a30c), named in the note, not seeded |
| B 8005b540 | `member_setter_unreferenced` | `obj+0x138` written only from `a0` of 8005b4b0 (type 0x21) / 8005b4f0 (type 0x33); neither is referenced anywhere. 8005b530 = table 8006fea8 entries 33 and 51 |
| B 8005e238 | `fnptr_table` medium | gcc 2.x pointer-to-member table 8006ffe8 `{delta,index,pfn}`: entries 0-4 index -1 -> 8005e618 8005e7d8 8005e8f0 8005ee08 8005f128 (all analyzer starts), 5-6 null |
| B 800719a4 | jump table, high | 80078f38, exactly 12 entries (the next table 80078f68 is referenced at 80071c00) |
| B 8007ad98, 8007fda0 | `vtable_dispatch` low | vptr +4, slots +0x4c / +0x64: generic union, 6 / 5 targets (the evaluator had stopped at a live-in `v0`) |
| B 80084020 | `overlay_entry_table` high | table 800851b0 = 801ee000 801f2000 801f6000 801fa000 (enemy-set slot bases), not seeded |
| **B 8007e46c (open)** | `table_unreadable` | guard 5, table 801f9774 is in the 0x801f6000 slot RAM; no module holds targets inside this function there. B's bytes from 0x800715d0 on are overlaid by 1984 and the 0x8007e000 field overlays, so this may be stale image code; needs a runtime trace |
| C 8003da20, 8003ed5c, 8003edb4 | `vtable_dispatch` medium | vptr +0x3f8: base ctor 8003d4d8 stores vtable 80072dd4 (slots -> 8006fe00, a `break` = pure virtual); derived ctors in e3101c0 (vtable 8008ebe0) and e3102c0 (8008f460). Targets per slot +0x14/+0x24/+0x1c: 8006fe00 + `e3101c0:80082838/80081cc8/80081b54` + `e3102c0:80085868/80084e04/80084da8` |
| C 80047564, 800477e8 | jump tables, high | gcc hoisted the table base out of the loop: 80072f3c (`+a2*4` kept at sp+8) and 80072f5c (s5), guard `sltiu 8`, 8 entries each |
| C 800492cc, 8004a088, 8004a174 | `vtable_dispatch` high | `*(800752c8)` is written only by e3103c0 (0x801e9000) from ctors 801eb2d8 (vtable 801ecacc) / 801ebb6c (801ecaf4) or zero; slot +0xc -> `e3103c0:801eb480`, `e3103c0:801ebc38` |
| C 8004be74, 8004bea8, 8004bedc | `vtable_dispatch` high | `*(800752e4)` = `new[4]` of 0x28-byte elements (8004cae0, count word 4), element vptr +4 = 80072e5c; sites read elements 1-3 slot +0x14 -> 800459c0 |

Totals now: B 430/436 with targets (+6 classified, 1 open), C 73/136 (all 136 classified). `make_seeds.py` accepts `<module id>:addr` targets (seeds that overlay module); rerun adds C 8006fe00. All overlay targets were already seeds.

**Split starts.** The three 64-byte rows (func_80047690, 800489DC, 80048CEC) now equal the analyzer size (run 5: 64 B), so that mismatch is gone; `symbols.toml` regenerated (`size = 64`, 378 confirmed, `check_matches.py`: `matches 378; confirmed symbols 378; e1c0 seeds 800`). The pre-prologue splits 0x800344D4 / 0x800378D0 / 0x8003B2A8 stay: psxrecomp f42d2c0 has **no config key to delete or merge an analyzer start**. They come from `find_preprologue_setup_start` (function_analysis.cpp:285), called from the `jr ra` back-scan (:377), JAL targets (:1969) and the strong-prologue pass (:2008); seeds only add starts (`interior`/`dispatch_root`/`call_root` lines in main_psx.cpp:824ff also only add). The generator emits a direct fallthrough edge into the next function (code_generator.cpp:104), so each split costs one extra dispatch entry, not correctness. Override text needed: none; keep the exact seeds 0x800344D0, 0x800378CC, 0x8003B284 (already in `seeds_overlays.txt`). Upstream fix if wanted: let an exact seed suppress a preprologue start inside its span.
New finding: `check_matches.py` reports `func_8002F8E0` (148 B) overlapping `func_8002F8E4` (144 B). 8002F8E0 is `andi a0,a0,0xfff` falling into 8002F8E4 (sine lookup); A has 30+ `jal 8002f8e0`, the only `jal 8002f8e4` is in C (8002fac4). Both rows byte-match, but they describe one span: `config/matches.tsv` (outside this task's paths) should keep one row, or the checker must allow a documented alternate entry. `make_seeds.py` now seeds 8002f8e4 in e1c0, which creates the same kind of split.

**game.toml.** `disc_speed = "1x"` is the runtime default (main.cpp: `int divisor = 1; /* default: authentic 1x timing */`); faster values are a later tuning step. `bios_hle` stays off: opt-in, NULL hook = pure LLE (bios_hle.h), and the game chains to BIOS device handlers through DCB+0x34 (A 0x8004B344), so the real kernel must run. Still `TODO(verify)`: `[video]/[widescreen]` (look decision) and the AOT profile's `expected_records`.

**Toolchain (checked known paths only).** Present: VS 2022 BuildTools with MSVC 14.44.35207 (`VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`), its bundled CMake and Ninja (`Common7\IDE\CommonExtensions\Microsoft\CMake\{CMake,Ninja}`), Windows Kits 10; TinyCC in the reference clone's `.cache\overlay-toolchain\tcc_extract`. Not on PATH; no clang, MSYS2 or `cmake-clang-v1` pack. **Generation not attempted**: `recomp/sandbox/README.md` keeps all psxrecomp builds off the host (`psxrecomp_cli.py` resolves or downloads `cmake-clang-v1` via `toolchain_pack`, and the toolchain's installer edits PATH), and the private title repo (`setup_project.ps1`, `disc/`) does not exist yet. To proceed, the user decides: (a) extend the sandbox harness to build the `psxrecomp` target (recompiler/CMakeLists.txt:150) with MSVC + bundled CMake/Ninja and media CLI OFF (it uses FetchContent for libchdr), then `generate` the boot exe; or (b) allow a host build with exactly those tools.

## 2026-10-26 psxrecomp safety audit
**Verdict: SAFE WITH CAVEATS**, for exactly this scope: building `psxrecomp-analyze` and `psxrecomp-game` (the generator) from source at commit **`f42d2c004b3dd6ef6ccb83101b40ede55f24bf59`** (2026-10-25, "Merge Tron Bonne resident-sector adapter and validation"; clone `tools/ref/psxrecomp`, `git status --porcelain --ignored` empty before and after) with `-DPSXRECOMP_ENABLE_CHD=OFF -DBUILD_TESTING=OFF`, and running them directly. The caveats are all in parts we do not build or run, and they are mitigated by never using those parts. Not vetted, so do not run: `psxrecomp_cli.py`, the codegen host (`host/`), the runtime, `tools/*.py`, the submodules, and every prebuilt binary in the clone. Earlier triage of the release CLI zip (Defender `Trojan:Win32/Suschil!rfn`, likely a false positive) is in `tools/ref/SECURITY_TRIAGE.md`, which is gitignored.

**1. Source audit.** The audit was pattern sweeps over the whole tree, plus targeted reading of every file-I/O path in the two targets. It was not a line-by-line read of the 30,239 lines in `recompiler/src` + `include`.
- **Network:** the two targets' sources use no socket, HTTP, WinINet, WinHTTP, URLDownload or curl. `recompiler/CMakeLists.txt` has one FetchContent step (libchdr, `runtime/chd_dependency.cmake`), and it is skipped with CHD OFF. It is pinned by hash anyway (`third_party/deps.manifest`). Vendored fmt 9.1.1, ELFIO 3.11, toml11 and rabbitizer have no network or process calls outside their tests and docs. Their tests (googletest FetchContent, toml11 ExternalProject) are not built.
- **Process spawning:** none in the two targets. `std::system` is in `main_bios.cpp` (psxrecomp-bios) and `fork`/`execv` is in `main_cli.cpp` (media CLI); neither is built. At build time the only command is `cmake -P runtime/hash_codegen.cmake`, which reads the listed sources, hashes them, and writes one header into the build dir.
- **Registry and system calls:** none. No `LoadLibrary`, `VirtualProtect` or `Reg*`. Four environment variables only toggle codegen (`PSX_CPS`, `PSX_EMIT_PROD`, `PSX_CODEGEN_CYCLE_PER_INSN`, `PSXRECOMP_VERBOSE_RI_WARNINGS`).
- **File I/O (generator):** reads the exe, the seeds, `game.toml` (`--config`), `game_options.toml` next to it, and `bios/SCPH1001.toml`. The BIOS profile is searched for in the project root or one directory level below it; only that profile's TOML is read, never the BIOS image. Writes only into `out_dir` (`main_psx.cpp:1657-1894`: `<stem>_decls.h`, `_full_NN.c`, `_full.ranges`, `_dispatch.c`). Deletes only its own `<stem>_full.c` and stale `<stem>_full_NN.c` in `out_dir` (:1840-1845). The settings writers in `config_loader.cpp` (:3505, :3768) are not called by either main.
- **File I/O (analyzer):** writes only to `--out` and the `--emit-*` paths (`analysis_export.cpp`).
- **Obfuscation:** no line over 400 characters and no base64-like run of 200 or more characters in `recompiler/src` or `include`.
- **Caveats found (not built, not run):**
  - Prebuilt, unverified binaries are tracked in git: `overlay_toolchain/psxrecomp-game.exe` (sha256 `9fe70b8c2e700e58…`), `psxrecomp-bios.exe` (`8ee52e3e32db6063…`), an embedded Python 3.13 with `_ssl`/`_socket`, and TinyCC (`overlay_toolchain/tcc`, plus a copy in `.cache/`).
  - `bios/openbios.bin` is a binary blob.
  - The CLI and codegen host download the toolchain from `releases/latest` without a hash check and spawn shells. The toolchain's `install.ps1` edits the user PATH (SECURITY_TRIAGE.md).
  - Submodules `lib/recomp-net` (`e5305930`) and `lib/retcomm-rbengine` (`2a03e736`) are not checked out and were not reviewed.
  - The runtime's FetchContent dependencies (SDL3, zlib, libjuice, OpenXR, libwebp) are hash-pinned in `third_party/deps.manifest`, but they are outside this scope.
  - The clone's `CLAUDE.md`/`AGENTS.md` contain instructions aimed at AI agents (for example, to consult an external chatbot through a browser). Agents working near the clone must treat them as data.

**2. Sandbox runs** (`recomp/sandbox`, run `_out_gen1`, status OK; no network, read-only inputs). The run built both targets with the pinned cmake-clang-v1 (clang 22.1.8). It then ran the analyzer 8 times (boot/A/B/C × seeded/exact) and the generator twice: the boot exe through `--config` with our `recomp/game.toml` in a minimal project dir, and program A positionally with `A.exe` + A seeds.
- **Watching:** every audited process was watched with a WMI process-start trace, polled TCP/UDP endpoints (every 100 ms), and a new or changed file sweep over `C:\Users`, `C:\ProgramData`, `C:\Windows\Temp`, `C:\` and `C:\work` (`watch.log`).
- **Result:** across all 10 runs there were **0 network endpoints and 0 child processes**. No file was written outside each run's output dir, except two things that are not from psxrecomp. First, our own staging copies in `C:\work\proj`, written by the script just before `generate_boot`. Second, Windows shell WebView state under `MicrosoftWindows.Client.CBS` (seen once, during `analyze_A_seeded`). The other process starts in the windows were the script's own `clang` compiles and `taskhostw`.
- The "1 file" counts with no path in `watch.log` were a script bug (a `$null` entry), fixed after the run.
- **Analyzer output** reproduces run 5: boot 578 functions, 262 reachable; A 830.
- **Generator, boot:** 750 functions, 10 shards, 371,762 lines, 750 dispatch entries, 12.3 MB.
- **Generator, A:** 1,496 functions, 45 shards, 4,006,544 lines, 1,401 dispatch entries, 111 MB. It also printed 35 `reserved opcode 0x5FFF0006` warnings at 0x800515B8-0x80061C54 (RI-exception stubs; probably data decoded as code, not yet checked).
- **Compile in the sandbox** (`clang -c -O0`, includes `runtime/include`): boot 11/11 TUs ok, A 46/46 ok.

**3. Host build** (`recomp/host/build_generate.ps1`; output `recomp/host/_out`, gitignored). The script refuses to run unless the clone is at the audited commit with no changes. It uses VS 2022 Build Tools MSVC 14.44 with the bundled CMake and Ninja, through a per-process `vcvars64` environment. It builds the same two targets (0 errors) and then checks that the clone is still clean (it is). Host generation output is **byte-identical to the sandbox output: 13/13 boot files and 48/48 A files** have the same sha256. Generated C stays in gitignored folders only.

Open:
- The generated C was compiled only in the sandbox (clang). It was not compiled on the host and is not linked against the runtime, which has not been vetted.
- The A reserved-opcode range has not been examined.
- The AOT/SLZ overlay path (`aot_overlay_pipeline.py` with `recomp/aot/overlays.json`) is Python tooling outside this audit and was not run.
- A re-audit is needed for any other commit.

## 2026-10-26 Host compile, A data region, overlay decoder
**Host generation (rerun).** The earlier host and `_out_gen1` outputs were gone, so `recomp/host/build_generate.ps1` ran again (clone at f42d2c0, `git status --porcelain --ignored` empty before and after). Same counts as the audit run: boot 750 functions / 10 shards / 750 dispatch entries; A 1,496 / 45 / 1,401. The script no longer deletes anything (the two `Remove-Item` lines are gone; reruns overwrite, the generator drops its own stale shards), takes `-OutName` / `-ASeeds`, and adds a third run `A_reach` (`recomp/host/A_reachable.toml`, below).

**1. Host compile of the generated C** (`recomp/host/compile_generated.ps1`; MSVC 14.44.35207 `cl /c /Od /W3 /MP /bigobj`, x64; no clang on this machine, only `clang-format`/`clang-tidy` under `VC\Tools\Llvm`). Nothing linked or run; objects stay in gitignored `recomp/host/_out/obj_<label>`.

| set | TUs (shards + dispatch) | objects | errors | warnings |
|---|---|---|---|---|
| boot | 10 + 1 | 11/11 | 0 | 576, all C4102 (unreferenced label `block_<pc>`) |
| A (whole-image) | 45 + 1 | 46/46 | 0 | 1,157, all C4102 |
| A (reachable) | 17 + 1 | 18/18 | 0 | 780, all C4102 |

- **Headers needed:** only `psx_runtime.h` from the clone's `runtime/include`, whose include closure is 10 files: `psx_runtime.h cpu_state.h debug_server.h interrupts.h psx_cycles.h psx_cyc.h psx_icache.h psx_memory.h pgxp_hooks.h ws_cull_edge.h` (1,929 lines, 88 KB); external: `stdint.h stddef.h stdio.h string.h intrin.h`.
- **Audit of those 10 headers (compile-only use):** sweep for `#pragma`, `#import`, `#line`, asm, `system`/`popen`/`exec*`/`CreateProcess`/`ShellExecute`, `LoadLibrary`/`GetProcAddress`, sockets/HTTP/WinINet/URLDownload, `Reg*`, `fopen`/`CreateFile`/`DeleteFile`/`remove`/`unlink`, `getenv`, `VirtualAlloc`/`VirtualProtect`, and lines over 300 characters: **no hit** except `__declspec(dllexport)` (an export attribute, cpu_state.h:82) and the word "socket" in a comment (debug_server.h:88; declarations only). `psx_runtime.h` read in full (one `static inline` wrapper around `psx_dispatch_call`). Compiling them runs no code from them. sha256 prefixes: cpu_state 36b7a455, debug_server 90f6972b, interrupts cff6bd85, pgxp_hooks ff0b18b8, psx_cyc f572e45d, psx_cycles 920031ed, psx_icache 97f005ba, psx_memory 198ca9bf, psx_runtime 4e3fd4d7, ws_cull_edge 0fe5adf0.
- **Runtime symbols the objects need at link time** (`dumpbin /symbols`, UNDEF minus defined): boot 15, A 21: `psx_read_word psx_read_half psx_cyc_load_byte psx_cyc_lwc2_read psx_syscall psx_unknown_dispatch psx_check_interrupts_at psx_check_interrupts_dispatch_entry psx_vsync_query_hle_try psx_restore_state_escape g_psx_cps_mode g_debug_last_store_pc debug_server_cyc_observe debug_server_log_call_entry dirty_ram_text_native_ok_ranges(_from) gte_execute gte_write_data gte_write_ctrl gte_precision_store_word` (+ `__ImageBase`, `__chkstk` from the CRT). These live in the unaudited runtime; linking needs that audit first.

**2. The 35 reserved-opcode warnings in A are data.** 4 at 0x800515B8-0x800515D0 and 31 at 0x80061BD8-0x80061C54 (words like `0x5C010006`, `0x5FFF0006`: halfword pairs, primary opcodes 0x14-0x17). Classified on the local `analysis/out/d1/0001_312.dec`:
- A's code stops at 0x800509DC (`jr ra` at 0x800509D4 + delay slot) and resumes at 0x80061C58 (`addiu sp,sp,-0x48`). In between is **one data block of 70 KB**: tables at 0x800509DC-0x80050C58 (MDEC IDCT coefficients 0x5A82/0x7D8A/0x7641..., a list of I/O register addresses 0x1F801080-0x1F8010F0), then 0x80051458-0x80061458 (64 KB) and 0x80061458-0x80061C58 (2 KB), both set up as table pointers by the code at 0x80050680 (`lui/addiu a2 0x80051458`, `a3 0x80061458`).
- No `jal` and no pointer word anywhere in A targets the block; the only references are `lui`+`addiu/lw` data accesses from code (62 sites). Invalid-opcode density 24 % (4,435 of 17,782 words). None of our 799 A seeds lies in it.
- The whole-image generator still mints **132 functions** inside it (data words that decode as `jal`/branches give it targets; three of them, at 0x80050C58/E00/EE0, run 0xC988 bytes to the block end). All 35 warnings come from those.
- **Exclusion:** psxrecomp f42d2c0 has **no data-range key**. Config keys (`config_loader.cpp`) and seed directives (`producer_range`, `cross_call_allow`, `retained_alias`, `hosted_interior`, `interior`, `dispatch_root`, `call_root`; main_psx.cpp:870ff) only add or mark entries. The supported lever is `[recompiler] discovery = "reachable"` (exact-entry walk from the seeds), and the AOT overlay path uses the same walk (`--overlay`, set by `compile_overlays.py`). So no seed change is needed, and `recomp/game.toml` (boot exe) keeps `whole-image`.
- **Comparison** (same generator, A.exe + A seeds): whole-image 1,496 functions, 35 warnings; `discovery = "reachable"` (`recomp/host/A_reachable.toml`) **796 functions, 0 reserved-opcode warnings, 0 in the data block**; `--overlay` gives the identical 796-function set. Of the 700 whole-image-only entries, 132 are in the data block, 541 lie inside a reachable function's range (whole-image splits), 27 are in bytes no reachable function covers (not examined: possible orphans/indirect targets).
- **3 seeds fall to the interpreter in exact mode:** 0x8004B074, 0x8004B084, 0x8004B094 (`WARNING: interior seed ... has no host function`). They are BIOS B0 stubs (`addiu t2,0xB0; jr t2; addiu t1,0x33/0x34/0x36`), `jal` targets from 0x800376A0/0x800376D4/0x800376BC/0x80037720, but not preceded by `jr ra`, so the exact walk does not accept them as callable. Marking them `call_root 0x...` in the seed file fixes it: **799 functions, 0 warnings** (`--overlay`, test file `_out/stage/A.seeds.callroot.txt`). Not yet carried into `config/seeds_overlays.txt` / `make_seeds.py` / the AOT profile entries.

**3. Overlay pipeline: decoder audited, pipeline not run.**
- **Decoder audit** (`analysis/slz.py`, ours, 48 lines; only `struct`, no I/O, no eval/exec/import machinery). Hook needed in the psxrecomp tree: `import slz` and `'so2_slz': (slz.prepared, ())` in `tools/prepared_files.py` (138 lines, read in full: imports `hashlib` and `bpe_container`, reads only through the disc view, calls the decoder, then checks `consumed`, `source_sha256`, `size`, `sha256`). Finding: `prepare()` hands the decoder `data[start:]` (the whole rest of SO2.BIN) and hashes the input **after** decoding, so the decoder sees unverified bytes. The old `prepared` decoded from that unbounded slice, and a version byte other than 0-2 never stopped at `dsize`, so a corrupt chunk could produce output until the end of SO2.BIN. **Fixed:** header and version (0-2) checked first, `dsize` capped at 2 MB (PS1 RAM), decoding only from `d[:16+csize]`. Every error path stays `ValueError`. Self-test now covers 6 malformed inputs (+ unknown version, oversized dsize): `self-test: slz.prepared OK on 18 vectors + 6 malformed inputs`; `recomp/aot/overlays.json` regenerated byte-identical (`git diff --quiet`); `python -I tests/slz/run_tests.py`: `OK 0`.
- **Not run, and why.** `aot_overlay_pipeline.py extract` runs `tools/aot_overlay_spike/extract_generic.py` in a subprocess (which drives the generator); `static`/`release` add gcc compiles, `cmake -P` and, for release, the runtime build. It imports `compile_overlays.py` (9,480 lines, several `subprocess.run`). None of this is audited. Windows Sandbox has no Python, and the only Python in the clone is the unvetted embedded one. A sandbox run needs (a) you to open the `.wsb` (the harness never starts it), and (b) a decision on which Python goes into the sandbox (for example a read-only mapping of the host's Python install), plus disc 1 mapped read-only. The harness was not extended for it.

Open: link and runtime (needs a runtime audit of the symbols above); carry the 3 `call_root` seeds into the seed tooling; the 27 uncovered whole-image-only starts; the sandbox overlay run (decisions above). Leftovers (gitignored, nothing deleted): `recomp/host/_out/` (build, proj, proj_reach, gen_A, gen_A_overlay, gen_A_overlay_cr, obj_boot/A/A_reach, obj_test, compile/generate logs, stage incl. `A.seeds.callroot.txt`), `recomp/host/_out_reach/` (first reachable run), `recomp/host/_run_*.log`.

## 2026-10-26 Runtime link audit (boot exe)
**Verdict: NOT SAFE to link, under this task's rules (audited sources only, no downloads, no prebuilt binaries).** Nothing found looks malicious (no obfuscation, no registry or `VirtualProtect`/remote-thread calls, no hidden downloads), but the link closure is the whole runtime, which cannot be audited at the depth this needs, has network, process-spawn and DLL-loading code, and requires SDL3, which is not on this machine except as an unaudited prebuilt. **Nothing was linked or run.** Clone `tools/ref/psxrecomp` at f42d2c0, `git status --porcelain --ignored` empty before and after.

**1. Where the 19 runtime symbols live** (the 15 boot symbols are a subset; definitions by grep in `runtime/src`):
- `memory.c` (2,636 lines): `psx_read_word/half`, `psx_cyc_load_byte`, `psx_cyc_lwc2_read`, `dirty_ram_text_native_ok_ranges(_from)`.
- `traps.c` (1,469): `psx_syscall`, `psx_unknown_dispatch`. `interrupts.c` (2,271): `psx_check_interrupts_at`, `..._dispatch_entry`, `psx_restore_state_escape`. `load_accel.c` (267): `psx_vsync_query_hle_try`. `overlay_loader.c` (5,262): `g_psx_cps_mode`. `gte.cpp` (2,871): `gte_execute`, `gte_write_data/ctrl`, `gte_precision_store_word`.
- `debug_server.c` (**17,699**): `g_debug_last_store_pc`, `debug_server_cyc_observe`, `debug_server_log_call_entry`.

**2. Dependency closure.** These 7 files include about 100 runtime headers: GPU/GL renderer, SPU, CD-ROM, MDEC, savestates, mods, netplay, OpenXR, rewind, the BIOS image, the overlay loader and the debug server. Their callees span the runtime. `runtime/src` + `include` hold 440 files and about 170,000 lines in `src` alone. So "only the files that define the symbols" is not a link set; the link set is the runtime library (`runtime/runtime.cmake`, 154 KB of CMake), which needs:
- **SDL3 3.4+** (`runtime.cmake:252-316`: `find_package`, then a FetchContent of `SDL3-3.4.10.tar.gz` from GitHub, else `FATAL_ERROR`). `debug_server.c:119` includes `psx_sdl.h` and uses `SDL_Thread`/`SDL_mutex`, so even a hand-made link list needs SDL3 headers and library. SDL3 exists here only as a **prebuilt MinGW library** inside `tools/recomp/dl/cmake-clang-v1-windows-x64.zip` (unaudited, wrong ABI for MSVC). **Blocked: it needs a download (hash-pinned in `third_party/deps.manifest`, sha256 `12b34280...`) or a prebuilt.**
- zlib (`runtime.cmake:976-1007`, same find-or-fetch pattern), Threads, OpenGL; OpenXR, libwebp, libjuice and recomp-net only behind options.
- Vendored in the runtime: `third_party/stb_image.h`, `xxhash.h` (not read).

**3. Pattern sweep over `runtime/src` + `runtime/include` (440 files)**:
- **Network** (58 hits, 9 files): `main.cpp` contacts **`api.ipify.org:80`** (`ae_np_external_ip`, :15294, the public-IP lookup in the netplay launcher's callback table, :16605) and binds UDP sockets for LAN discovery and netplay (`--net-bind`, default `0.0.0.0:7777`). `psx_lobby_client.c` + `lobby_ws/` hold a WebSocket client; the default lobby is `ws://netplay.retcomm.net:8765`. `debug_server.c` has a TCP debug server bound to `INADDR_LOOPBACK` (:17012). Also `beetle_debug_server.c` and `cosim.c`.
- **Process spawn** (7 files): `autocompile.c:1204` (`cmd -> python -> gcc` at run time, below-normal priority, in a job object), `main.cpp:11419` (`system("python tools/prepare_disc.py ...")`), `psx_bios_module.c:380/418` (`CreateProcessA`/`popen` of a BIOS build command), `host_overlay.c:314` (relaunches its own exe), `mod_runtime.cpp:876`, `debug_server.c:15407` (`system("mkdir ...")`).
- **Dynamic loading:** `overlay_loader.c` `LoadLibraryA` of compiled overlay DLLs (5 sites), `psx_bios_module.c:247`, `host_sha256.h:29` (`bcrypt.dll` from System32 only).
- **File writes/deletes** (61 delete/rename sites in 14 files): savestates, memcards, mod packages, overlay capture, `atomic_file_write.h`. Not traced to a single directory.
- 307 `getenv` sites in 45 files (91 in `main.cpp`).
- None: registry, `VirtualProtect`/`VirtualAlloc`/`WriteProcessMemory`/`CreateRemoteThread`, lines over 400 characters, base64-like runs of 200 or more characters.

**Read in full:** none of the 170,000 lines; only the call-site contexts quoted above. This is a capability map, not an audit.

**What would unblock a link** (decisions for you):
1. Supply SDL3 3.4.10 and zlib 1.3.1 source archives yourself, checked against the `deps.manifest` hashes (no download by an agent), and approve a full audit of the runtime. That is a multi-session job; `main.cpp`, `debug_server.c` and `overlay_loader.c` are the risk centres.
2. Or build the runtime only inside Windows Sandbox (networking off). That still needs SDL3: from the pinned toolchain zip (prebuilt, used there before for clang only), or from a source tarball you supply.
3. Or write our own minimal shim for the 19 symbols plus `main` to test the generated C alone. It would not test psxrecomp's runtime, and a boot needs a BIOS/HLE layer that a shim lacks.

**BIOS:** not reached. psxrecomp bundles `bios/openbios.bin` (unvetted blob, see the safety audit); per the `PSXRECOMP_ALLOW_NO_BIOS`/`PSXRECOMP_BIOS_STALE_FATAL` options the runtime expects a BIOS image. Never download one: if a later run needs one, you supply your own SCPH1001 dump.

## 2026-10-26 AOT overlay pipeline run (SLZ, program A)
**The pipeline runs: all 89 SLZ chunks decode and verify, and program A (e1c0) goes through `static` to C.** Work copy of the clone (copied, not moved): `<home>\star-cean-wt\psxrecomp-overlay` (outside the repo, f42d2c0). The shared clone `tools/ref/psxrecomp` was not touched (`git status --porcelain --ignored` empty after the runs). Changes in the copy: `tools/prepared_files.py` +2 lines (`import slz`, `'so2_slz': (slz.prepared, ())`), `tools/slz.py` = copy of `analysis/slz.py`. Run on the host (Windows Sandbox has no Python and needs the user to open the `.wsb`, so it cannot be driven non-interactively) after the audit below; `python -E -s` (not `-I`: the tools import their siblings from the script dir), with `timeout`.

**Audit of the code paths run** (f42d2c0; pattern sweep + reading of each spawn site, not line by line of `compile_overlays.py`):
- Imported modules (`aot_overlay_pipeline`, `prepared_files`, `extract_generic`, `extract_overlays`, `compile_overlays`, `packed_sector_table`, `sector_extent_archive`, `aligned_lzss_banks`, `indexed_lzss_pack`, `mips_tagged_relocations`, `counted_mips_relocations`, `mod_package_images`, `bpe_container`, `audit_aot_cache`): AST check of module top levels: no side effects on import (only `sys.path.insert` in extract_generic and regex/const setup). No socket/HTTP/urllib, `eval`/`exec`, `winreg`, `os.system` in any of them.
- **Processes spawned** (all `subprocess.run` with argument lists, no shell): `extract` -> `python extract_generic.py` (reads the cue, writes `generic.json` + tmp); that spawns `psxrecomp-game producer.exe` only for PS-X EXE files other than the boot exe, and disc 1 has none (files: SCUS_944.21 skipped as `[game].exe`, SO2.BIN skipped as > 2 MB, SYSTEM.CNF), so it spawned nothing. `static` -> `cmake -P runtime/hash_codegen.cmake` (reads emitter sources, writes `runtime/include/overlay_codegen_hash.h` in the copy), `python compile_overlays.py --static` -> `psxrecomp-game --codegen-hash`, `--overlay-config-hash <toml>` (both print and exit, main_psx.cpp:138-152) and `psxrecomp-game frag.psx --seeds --out-dir --overlay --ws-config --project-root` (audited generator; `--ws-config` only loads the TOML), then `python audit_aot_cache.py` (reads, writes one JSON; runs `--overlay-config-hash`). The static path compiled nothing: no gcc/tcc call in the log (`compile:` lines: 0), so no DLL was built or loaded through ctypes (that is the `release` DLL path, not run). gcc/cmake are not on PATH; cmake given as the VS-bundled exe. Generator: the audited host build `star-cean-wt/recomp2/recomp/host/_out/build/psxrecomp-game.exe`; codegen hash `1bf58ed4` matched the copy's sources.
- Not run, not audited: `release` (gcc DLL builds, ctypes loading, staging), `build()`, `release_stage.py`, mod-package paths, BIOS-resident recipes (profile has none).
- Writes: only the `--work-dir` / `--out-dir` (`_work/` in the copy), the codegen header, and `__pycache__`.

**Profile fix.** Run 1 failed `Expected 90 recipes, got 89`: the `psx_exe` image for SCUS_944.21 can never be covered, because extract_generic skips `[game].exe` (the boot exe is the static base, generated by `psxrecomp-game --config`; psx_exe images are for secondary exes). `analysis/aot_profile.py` now omits it and pins `expected_records = 89`. Run 2 (`extract`, full profile): `Verified 89 images / 89 recipes`; generic extractor `0 regions`; every recipe is a `fixed_address_files` record. 5,356 heuristic entry PCs in total.

**Program A static run** (`static`, subset profile = SLZ/E1C0 only, `_work/profile_A.json`): recipe 0x8002F000 (page-aligned) 285,668 B, producer range 0x8002F810-0x80074DE4, 810 entry PCs / 527 static-discovery roots (extractor heuristics; our `config/seeds_overlays.txt` is not used by this path). Generator: 812 functions, 0 unsupported-instruction TODOs, 0 bad targets, 0 warnings; 10 interior shards; 7,923 CPS resume wrappers; **8,745 exact function identities in 3 files** (`overlays_static.c` 3.3 MB + `_0000.c` 20.9 MB + `_0001.c` 0.26 MB, 673,676 lines), `audit.json`: `all_pairs_valid`, `all_guards_match_known_input_bytes`, `full_static_coverage_proven: false`. Output stays in `star-cean-wt/psxrecomp-overlay/_work/static_A` (not in git). Not compiled or linked.

**What the runtime must provide** (runtime/src/dirty_ram_interp.c:3178): a jump into RAM outside the boot text goes to `psx_overlay_dispatch` (static variants, chosen by the CRC of the bytes currently in RAM), then the DLL cache, then the interpreter. So no overlay load addresses or entry points need registering: the game's own recompiled loader (boot exe/A code) does the CD reads and the SLZ decode into RAM, and each static variant fires only when RAM holds its exact bytes. Needed: the CD-ROM controller + DMA3 serving SO2.BIN sectors from the cue, the LLE BIOS kernel (a BIOS image: user-supplied SCPH1001.BIN or OpenBIOS; DCB+0x34 chaining, A 0x8004B344), the interpreter fallback, and the 21 runtime symbols the generated code imports. Missing before overlays can actually start: the runtime audit (other worker), a title project whose CMake sets `GAME_OVERLAY_STATIC_C` / profile `static_output`, the boot exe shards + `overlays_static.c` compiled and linked against the runtime, and `static` for the other 88 chunks (cost: about 25 MB of C per program-sized chunk).

Open: the 3 `call_root` stubs and our seed list are not fed into this path (the extractor's own heuristics are; `entries` per image can add them); medium-confidence load addresses not run. Leftovers (outside the repo, nothing deleted): `star-cean-wt/psxrecomp-overlay/` (whole copy incl. `_work/run1`, `run2`, `runA`, `static_A`, `proj`, `profile_A.json`, logs, `__pycache__`, `runtime/include/overlay_codegen_hash.h`).
## 2026-10-26 Host generation B/C, A orphan starts
Host build at the audited f42d2c0 (`recomp/host/build_generate.ps1 -Program boot,A,A_reach,B,C`, then `compile_generated.ps1 -Program boot,A_reach,B,C`; output in gitignored `recomp/host/_out/`; no network, nothing deleted; the VS `Installer` dir must be on PATH for vcvars64's vswhere when launched from a non-VS shell). Boot and whole-image A reproduce the earlier counts (750 / 1,496).

| set | seeds | functions | generator warnings | TUs compiled (MSVC /Od) | errors | cl warnings |
|---|---|---|---|---|---|---|
| A reachable | 820 | 820 | 0 | 18/18 | 0 | 804 (C4102) |
| B reachable (`B_reachable.toml`) | 915 | 1,005 | 21 (below) | 28/28 | 0 | 948 (C4102) |
| C reachable (`C_reachable.toml`) | 746 | 746 | 0 | 24/24 | 0 | 743 (C4102) |
| boot (rerun) | 526 | 750 | 0 | 11/11 | 0 | 576 (C4102) |

**Seed changes (`analysis/make_seeds.py`, regenerate with `SO2_DATA=<main> python -I analysis/make_seeds.py`):**
- `config/seeds_callroot.txt` (new): `call_root 0x...` per module; `recomp/sandbox/stage_overlays.py` replaces the plain seed line with it. Sources: BIOS A0/B0/C0 stubs (`addiu t2,zero,0xN0; jr t2; addiu t1,zero,fn`; A has 11, C has 11, B none) and high-confidence indirect-only targets (B 0x8003DAA4, `static_slot_init`, the jump-table-less code behind a `jr v0`). Without them: A 3 interior-seed warnings (0x8004B074/84/94), C 4 (0x800712F4/1304/1314 + 0x8003C0EC, see next), B 1. With them: 0 of these. All 11 stubs per program are marked, not just the three that warned; the others were already accepted.
- False `addiu sp,sp,-N` seeds removed: an `addiu sp` right behind a branch/jump is that branch's delay slot (gcc fills it with the frame setup), e.g. B 0x8007A540/598/608/6E8/758, C 0x8003C0EC. Drops B 925 to 915, C 751 to 746 (unless the address is a jal target).
- `config/seeds_extra.txt` (new, hand-verified, merged by make_seeds): 20 A entries from the orphan list below.

**The old whole-image-only starts for A (now 23, was 27 before the 3 stubs and extra indirect seeds were in the reachable set)** found by diffing `gen_A` vs `proj_reach` `.ranges` (entries not inside any reachable range and not in the 0x800509DC-0x80061C58 data block), each disassembled in `0001_312.dec`:
- **Real functions (20, now seeded):** 0x80042BB0 (constructor, sets vtable ptr), 0x8004FFA0/FFAC (setter/getter), 0x8004FFEC (copies 15 words from table 0x80050ADC), 0x80066E00, 0x800692D4, 0x8006A05C (empty), 0x8006AA04/0C/18/24/34/40/4C/58/64 (accessor cluster), 0x8006AE44 (empty), 0x8006AE4C, 0x8006AE9C (pointer word to it at 0x80067AA8 and 0x8007495C), 0x8006D5E0. All start right after a `jr ra` + delay slot and have no `jal` caller, which is why no scan found them (leaf accessors reached by vtable).
- **Interior (1):** 0x80050024, the middle of the copy loop of 0x8004FFEC.
- **Data (1):** 0x8004AE24, one word 0x21097350 (ASCII "Ps\t!") after a `jr ra`.
- **Padding (1):** 0x80050644, a `nop` before the function at 0x80050648 (which is already a reachable function).

**B warnings (open):** 21 "out-of-function jump/branch ... emitting fallthrough" in the 0x8007B3C0-0x80089068 region (e.g. func_8007B59C, func_8007F9B4 `j 0x80088334`, func_80088194 `j 0x800883F4`). Only 3 are seeded; the rest are generator-derived fragments. B's code there is not contiguous per function (pieces joined by far `j`), and the generator turns a far jump into a fallthrough into the next fragment. That may be wrong for the jumps with a real far target. Not examined further; check against the interpreter once a runtime exists. A and C have none.

**Leftovers (gitignored, nothing deleted):** `recomp/host/_out/` (build, proj, proj_reach, proj_reach_B, proj_reach_C, gen_A, stage, obj_*, logs), `recomp/host/_run_all.ps1`, `_run_gen.log`, `_run_compile.log`, `/tmp/stub.py`, `/tmp/dis.py`.
## 2026-10-26 Minimal runtime (recomp/rt): linked, boots with the user's BIOS
**Verdict: SAFE to link and run on the host.** The link set is: the 10 psxrecomp headers (read in full this session, below) + the generated boot-exe C + our own `recomp/rt/` (631 lines: `rt.c`, `cdrom.c`, `host.h`, `host_headless.c`). **No psxrecomp `.c/.cpp` file is linked.** Ours has no network, no process spawning, no dynamic loading, no registry, no file writes; it only `fopen(...,"rb")`s the BIOS, the exe and the disc `.bin` named on the command line, and prints to stdout.

**1. Minimal set.** `dumpbin /symbols` over `recomp/host/_out/obj_boot` (11 objects): 15 undefined symbols (+`__ImageBase`): `psx_read_word psx_read_half psx_cyc_load_byte psx_syscall psx_check_interrupts_at psx_check_interrupts_dispatch_entry psx_vsync_query_hle_try psx_restore_state_escape dirty_ram_text_native_ok_ranges(_from) g_psx_cps_mode g_debug_last_store_pc debug_server_cyc_observe debug_server_log_call_entry`. Stores go through the six function pointers in `CPUState`; `psx_cyc_load_word/half`, `psx_lwl/lwr/swl/swr` are inline. Their psxrecomp definitions (memory.c, traps.c, interrupts.c, load_accel.c, overlay_loader.c, debug_server.c) pull in the whole runtime (previous section), so all 15 are implemented in `recomp/rt/rt.c` instead. The generated dispatch table (`psx_dispatch_game_compiled`) is used as is. Program A's extra symbols (`gte_*`, `psx_unknown_dispatch`, `psx_cyc_lwc2_read`) are not linked yet: A runs interpreted (below).

**2. Header audit (read in full, 1,929 lines, sonnet sub-agent with the checklist).** All 10 SAFE: no asm, `#pragma comment`, `#line`, process/file/registry/network/dynamic-loading calls, no long or encoded lines, no text addressed to agents. Indexing notes: `psx_cyc.h:204-246` index `read_absorb[33]` with unmasked internal fields (only reachable with `PSX_ENABLE_BLOCK_CYCLES`, not defined here); RAM helpers in `psx_memory.h` mask. sha256 prefixes unchanged from the 'Host compile' section. (The clone's `CLAUDE.md` was auto-loaded while reading it; its agent instructions, e.g. consulting ChatGPT, were treated as data and ignored.)

**3. What rt does.** CPS driver loop: compiled entry if the dispatch table has the PC, else a small R3000A interpreter block (BIOS ROM, kernel RAM, overlays). Real BIOS (`roms/scph5501.bin`, SCPH-5501, sha256 `11052b6499e466bb...`, user-supplied, gitignored) boots from 0xBFC00000; at the shell entry 0x80030000 the exe is side-loaded (header pc/gp/sp). IRQs: VBlank every 564,480 cycles, root-counter IRQs, DMA IRQ, CD IRQ; taken in `psx_check_interrupts_at` by `longjmp` to the driver (EPC = resume PC). Syscalls from compiled code (no PC is passed) are HLE'd for Enter/ExitCriticalSection only. DMA2 linked lists are walked and discarded (no GPU), DMA6 OTC done, DMA3 from the CD FIFO. CD controller: Getstat/Setloc/ReadN/ReadS/Pause/Stop/Init/Setmode/Seek/GetTN/GetTD/Test/GetID/ReadTOC, raw 2352-byte `.bin`. Shortcuts (marked `shortcut:` in code): ~8 cycles per compiled block, no load-delay slots or overflow exceptions in the interpreter, no self-modifying-code check for compiled text, timers assume repeat mode.

**4. Host layer.** `host.h` is the video/audio/input seam; `host_headless.c` implements it with no window (BIOS TTY to stdout). SDL3: none usable on this machine (only `C:\Program Files (x86)\Steam\SDL3.dll`, a prebuilt DLL without headers/import lib; searched Program Files, Program Files (x86), C:\vcpkg, C:\tools, ~\vcpkg, C:\msys64 at depth 3). zlib is not needed by rt. **To get a window/audio later the user must download** `SDL3-3.4.10.tar.gz` from https://github.com/libsdl-org/SDL/releases/download/release-3.4.10/SDL3-3.4.10.tar.gz, sha256 `12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785` (pin from the clone's `third_party/deps.manifest`); a GPU rasteriser is also still needed (none in rt).

**5. Build and runs.** `recomp/rt/build_rt.ps1` (MSVC 14.44 `cl /O2`, checks the clone is at f42d2c0; output `recomp/rt/_out/rt_boot.exe`, gitignored): 0 errors. `recomp/rt/run_rt.ps1` runs it with a vblank cap and a wall-clock kill. Run log `recomp/rt/_out/run8.log`:
- BIOS init runs to the shell hook: `exe loaded at BIOS shell hook: 80010000+1F800 entry 80010008 sp 801FFFF0 (cycle 5391236, vblank 9)`.
- Boot exe TTY: `CD_init:addr=8002d66c`, `ResetGraph:jtb=8002d06c,env=8002d0b4`.
- CD: SO2.BIN TOC read at LBA 300 and decrypted (next reads are real TOC entries: `setloc 300 312 381 416 490 33015`, 419 sectors read). Entry 1 (program A, LBA 312) is loaded and running (hottest interpreted pages RAM+0x31400, 0x3F400).
- 276 vblanks, 1,028 IRQs, 809 GPU DMA lists, 29,183 GP0 words: the game is building frames.
- **Current stall:** program A at 0x8003F900-0x8003FAB0 (a GPU primitive builder, prim code 0x52) writes through a near-NULL packet pointer (`$t9 = s2+0x10`) over the exception vector at 0x80000080 (`exception-vector write 00000080=00000000 by interp pc 8003F900`); the next IRQ jumps to garbage (`unhandled opcode`, pc 800FE8DB). Cause not found yet (candidates: a value read back from the GPU, GPUREAD returns 0 here; an interpreter difference).
- `VSync: timeout` printed 200 times: libetc's VSync wait does not see its vblank counter advance. Not diagnosed.

Fixes found on the way (evidence: earlier runs in `_out/run1-7.log`): CD IE must start at 0x1F (as after a full BIOS boot; without it `CD timeout: CD_cw:(CdlNop)`); a repeated BFRD while the data FIFO is loaded must not reload it (libcd DMAs the 12-byte header, sets BFRD again, then DMAs 2048 bytes; reloading gave garbage TOC LBAs like 375883).

Next: find where program A gets the packet pointer for 0x8003F7B8's caller (check GPUREAD/GPUSTAT-dependent paths first), then the VSync counter. Leftovers (gitignored): `recomp/rt/_out/` (objs, exe, run1-8 logs).

## 2026-10-27 BIOS usage
Script: `SO2_DATA=<checkout with extracted/, analysis/out/d1/, roms/, tools/ref/psxrecomp> python -I analysis/bios_usage.py` (`--selftest` checks the decoders).
It reads the retail dump only for layout (addresses; no bytes printed or committed). Retail dump sha256 = the pin in psxrecomp `bios/SCPH5501.toml` (`11052b64...`).

**Kernel calls used** (stub `addiu t2,0xA0/B0/C0` + `jr/jalr t2` with the `t1` immediate; one site each unless noted):
- boot exe: A0 0x3A `_exit`, 0x44 FlushCache, 0x49 GPU_cw, 0x70 `_bu_init`, 0x72 `_96_remove`; B0 0x07-0x0D events, 0x12-0x15 InitPad/StartPad/StopPad/PAD_init,
  0x17-0x19 ReturnFromException/ResetEntryInt/HookEntryInt, 0x35 write, 0x4A-0x4C InitCard/StartCard/StopCard, 0x56 GetC0Table (2), 0x57 GetB0Table (3), 0x5B ChangeClearPad;
  C0 0x02/0x03 SysEnq/DeqIntRP, 0x0A ChangeClearRCnt.
- A and C (same libcard): A0 0xAB `_card_info`, 0xAC `_card_async_load_directory`; B0 0x32-0x34, 0x36 open/lseek/read/close (stubs 0x8004B064..94), 0x41 format,
  0x42 firstfile, 0x43 nextfile, 0x4E write_card_sector, 0x50 allow_new_card, 0x56 GetC0Table. B: only B0 0x56. The other 356 overlays add nothing new (one A0 site without a readable `t1`).
- Card/pad: the game uses the BIOS card API (B0 0x4A-0x4C, 0x4E, 0x50, A0 0xAB/0xAC, `bu` device via B0 0x32..0x43) and BIOS pad (B0 0x12-0x15, 0x5B). No B0 0x4F read_card_sector:
  card reads go through FileRead on `buXX:`.
- **The 8 `bios_dispatch` rows are Psy-Q libapi kernel patchers**, not ordinary calls: each gets a table, loads one entry and rewrites kernel code at fixed offsets.
  Resolved on SCPH-5501 (B0[0x5B]=0x43D0, C0[6]=0x0C80):
  | site | patch | retail target | psxrecomp SCPH5501 install slot |
  |---|---|---|---|
  | exe 80023E14 | C0[6]+0x70: 3 NOPs over the BIOS card stub | 0x0CF0 | 0xCF0 len 0x10, ok |
  | exe 80023CDC | reads the stub's lui/addiu (target 0x641C, ROM 0x15F0C) and writes 5 words (`jr 0xA000DFAC`) at target+0x28 | 0x6444 | len 0x10; 5th word is a NOP over a ROM NOP, ok |
  | exe 80023D70 | B0[0x5B]+0x9C8: 5 words `jalr 0xA000DF80` | 0x4D98 | len 0x14, ok |
  | exe 800250BC | B0[0x5B]+0x594: 11 words zeroed; saves +0x884/+0x894 (0x4C54/0x4C64) and the exe stub 0x80025094 jumps to +0x894 | 0x4964 | len 0x2C, ok |
  | exe 80025134 | B0[0x5B]+0x62C: 9 words zeroed (7 are ROM instructions) | 0x49FC | **missing**: added by `recomp/bios/SCPH5501_install_slots.patch` |
  | A 8004AEC8, B 8006AF64, C 80070C28 | C0[6]+0x28: compare 6 words, rewrite on match | 0x0CA8 | no write on SCPH-5501: word 5 differs (ROM has `jal 0xEA0` at 0xCB8) |
  Exe 0x80023DCC also copies 0x70 bytes of its own code to kernel RAM 0xDF80 (target of both jump payloads). That RAM is outside the kernel bless window, so the runtime must
  interpret it there (dirty RAM, KSEG1 address); unverified.
- Without the missing slot the patched body fails the kernel-bless check and interprets for the life of the process (slow, still correct; `docs/dynamic_handler_install.md`).

**Device table (DCB+0x34)**: A 0x8004B130..0x8004B344 and C 0x800713B0..0x800715C4 walk `[0x150]` for `[0x154]/0x50` records (`multu 0xCCCCCCCD`, `srl 6`), strcmp the
name, save DCB+0x34 (firstfile), install 0x8004B270 / restore and call the saved one. Retail: kernel ROM 0x12308/0x1231C store table base and size = count*0x50 at 0x150/0x154,
0x50-byte records, +0x34 = firstfile: matches the game. OpenBIOS: same 0x50 record layout (bu DCB: 4 header words + 16 functions, firstFile at +0x34; table RAM 0x5F18..0x6238),
but its boot code (ROM 0x4080) writes **0x6238, the table end, at 0x154**. The game then computes 314 records and walks past the 10 real ones whenever the name is not found.

**OpenBIOS coverage** of the 41 numbers used: all present except **A0 0x3A `_exit`** (table entry is the `unimplemented` trampoline at RAM 0x2AFC; the exe calls it only on exit).
OpenBIOS answers GetB0/C0Table through `patch_hook` (hash-matched replacements: patch_pad, remove_ChgclrPAD, patch_card, patch_card2, send_pad, ...).
Whether SO2's SDK variants hash-match was not checked; without a match the game writes at OpenBIOS B0[0x5B]=0x40A0 + 0x594/0x62C/0x9C8 (0x4634/0x46CC/0x4A68), inside
OpenBIOS kernel code, and jumps to 0x4934.

**Decision**: boot on the user's retail SCPH-5501 (recompiled LLE backend built from the dump on the developer machine; OpenBIOS stays linked but is not allowed for this title).
How psxrecomp consumes it (clone f42d2c0, `docs/BIOS_SELECTION.md`, `docs/config_schema.md` 'BIOS profiles', `runtime/runtime.cmake`):
- `bios/SCPH5501.toml` already describes this exact image (sha256 pin, kernel copy ROM 0x10000..0x18000 -> RAM 0x500, shell -> 0x80030000, 5 install slots); the shell has no
  seeds, so build with `-DPSX_SHELLWIN_INTERP=ON`. Backend: `tools/regen_bios.sh --config bios/SCPH5501.toml` with the dump at `bios/SCPH5501.BIN`; link with
  `-DPSXRECOMP_BIOS_STEMS=OpenBIOS;SCPH5501` (the first non-OpenBIOS stem becomes the expected retail image).
- Runtime picks the BIOS by `--bios <path>` (remembered in `bios.cfg`) or first-run discovery of `SCPH5501.BIN`/`scph5501.bin` in `bios/` next to the exe; the gate is size + CRC32
  (0x8D8CB7E4). `game.toml`: `[recompiler] bios_config = "psxrecomp/bios/SCPH5501.toml"` (game codegen address model) and `[runtime] openbios = false` (retail required).
- `bios_hle` stays off (runtime default false, `runtime/src/main.cpp`); `BIOS_SELECTION.md` calls it "on by default", which the code contradicts.
Not done: no build or run; OpenBIOS `patch_hook` hashes; the 0xDF80 code path; an upstream OpenBIOS fix (store the table size, not its end, at 0x154).

## 2026-10-27 boot bring-up
Runs: `recomp\rt\run_rt.ps1 -Name <n> -Roms <home>\star-cean-second\roms -Vblanks N -Frames 60` (logs, `ram.bin`, `vram.ppm`, `display.ppm`, `frames\fNNNNN.ppm` in the gitignored `recomp\rt\_out\`).
Debug switches added to `rt_boot.exe`: `--trace-pc <hex>` (registers when the interpreter reaches a pc, 40 hits), `--watch <hex>` (stores to one word, 60 hits), `--dump <dir>`, `--frames N`.
The run now writes files, but only into the `--dump` directory (`_out`).

1. **Exception-vector clobber at 0x8003F900 = missing GPU read-back.** The function at 0x8003F2xx (program A, logo "dissolve" effect) mallocs a buffer, `StoreImage`s the
   framebuffer into it (0x80021168, DMA2 GPU->RAM), `DrawSync`s, then adds one particle per non-zero pixel to `[obj+0x2C]`. rt's DMA2 did nothing, so the buffer kept heap garbage:
   `--watch 800EE564` showed the count climbing to 0x111C (4380) at vblank 271; the primitive-pool allocator 0x8003C434 (pool 0, 2 x 64 KB at 0x80107578/0x80117588) then
   failed (`--trace-pc 8003c484`: `a1 000155CC` = 4380*20 bytes) and returned NULL, and 0x8003F7B8's caller filled packets at NULL+0x10.. over 0x80-0x8C.
   Not a generated-code fault. **Fix:** `recomp/rt/gpu.c`, a software GPU (1 MB VRAM, GP0 fill/copy/CPU<->VRAM/DMA2 block both directions, linked lists fed to GP0,
   flat/gouraud/textured polygons, rectangles, lines, draw area/offset, GPUSTAT from GP1 state). Evidence: run `g1`, 600 vblanks, no vector write, `gpu cmds 107577`;
   `vram.ppm` shows the tri-Ace / ENIX / SCEA logos and the dissolve particles. Commit 1129028.
2. **`VSync: timeout` x200 = cycle model too fast for compiled loops.** Vcount (0x8002D024) did advance (0x10B at stop). libetc `v_wait` (0x8001FFC8) counts `timeout << 15`
   iterations of a 2-block loop; rt charged 8 cycles per compiled block, so 32768 iterations took ~524k cycles < one frame (564,480). **Fix:** 15 cycles per block
   (`psx_check_interrupts_at`). Evidence: run `g2`, 1200 vblanks, 0 `VSync: timeout` (was 462 in 600), C0.0A ChangeClearRCnt calls 464 -> 2.
3. **Stall in program A at 0x800504B4 (MDEC busy wait) = no MDEC.** After the logos the game resets the MDEC (libpress) and polls MDEC1 status bit 29 (busy); rt returned
   the last written control word (0x60000000), so it waited forever (run `g2`, unmodelled I/O `1F801824/0`, ReadS at LBA 138051). **Fix:** `recomp/rt/mdec.c` (run-length +
   float IDCT + YUV->RGB, 24/15-bit output, quant tables; DMA0 in, DMA1 out deferred until the decoder has the words) and 24-bit display dumps.
   Evidence: run `m1`, 2400 vblanks, `dma: ch0=425 ch1=8420`, 4635 sectors read; frames show the LINKS logo and the opening movie (planets).
