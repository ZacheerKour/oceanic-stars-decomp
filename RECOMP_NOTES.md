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
- Analyzer (sandbox run 2, program A, seeded, run with the old seeds): 232/235 matched functions are analyzer functions of equal size. The 3 others follow this pattern: `func_800344D0` (analyzer 4 B + 0x800344D4, see 'Indirect call resolution'), `func_800378CC` (4 B + 0x800378D0), `func_8003B284` (36 B + 0x8003B2A8). The last two splits came from our old seeds; a rerun with the new seeds should leave only 0x800344D4 (the analyzer's own pre-prologue heuristic). Not rerun (needs the sandbox).
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
