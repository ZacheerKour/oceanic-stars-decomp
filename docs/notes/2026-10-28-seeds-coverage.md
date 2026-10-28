## 2026-10-28 seeds_final coverage (bead star-cean-second-9kl)
**Result: `config/seeds_final` covers all 89 SLZ overlay chunks; no gaps found, no seeds added.**
- Chunk universe = `SLZ/<name>` files in `recomp/aot/overlays.json` images (14 images, 89 unique chunks) = the 89 `# module` headers in `config/seeds_overlays.txt`.
- `config/seeds_final/` holds 89 `E*.txt` files (+ `report.tsv`): missing 0, extra 0, empty 0.
- 5379 seeds total; all inside their chunk's [load, load+size) range, all 4-aligned (0 violations).
- Every address in `seeds_overlays.txt` per module is in the matching final file (ours-not-in-final = 0).
- Chunks with <5 seeds (all tiny, 328-3420 bytes): E1984C10, C2-C9 (1984 family), E3104C0, E3105C0, E3106C0. Plausible single-function stubs; not proven.
- Lowest density (bytes/seed): E3103C0 2132 (40 seeds/85 KB), E3105C0 1710, E3107C0 1339. E3103C0/E3107C0 already pass the AOT audit with 0 TODOs (2026-10-27 note), so data-heavy rather than missed.
- **Not verified here:** that seeds reach every real function (no discs run; coverage = presence/range/superset only). The 81 chunks beyond the 8 pipeline-audited ones still need a `run_static_all.py --mode final` run (open item of 2026-10-27-overlay-seeds.md).
- Method: ad-hoc python over the files above (no game bytes read).
