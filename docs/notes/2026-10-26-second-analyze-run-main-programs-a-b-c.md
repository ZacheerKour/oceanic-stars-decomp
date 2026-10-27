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

