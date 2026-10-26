# AGENTS.md - project guide

Goal: a decomp AND recomp project for Star Ocean: The Second Story (USA, PS1). Decomp track: byte-matching source.
Recomp track: native PC port via psxrecomp (see RECOMP_NOTES.md, recomp/), fed by the decomp's symbols, overlay map and
seeds. The decomp does not have to reach 100% before the recomp can progress. Method skills: https://github.com/lemiur/Game-Decompilation-Skill-For-Claude.
This file overrides the skills' defaults.

## Project adapter
- Build/compare environment: Docker image `so2-decomp` (`docker\run.ps1`, project mounted at `/work`).
- Compilers: `/opt/gcc-2.6.0-psx`, `2.7.2-psx` (best current guess), `2.7.2-cdk`, `2.8.0-psx`, `2.8.1-psx`; assembler via
  `tools\maspsx` + mipsel binutils. ASPSX not installed.
- Diff: `tools\objdiff`. Splitting: splat64. Permuter: `tools\decomp-permuter`.
- Retail binaries: `roms\` and `analysis\out\` (gitignored). Never commit game data or code generated from it.

## Rules
- Record claims with evidence (log line, hash, commit). Record failed approaches under Dead ends here.
- Only claim a function matches after an objdiff/byte comparison against the retail bytes.

## Dead ends
- GCC 2.8.x and 2.7.2-cdk do not fit the game's leaf-function shape (ANALYSIS.md section 4).

## Next steps
0. recomp track (RECOMP_NOTES.md, recomp/README.md): resolve the `TODO(verify)` values in `recomp/game.toml`, run psxrecomp `analyze` on the boot exe, then decide how to get SLZ overlays into its AOT pipeline (captures file vs. `slz_members` fork). Tests: `python -I tests/slz/run_tests.py`.
1. Place the unplaced overlays (ANALYSIS.md) and pick the first overlay to split with splat.
2. DONE 2026-10-26 (ANALYSIS.md section 5): 10 leaf functions match with gcc-2.7.2-psx -O2 -G0 via `scripts\match.ps1`. Next: widen the splat segment beyond 0x10000.
3. DONE 2026-10-26 (ANALYSIS.md section 5b): +19 functions with frames/calls/loops match with 2.7.2-psx (29/29; 2.6.0-psx 24/29). Two non-rotated loops (0x800361CC, 0x800386FC) unmatched. Next: switch tables, larger frames, `.rodata`.


## Project profile (decomp-code-rules, pinned 2026-10-26)
Applies to everything under `src/`. These are the project's rulings; workers do not pick their own style.
- **Compiler / flags:** gcc-2.7.2-psx `cc1 -O2 -G0` after host `cpp -P -Isrc`, then maspsx (`--aspsx-version=2.77`) + GNU as, linked at the retail address (`scripts/match.sh`).
- **Dialect (what 2.7.2-psx accepts; tested 2026-10-26):** C89. Declarations only at the top of a block (`int x=a; x++; int y=x;` is a parse error). `//` comments only work because the host cpp strips them: write `/* */`. `inline`, `long long`, VLAs, `?:` and `__asm__` are accepted but not used (asm is banned by R18). Plain `char` is **unsigned** (retail `lb` needs `signed char`; `c<0` on plain char warns "always 0"). `-G0`: globals are `extern T D_<ADDR>;`, never gp-relative. A callee that ignores some argument registers is declared K&R `()` (`int f();`) and called with only the real arguments, so the untouched registers pass through. Loop shape depends on the source: gcc 2.7.2 rotates `for`/`while` (bottom test), `p[i*0x30]` indexing and pointer bumping compile differently (ANALYSIS.md 5b).
- **Naming / layout ruling:** id Software defaults (Quake III table in `decomp-code-rules/references/style-defaults.md`): K&R braces with spaces inside parentheses (`if ( x ) {`), C89 locals aligned at the top of the block, `/* */` banners (`=====`, name, description). Exceptions: functions stay `func_<ADDR>` (8 upper-case hex digits), globals `D_<ADDR>`, file `src/func_<ADDR>.c` holds exactly that function. **No invented names without evidence** (string, SDK symbol, console variable); a name needs a one-line evidence comment. Struct types are declared once in a header under `src/` (today `src/so2.h`) as `S_<ADDR>` (address of the first function that proves the layout); members `<t><hexoff>` (t = b byte, h half, w word, p pointer, f float: `w10`, `p8`); unknown gaps `pad<hexoff>[n]`. Named constants only with evidence; otherwise hex literals.
- **Placeholder patterns (open naming work, not a failed match):** `func_[0-9A-F]{8}`, `D_[0-9A-F]{8}`, `S_<ADDR>`, `<t><hexoff>` members, `pad*`.
- **`rules` command:** `python -I scripts/rules.py [files]` (default `src/*.c src/*.h`, not `src/wip`). FAIL = [match] rule: R1 declarations only in headers and once, R2 types only in headers, R4 no offset or byte-pointer casts and no absolute-address literals, R7 naming pattern, R12 one function per file named after it, R17 no `#pragma`, R18 no asm. WARN = [free] hygiene: R23 line over 120 columns, R26 `pad` members, `volatile`, no header included. Exit code 1 on any FAIL. It cannot see dummy temporaries, merged variables or invented names: the orchestrator reads a sample at merge.
- **Gate:** `powershell -ExecutionPolicy Bypass -File scripts\match.ps1` (Docker image `so2-decomp`); every row of `config/matches.tsv` must print MATCH. In a git worktree run `scripts\prep_worktree.ps1 <dir>` once first (copies the gitignored `build\main.bin` and `tools\maspsx`).
- **Line information: none.** The overlay is a flat binary without debug sections and the compile has no `-g`, so layout and format edits are [free]; `__LINE__` / `__FILE__` stay [checked]. Re-check if a function turns out to embed assert strings.
- **Parking unfinished functions:** `src/wip/func_<ADDR>.c` (best state, first-line comment with the differing-byte count and wall class). Not in `config/matches.tsv`, not compiled by the gate, not checked by `rules.py`, not counted. Move to `src/` and `matches.tsv` only when byte-exact.
- **Counting:** report two numbers. *Bytes-matched* = rows of `config/matches.tsv` printing MATCH. *Matched under the rules* = those whose `src/*.c` has no `rules.py` FAIL. Baseline after wave 6 (func_8002F8E4 dropped as interior of 8002F8E0): 378 bytes-matched, 375 under the rules. MATCH-PENDING (exact, failing a [match] rule or a cast/layout trick): func_8003B5DC (R4 byte-pointer cast), func_80031324 (offset cast `(int)o + k`, not seen by rules.py), func_80030D64 (reads `p[32]` past `b0` into `b20`, layout trick). func_80073AA0 indexes `pad0[]` by the vtable delta (real this-adjustment, watch for R4 review) (gate and rules.py 0 FAIL) (16 files keep per-file `Obj` typedefs, local externs or byte-pointer casts: cleanup work, not lost matches). Exact but failing a [match] rule = `MATCH-PENDING`.
- **Campaign:** `docs/CAMPAIGN.md`; lane lists `config/campaign/lane{1,2,3}.tsv`; function inventory `python -I scripts/funcscan.py`.
