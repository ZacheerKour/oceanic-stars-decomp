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


## Priority (user, 2026-10-26)
The top goal is a fully playable native port as soon as possible; the decomp serves it. Do not pause between jobs. Valid reasons to stop: usage limits, context ceiling (hand off), or the user must supply/download something (e.g. SDL3/zlib sources). No Sony BIOS is needed: use OpenBIOS built from source or audited, or HLE BIOS calls. Do not delete files, branches or worktrees; list them in CLEANUP_LATER.md instead.

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
- **Per-row compiler (PsyQ library code):** `config/matches.tsv` may carry a 4th column naming another compiler for that row. func_8004AFD4 matches only with gcc-2.8.0-psx and func_8004B374 (strcmp) only with gcc-2.7.2-cdk, so the SDK library objects were probably built with a different gcc than the game code; game code stays on 2.7.2-psx. func_8004B374 keeps a `(unsigned char)` value cast (reader may call it pending).
- **Counting:** report two numbers. *Bytes-matched* = rows of `config/matches.tsv` printing MATCH. *Matched under the rules* = those whose `src/*.c` has no `rules.py` FAIL. Baseline after wave 6 (func_8002F8E4 dropped as interior of 8002F8E0): 460 bytes-matched, 457 under the rules. MATCH-PENDING (exact, failing a [match] rule or a cast/layout trick): func_8003B5DC (R4 byte-pointer cast), func_80049060 and func_80049248 (one address under two names, `D_8004ECB0[16]` / `D_8004ECD0`). Cleared in wave 9: func_80031324, func_80030D64, func_8006F628. func_8003214C and func_80074008 cleared in wave 10 (S_80030CBC split into the state object and the resource object S_80032270).
- **Campaign:** `docs/CAMPAIGN.md`; lane lists `config/campaign/lane{1,2,3}.tsv`; function inventory `python -I scripts/funcscan.py`.

<!-- BEGIN BEADS INTEGRATION v:1 profile:minimal hash:970c3bf2 -->
## Beads Issue Tracker

This project uses **bd (beads)** for issue tracking. Run `bd prime` to see full workflow context and commands.

### Quick Reference

```bash
bd ready              # Find available work
bd show <id>          # View issue details
bd update <id> --claim  # Claim work
bd close <id>         # Complete work
```

### Rules

- Use `bd` for ALL task tracking — do NOT use TodoWrite, TaskCreate, or markdown TODO lists
- Run `bd prime` for detailed command reference and session close protocol
- Use `bd remember` for persistent knowledge — do NOT use MEMORY.md files

**Architecture in one line:** issues live in a local Dolt DB; sync uses `refs/dolt/data` on your git remote; `.beads/issues.jsonl` is a passive export. See https://github.com/gastownhall/beads/blob/main/docs/SYNC_CONCEPTS.md for details and anti-patterns.

## Agent Context Profiles

The managed Beads block is task-tracking guidance, not permission to override repository, user, or orchestrator instructions.

- **Conservative (default)**: Use `bd` for task tracking. Do not run git commits, git pushes, or Dolt remote sync unless explicitly asked. At handoff, report changed files, validation, and suggested next commands.
- **Minimal**: Keep tool instruction files as pointers to `bd prime`; use the same conservative git policy unless active instructions say otherwise.
- **Team-maintainer**: Only when the repository explicitly opts in, agents may close beads, run quality gates, commit, and push as part of session close. A current "do not commit" or "do not push" instruction still wins.

## Session Completion

This protocol applies when ending a Beads implementation workflow. It is subordinate to explicit user, repository, and orchestrator instructions.

1. **File issues for remaining work** - Create beads for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **Handle git/sync by active profile**:
   ```bash
   # Conservative/minimal/default: report status and proposed commands; wait for approval.
   git status

   # Team-maintainer opt-in only, unless current instructions forbid it:
   git pull --rebase
   bd dolt push
   git push
   git status
   ```
5. **Hand off** - Summarize changes, validation, issue status, and any blocked sync/commit/push step

**Critical rules:**
- Explicit user or orchestrator instructions override this Beads block.
- Do not commit or push without clear authority from the active profile or the current user request.
- If a required sync or push is blocked, stop and report the exact command and error.
<!-- END BEADS INTEGRATION -->

<!-- BEGIN BEADS CODEX SETUP: generated by bd setup codex -->
## Beads Issue Tracker

Use Beads (`bd`) for durable task tracking in repositories that include it. Use the `beads` skill at `.agents/skills/beads/SKILL.md` (project install) or `~/.agents/skills/beads/SKILL.md` (global install) for Beads workflow guidance, then use the `bd` CLI for issue operations.

### Quick Reference

```bash
bd ready                # Find available work
bd show <id>            # View issue details
bd update <id> --claim  # Claim work
bd close <id>           # Complete work
bd prime                # Refresh Beads context
```

### Rules

- Use `bd` for all task tracking; do not create markdown TODO lists.
- Run `bd prime` when Beads context is missing or stale. Codex 0.129.0+ can load Beads context automatically through native hooks; use `/hooks` to inspect or toggle them.
- Keep persistent project memory in Beads via `bd remember`; do not create ad hoc memory files.

**Architecture in one line:** issues live in a local Dolt DB; sync uses `refs/dolt/data` on your git remote; `.beads/issues.jsonl` is a passive export. See https://github.com/gastownhall/beads/blob/main/docs/SYNC_CONCEPTS.md for details and anti-patterns.
<!-- END BEADS CODEX SETUP -->
