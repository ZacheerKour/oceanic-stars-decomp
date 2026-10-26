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
