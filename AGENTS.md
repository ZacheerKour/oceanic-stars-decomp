# AGENTS.md - project guide

Goal: byte-matching decomposition of Star Ocean: The Second Story (USA, PS1), then a native PC port
(`/native-platform-porting`). Method skills: https://github.com/lemiur/Game-Decompilation-Skill-For-Claude.
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
1. Place the unplaced overlays (ANALYSIS.md) and pick the first overlay to split with splat.
2. DONE 2026-10-26 (ANALYSIS.md section 5): 10 leaf functions match with gcc-2.7.2-psx -O2 -G0 via `scripts\match.ps1`. Next: widen the splat segment beyond 0x10000.
3. DONE 2026-10-26 (ANALYSIS.md section 5b): +19 functions with frames/calls/loops match with 2.7.2-psx (29/29; 2.6.0-psx 24/29). Two non-rotated loops (0x800361CC, 0x800386FC) unmatched. Next: switch tables, larger frames, `.rodata`.

