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

