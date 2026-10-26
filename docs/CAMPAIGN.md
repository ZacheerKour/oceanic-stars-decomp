# Matching campaign: main overlay A (SO2.BIN entry 1, load 0x8002f810)

Three parallel workers, one lane each, in separate git worktrees. Orchestrator method: the `orchestrator` skill; code rules: `AGENTS.md`, "Project profile".
Headline numbers, always both: **bytes-matched** and **matched under the rules** (no `rules.py` FAIL). `MATCH-PENDING` is open work.

## Lanes
| lane | worktree | branch | band (disjoint) | list |
|---|---|---|---|---|
| 1 | `..\star-cean-wt\lane1` | `lane/1` | 0x8002F810-0x80036240 | `config/campaign/lane1.tsv` |
| 2 | `..\star-cean-wt\lane2` | `lane/2` | 0x80036240-0x8003EC54 | `config/campaign/lane2.tsv` |
| 3 | `..\star-cean-wt\lane3` | `lane/3` | 0x8003EC54-0x80075000 | `config/campaign/lane3.tsv` |

Lists come from `python -I scripts/funcscan.py` (needs the local `build/main.bin`; its output has no game bytes) and are filtered: not yet matched, 4-150 instructions, no cop2/GTE, no cop1, no jump table (`jr` non-ra), outside the data block 0x8004B440-0x8006C1D4, and either inside the splat-verified 0x8002F810-0x8003F810 or a `jal` target. Columns: `addr`, `size_bytes`, `notes`. Each list is the 12 easiest plus 6 medium functions of the band (119 eligible per band). When a worker exhausts its list it takes more from `funcscan.py` output **inside its band only**. The two known walls 0x800361CC and 0x800386FC are excluded (ANALYSIS.md 5b). A function with 0 callers may be a pointer-table entry; its boundaries are less certain than a `jal` target's.

## Worker setup (once per slot)
1. `git worktree add -b lane/N ..\star-cean-wt\laneN main` (done for N = 1..3), then `powershell -ExecutionPolicy Bypass -File scripts\prep_worktree.ps1 ..\star-cean-wt\laneN`. Docker Desktop must be running; image `so2-decomp`.
2. Check: `powershell -ExecutionPolicy Bypass -File scripts\match.ps1` prints MATCH for every row of `config/matches.tsv` (29 at the start).

## Worker loop (per function)
1. Read `AGENTS.md` first (it overrides the skills). Retail bytes: `build\main.bin` at offset `addr - 0x8002f810`. The splat asm `build\asm\main_code.s` covers only 0x0-0x10000 and is gitignored; for other ranges disassemble the bytes locally (never commit generated asm or bytes).
2. Write `src/func_<ADDR>.c` (one function per file, rules per the profile), add the row `func_<ADDR>`, `0x<ADDR>`, `<size>` (tab separated) to `config/matches.tsv`, run `scripts\match.ps1`. Callee prototypes, globals and types go in `src/so2.h` once (no local `extern`, no per-file `typedef`).
3. Byte-exact and `python -I scripts\rules.py` clean for the touched files: commit (`func_<ADDR>: match (gcc-2.7.2-psx)`). Exact but a FAIL left: keep it and report `MATCH-PENDING`.
4. Stuck: park in `src/wip/` (profile) and record the wall class and what was tried; the orchestrator copies dead ends to AGENTS.md. Move on.
5. Commit on `lane/N` only. No push, no PR. Touch only the lane's functions plus appends to `src/so2.h` and `config/matches.tsv`. Do not rename or retype an existing shared declaration; report it instead.
6. Report per function: the fix, the rule status, what is left.

`config/matches.tsv` and `src/so2.h` are append-only shared files (`merge=union` in `.gitattributes`, so lane appends merge without conflict markers). Two lanes may therefore declare the same symbol or type; the duplicate shows as an R1 FAIL in `rules.py` and usually as a compile error in the gate, and is resolved at merge (keep the first-merged lane's).

## Merge procedure (orchestrator, one lane at a time)
In the lane's worktree:
1. `git status` shows no unmerged paths. `git rebase main`. Conflicts: `matches.tsv` keep both sides' rows; `so2.h` keep both sides' declarations and drop duplicates. Confirm the lane's reported functions are still in `matches.tsv`.
2. Gate: `scripts\match.ps1` must print MATCH for every row, main's rows included (a regression of an edited function blocks the merge). Report counts only from that output.
3. Rules: `python -I scripts\rules.py` over the tree; compare with main. A function reported matched but flagged is `MATCH-PENDING`: correct the report. New findings in untouched files mean a shared-header change broke them: fix in the lane.
4. Reader-test review: read a sample of the lane's new functions, the biggest jumps first (merged variables, meaningless temporaries, invented names, dummy code, `volatile` carriers). Send fakematches back to the worker as `MATCH-PENDING`; never rewrite them at merge.
5. In the main checkout: `git merge --ff-only lane/N`, then update the two numbers in `AGENTS.md` / `ANALYSIS.md` and commit. No push without the user's go-ahead.
6. Park the slot: `git -C ..\star-cean-wt\laneN switch --detach main`, delete `lane/N` once merged, create the next lane branch from main. Re-run `prep_worktree.ps1` only if `build\main.bin` changed. Remove the worktrees with `git worktree remove` when the campaign ends.

Merge order 1, 2, 3; each later lane rebases onto the new main, which carries the earlier lanes' `so2.h` / `matches.tsv` lines.

## Later passes (orchestrator skill)
Helper recovery on `MATCH-PENDING` and walls, a structural pass, a finishing pass (move the per-file `Obj` typedefs and local externs into `so2.h` as `S_<ADDR>`; this raises "matched under the rules"). Excluded for now: switch/jump tables, `.rodata`, GTE, float.
