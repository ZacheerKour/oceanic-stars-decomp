# Analysis (2026-10-26)

Scripts: `analysis/` (iso.py ISO reader, toc.py SO2.BIN table, slz.py decompressor, extract.py, scan_*.py).
Output (gitignored): `analysis/out/` - `d1/`, `d2/` extracted entries, `manifest.tsv`, scan logs.

## 1. Discs
| | Disc 1 | Disc 2 |
|---|---|---|
| Volume | STAROCEAN2ND1, 236531 sectors | STAROCEAN2ND2, 307542 sectors |
| ISO9660 files | SCUS_944.21 (LBA 24, 131072 B), SO2.BIN (LBA 300, 483493888 B), SYSTEM.CNF | SCUS_944.22 (LBA 24, 131072 B), SYSTEM.CNF; **no SO2.BIN entry** |
| Boot exe | md5 3f6fc3bdc2471e9b1d4cb94ed76618f5 | identical bytes (only the file name / SYSTEM.CNF BOOT= differ) |

Disc 2's data is not in the ISO directory; it uses the same archive format starting at the same LBA 300 (magic present), addressed by absolute sector.

## 2. SO2.BIN archive (same format on both discs)
- Sectors 300..311 (0x6000 bytes) = encrypted TOC. Loaded by exe fn 0x80011c20; 0x1680 words copied to ctx+4.
- Layout: 0x1200 (4608) u32 slots, then 0x1200 size-low bytes. Slot i: CdlLOC BCD MSF (bytes 0-2, absolute, LBA = MSF - 150) + byte3 = size high byte; size in sectors = byte3<<8 | sizelow[i]; bytes = x2048. Slot 0 = magic 0x34829314.
- Decrypt (exe 0x80011cc4..): `a=0x13578642; for i<0x1200: w[i]^=a; a^=a<<1; sz[i]^=a&0xff; a=(~a)^0x13578642` (32-bit).
- Entries are sector-granular (no byte sizes), contiguous, no gaps/overlaps. Entry 1 is at LBA 312.

| | Disc 1 | Disc 2 |
|---|---|---|
| Non-empty entries | 4155 (max idx 4586) | 4170 (max idx 4604) |
| LBA span | 312..222881 | 312..293892 |
| Same content as disc 1, same index | - | 4136 (34 differ/new) |
| Kinds | 359 SLZ (26 multi-chunk), 3796 raw | 359 SLZ, 3811 raw |

Raw entries are mostly game data containers (counts/offset headers), plus STR video (Form1 sectors, 0x80010160 frame headers around LBA 144300+). Not parsed further.

### SLZ compression (exe decoders 0x800122b4 v1, 0x8001275c v2; runtime copy 0x8001abf8)
Header 16 B: "SLZ", ver(0 stored/1/2), u32 +4 = compressed size, u32 +8 = decompressed size, u32 +12 = offset to next chunk (0 = last; multi-chunk entries). LSB-first flag byte, 1=literal; ref = 2 B: offset=b0|(b1&15)<<8, len=(b1>>4)+3. v1 ends on offset 0. v2 stops at size and len==18 is a fill-run (`f<0x100`: byte=next, len=f+0x13; else byte=f&255, len=(f>>8)+3). Verified: output size equals header for all tested entries; 0 decode errors over all 359 disc-1 SLZ entries (first chunk).

## 3. Code overlays
Boot: exe main (0x80010650) reads entry 1 into a 512 KB heap buffer, `0x800105b0` -> SLZ-decompress to **0x8002f810** (directly after the exe image), sets sp=0x80200000 and calls it. The same fn is called from inside overlays (2-3 sites) to switch "main programs". Overlays call exe SLZ loader 0x800121a8(_, src, dst, chunk) from 22-30 sites each; seek/read via 0x80011654 / 0x80011804; load addresses seen as constants. Load address confirmed by jal-target alignment vote (350-407 hits) and literal references.

| Entry (idx, LBA, sectors) | Decompressed | Load addr | Evidence |
|---|---|---|---|
| 1 (312, 63) | 283604 | 0x8002f810 | loaded by exe 0x800105b0 |
| 2129 (24323, 98) | 381148 | 0x8002f810 | vote 366/903 |
| 2576 (30736, 82), 3100 = copy | 313340 | 0x8002f810 | vote 407/1221 |
| 2981,2985,2989,2993,2997 (17 sec, identical) | 63912 | 0x800d1b28 | vote 102; ref from idx 2576 at 0x80030af8 |
| 3001 (36227, 6) | 21269 | 0x80088000 | vote 49 |
| ~45 small modules (2986,2990,2994,2998,3004-3022,3101-3174 x~30 dupes,4191,4328) 5-72 KB | | 0x8007e000 | vote 8-53; refs from idx 2576 |
| 2131-2159 odd, 1981, 1984, 2982 and tiny 2135/2137/2157 | 0.7-66 KB | undetermined | few self-calls; candidates 0x801ee000 (ref in idx 2129), 0x801f9800 (idx 2576 ref) not matched to entries |
Full list: `analysis/out/code2_disc1.txt`. Not scanned: disc-2-only entries (34 differ), raw (non-SLZ) code. Dupes (3101..3174 etc.) are per-area copies of identical code.

## 4. Compiler
- Sony libs in exe: libc/libgpu(sys.c 1.140 1998/01/12, intr.c 1.76 1997/02)/libcd(bios.c 1.81 1996/12)/libspu, "Library Programs (c) 1993-1997". Suggests Psy-Q ~4.6/4.7 era (1998); no version string. Lib code (exe 0x80015000+) often fills `jr ra` slot with `addiu sp` (111/340), unlike game code -> libs built separately.
- Game code (exe 0x80010000-0x80014fff, all overlays): `jr ra; nop` with sp restore before (overlay 1: 679 nop vs 18 sp-in-slot), and leaf functions with an `addiu sp,-8` frame (17-22 per overlay).
- Docker test with /opt/gcc-*: 2.8.0/2.8.1 put `addu $sp` in the `j $31` slot and have no leaf -8 frames -> **ruled out**. 2.7.2-cdk has no leaf -8 frame -> ruled out. 2.6.0-psx and 2.7.2-psx both produce -8 leaf frames and plain epilogues.
- Re-compiled the exe's table-decrypt loop (0x80011ce4): 2.7.2-psx matches instruction-for-instruction (xor operand order); 2.6.0-psx swaps one xor's operands.
- **Conclusion: GCC 2.7.2 (Sony "psx" build, `/opt/gcc-2.7.2-psx`), -O2, -G0 (no gp-relative access; lui/%lo globals). Confidence ~75%; 2.6.x ~15%.** Based on 1 exact loop + epilogue style statistics, not on a full-function byte match. Next step: reconstruct and byte-match 3-4 real functions with `2.7.2-psx` + maspsx.

## 5. First byte matches (2026-10-26)
Setup: `config/main.yaml` is a splat config for the entry-1 overlay (flat file = `analysis/out/d1/0001_312.dec`, copied to gitignored `build/main.bin`; vram 0x8002f810). Code segment guessed as 0x0-0x10000 only (`python -m splat split config/main.yaml` -> `build/asm/main_code.s`, gitignored, 344 auto-named funcs, no symbol work; needs `pip install --user "splat64[mips]"`, spimdisasm was missing). Compare: `scripts\match.ps1 [gcc-dir]` (Docker; `config/matches.tsv` lists name/vram/size; each `src/<name>.c` -> cc1 `-O2 -G0` -> maspsx `--aspsx-version=2.77` + GNU as -> ld at the retail address with globals fixed in `config/globals.ld` -> byte compare with retail incl. relocated operands).

Byte-identical to retail with **gcc-2.7.2-psx -O2 -G0** (script output "MATCH"): 0x80032488 (16 B), 0x80032134 (24), 0x80032308 (24), 0x80033FB0 (16), 0x80034390 (16), 0x800343A0 (24), 0x80032984 (44), 0x80032950 (52, needed `q=a/4; rem=a-q*4` to get the register allocation), 0x8003093C (56, loop; matched only as `if (mask) { for ... }` with unsigned mask), 0x80032918 (56). All leaf functions; 4 are trivial 1-2 store setters (weak evidence). Sizes are my own instruction counts from splat's boundaries.

Other compilers on the same 10:
| Compiler | Result |
|---|---|
| 2.7.2-psx | 10/10 |
| 2.6.0-psx | 9/10; 0x80032950 differs (3 bytes, register choice) |
| 2.7.2-cdk, 2.8.0-psx, 2.8.1-psx | 5-6/10; fail the global-store setters (0x80032134, 0x80033FB0, 0x80034390, 0x800343A0) and, for 2.8.x, the loop at 0x8003093C |

Confidence: **GCC 2.7.2-psx -O2 -G0 now ~90%** (only 2.6.0-psx remains close, ~8%). Still unproven: assembler version (ASPSX 2.77 assumed; no function here is sensitive to it), and anything with calls/stack frames/switches/data - all matched functions are frameless leaves. Next: match functions with a stack frame and calls to see saved-register/prologue behaviour.

### 5b. Functions with frames, calls and loops (2026-10-26)
19 more functions byte-match with **gcc-2.7.2-psx -O2 -G0** (29 total in `config/matches.tsv`; `src/so2.h` holds the shared extern declarations, `extern`/`func_<ADDR>` symbols resolve to their address at link time). All have `addiu sp` frames with saved ra (and s0/s1), `jal` calls, branches or loops: 0x80030CBC, 0x80030D24, 0x80031214 (8-iteration loop), 0x80031C58, 0x8003244C (recursive), 0x800344D0, 0x80034534, 0x80034584, 0x800345B8, 0x80034600 (loop), 0x800346C4, 0x80034714 (loop), 0x80034FB4, 0x80034FF8, 0x8003544C, 0x80035AFC, 0x80035FDC, 0x8003823C, 0x80038DBC. 12-124 bytes.

| Compiler | Result on the 29 |
|---|---|
| 2.7.2-psx | 29/29 |
| 2.6.0-psx | 24/29. Fails 0x80032950 (register), 0x80031214 (loop shape, 34 bytes), 0x80034714 (18 bytes), 0x80035AFC (reloads the stored halfword with `lhu` after `sh`; retail keeps it in a register), 0x80038DBC (swaps `addu` operand order) |

Not tried on the new set: 2.7.2-cdk, 2.8.x (already ruled out in section 4).

So 2.7.2-psx still fits once frames/calls/loops are involved, and four functions are matched only by 2.7.2 (0x80031214, 0x80034714, 0x80035AFC, 0x80038DBC) plus 0x80032950 from before. That is the evidence separating it from 2.6.0. Confidence 2.7.2-psx: ~95%; 2.6.0 ~4%. Unproven: assembler version (still no sensitivity seen), switch tables, `.data`/`.rodata`, functions with s2+ or larger frames.

Source-level findings: plain `char` is **unsigned** in this gcc (retail `lb` needs `signed char`); `-G0` globals via `extern T D_<ADDR>;`; unused callee arguments pass through untouched registers (declare callees K&R `()`, pass the real arguments only). Loop pitfalls: 0x80034714 needed `r = 1; if (n != 0) { for ... } else r = 0;` for block order; 0x80031214 needed indexing `p[i * 0x30]` (a pointer-bump loop makes gcc count down).

Not matched (dead ends, source not kept):
- 0x800361CC (116 B, search loop over 12-byte records): registers and instructions match, but retail lays out the match body before the `i++; j top` block; every spelling tried (`for(;;)`+return, `while`, `if (!=) {i++; continue;}`, `for(i=0;;i++)`) puts the `i++` block first (112 B vs 116 B) or rotates the loop. Wall class: control-flow/block layout.
- 0x800386FC (100 B): retail test-at-top loop `while (i < D) f(++i)` (not rotated, `j top`); gcc 2.7.2 here always rotates it (`blez` guard + bottom test), also with `for(;;){if(..)break;}`, `continue`, and a `volatile` bound (the latter was an experiment only, not kept). Wall class: control-flow; the retail unrotated loops (these two) might need a different source construct or flag, undetermined.

## 6. Overlay map (2026-10-26)
Regenerate: `python -I analysis/overlay_map.py [-v]` (reads both `.bin` images, ~20 s) -> `config/overlays.tsv` (entry, disc, chunk, load_addr, size, md5 of the decompressed chunk, role, confidence, basis, identical entries, loader; no game bytes). This section supersedes the section 3 table.

**Census** (non-empty entries): disc 1 = 359 SLZ (26 multi-chunk) + 3796 raw; disc 2 = same 359 SLZ + 3811 raw. Raw: ~2490 offset-table containers (first u32 = byte offset of first item), ~1255 count-table containers, 20/32 entries that start with an STR frame header (0x80010160; disc 2 has 12 more), 30-34 unclassified. **No raw (non-SLZ) entry on either disc is MIPS code** (screen: >=3 `jr ra` + >=3 frame prologues + 80% valid opcodes). All code is SLZ: 86 entries / 141 code chunks per disc. **Disc 2**: all 86 code entries and every chunk are byte-identical to disc 1 (same md5); the 34 differing/new entries are 22 STR + 12 other raw data, no code. Boot exe SCUS_944.21 is identical on both.

**How addresses were found.** (a) Loader call sites: exe 0x800121a8 `(handle, src, dst, chunk)` is called with a literal `dst` (lui+addiu) for fixed-address overlays; the "main-switch" 0x800105b0 decompresses to 0x8002f810 and jumps. (b) Heap-steer idiom: `do { p = alloc(N) } while (p != CONST)` then decompress to `p` -> fixed address in the heap (2129@0x80045ae4, 2576@0x80055098/0x80055114). (c) Independent check on every chunk: jal targets inside the image must be function starts (prologue or after `jr ra`) and `j` targets must lie inside; script prints hit/in-range. (d) Entry indices are `addiu rX,zero,N` literals (e.g. 0xc1f) or u16 tables.

| Entries | Load addr | Loader / evidence | Conf |
|---|---|---|---|
| boot exe | 0x80010000 | PS-X header | high |
| 1 | 0x8002f810 | exe 0x80010650 -> 0x800105b0 | high |
| 2129, 2576 (=3100 copy), 3107 | 0x8002f810 | main-switch 0x800105b0 (jump target); 2576/entry 1 reference 2129/2576/3100 as literals; 3107 is loaded into heap var 0x800756f4 and passed at 2576@0x800682f4. Vote 521/551, 514/582, 607/637 (3107: 26/38) | high (3107 medium) |
| 2981,2985,2989,2993,2997 (identical) | 0x800d1b28 | 2576 area-group loader 0x80030a94: group base 2980+4n reads 4 entries; part 0 and 3 -> heap, **part 1 -> 0x800d1b28**, **part 2 -> 0x8007e000** (literals at 2576@0x80030b14/0x80030b38) | high |
| 2982,2986,2990,2994,2998 | 0x8007e000 | same loader (part 2); vote 5/5..43/43 | high |
| 3001 | 0x80088000 | area modules (2982/2986/2990/2994/3008/3010) load group 3000, part 1 -> literal 0x80088000 (e.g. 2986@0x8007e3e4) | high |
| 3004-3022 even, 3101, 3102, 3112-3174 even (x32 copies of 3101), 4191/4328/4332 (=3102) | 0x8007e000 | jal/function-start vote only (13-60 hits, 0 misses) + same slot as above; referenced by index literals in 2576/2129/entry 1; the specific call site was not traced | high by vote, loader unconfirmed |
| 1984 chunk 0 (65880 B) | 0x800715d0 | 2129@0x80045a3c literal, chunk 0 (index 1984 = 0x7c0 at 0x80045a58); vote 34/34 | high |
| 1984 chunk 1 (15856 B) | 0x80081728 | 2129@0x8004622c literal, chunk 1; = end of chunk 0 (0x800715d0+65880) | high |
| 1981 (11068 B) | 0x80085518 | 2129@0x800468d4 literal, chunk 0; = end of 1984/1 | high |
| 1984 chunks 2-10 (328-1688 B, 9 variants) | 0x8008c1f4 | 2129@0x80071c7c: scene switch picks chunk 2..10 (`a3=case+1`), literal 0x8008c1f4; all `j` targets fall inside the image at that address | medium |
| 2131-2159 odd (15 entries, 4 chunks each; 2133=2131, 2145=2143, 2149=2147) | chunk k at 0x801ee000 + 0x4000*k | 4 chunks are the same code relocated four times (lui/jal/j diffs of +0x4000: `jal 0x801ee05c`, `0x801f205c`, `0x801f605c`, `0x801fa05c`); 2129@0x80045ae4 steers the heap to 0x801ee000. Even partner entry (2130..2158) is the data half, listed in the u16 table at 2129+0x5569c. Large ones vote 2-5/2-5, tiny ones (2135, 2137, 2141, 2157: 0.7-1.3 KB) only the relocation + `j`-in-range check | high (large), medium (tiny) |
| 3103 (85292 B) | 0x801e9000 | 2576@0x80055098 steers heap to 0x801e9000, decompresses buffer var 0x800756bc (loaded from imm 0xc1f at 2576@0x8004b124); vote 20/21 | high |
| 3104, 3105, 3106, 3108, 3109 (1.2-3.4 KB) | 0x801f9800 | 2576@0x80055114 steers heap to 0x801f9800 (var 0x800756c4; u16 table 3109,3108,3106,3104 at 2576+0x44cc6; 3105 via register). 3105/3108/3109 vote-confirmed, 3104/3106 only slot-consistent | high (3105,3108,3109), medium (3104,3106) |

Role notes: 2129 is the battle program (owns the 1977-1984 and 2130-2158 loaders; 1984 loaded with 1977-1983 data entries), 2576/3100 the field program (area-group loader), entry 1 the boot program that loads 3100. The 2129 and 2576 images keep a default occupant in the 0x800715d0.. range (their tail), which the battle/area overlays replace. Dupes (identical decompressed bytes): 2576=3100; 2981=2985=2989=2993=2997; 3101=3112=..=3174 (even, 32); 3102=4191=4328=4332; 2131=2133, 2143=2145, 2147=2149. Everything else is unique (48 distinct 2131-family chunks).

**Still unknown / low confidence**: (1) the call sites that select 3004-3022, 3101/3102 and their copies (only the slot is known); (2) which scene value picks each 1984 chunk 2-10 and which exact enemy set picks each 2131-family entry (index = table value, code = +1 inferred from layout, not traced); (3) the other literal destinations seen in 2129 (0x8006ef88, 0x80088110, 0x80088900, 0x80089f18, 0x8008b1f0) belong to non-overlay data loads (script-driven); (4) 1979/1980 and similar SLZ entries are data (density 0.3-0.5), not classified further; (5) addresses were checked statically only, no run-time confirmation.
