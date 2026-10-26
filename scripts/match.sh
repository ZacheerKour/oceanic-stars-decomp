#!/bin/bash
# Runs inside the so2-decomp container (cwd /work). Usage: scripts/match.sh [gcc-dir-name]  e.g. gcc-2.7.2-psx
# Compiles each src/<name>.c listed in config/matches.tsv, links it at its retail address and compares the
# bytes with the retail overlay (build/main.bin, made by splat from analysis/out). Prints MATCH/DIFF per function.
GCC=${1:-${GCC:-gcc-2.7.2-psx}}; CFLAGS=${CFLAGS:--O2 -G0}
ASV=${ASV:-2.77}
BASE=0x8002f810; B=build/obj/$GCC; mkdir -p $B; rc=0
while IFS=$'\t' read -r name vram size _; do
  case "$name" in ''|\#*) continue;; esac
  cpp -P -Isrc src/$name.c 2>$B/$name.cpperr | /opt/$GCC/cc1 $CFLAGS -quiet -o $B/$name.s 2>$B/$name.err || { echo "$name: COMPILE-FAIL ($GCC)"; rc=1; continue; }
  python3 tools/maspsx/maspsx.py --aspsx-version=$ASV --run-assembler -march=r3000 -mabi=32 -o $B/$name.o < $B/$name.s || { echo "$name: ASM-FAIL"; rc=1; continue; }
  mipsel-linux-gnu-objcopy --set-section-alignment .text=4 $B/$name.o  # gas pads .text to 16; retail addresses are only 4-aligned
  printf "SECTIONS { . = $vram; .text : SUBALIGN(1) { *(.text) } }\n" > $B/$name.lds
  mipsel-linux-gnu-nm -u $B/$name.o | sed -nE 's/^ *U ((func|D)_([0-9A-Fa-f]{8}))$/\1 = 0x\3;/p' > $B/$name.syms.ld  # extern func_/D_<ADDR> resolve to their address
  mipsel-linux-gnu-ld -T $B/$name.lds -T config/globals.ld -T $B/$name.syms.ld -e 0 -o $B/$name.elf $B/$name.o 2>$B/$name.lderr || { echo "$name: LINK-FAIL"; rc=1; continue; }
  mipsel-linux-gnu-objcopy -O binary -j .text $B/$name.elf $B/$name.bin
  python3 - "$name" "$vram" "$size" "$GCC" "$B/$name.bin" <<'EOF' || rc=1
import sys
n,v,s,g,f=sys.argv[1:]; off=int(v,16)-0x8002f810; s=int(s,0)
ref=open("build/main.bin","rb").read()[off:off+s]; got=open(f,"rb").read(); pad=got[s:]; got=got[:s]; padok=not any(pad)
if got==ref and padok: print(f"{n} {v} {s}B {g}: MATCH"); sys.exit(0)
d=[i for i in range(max(len(got),len(ref))) if got[i:i+1]!=ref[i:i+1]] or [s]
print(f"{n} {v} {s}B {g}: DIFF ours={len(got)}B, {len(d)} differing bytes, first at +{d[0]:#x}"); sys.exit(1)
EOF
done < ${TSV:-config/matches.tsv}
exit $rc