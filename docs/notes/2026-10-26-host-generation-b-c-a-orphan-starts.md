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
