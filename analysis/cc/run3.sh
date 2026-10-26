cd /work/analysis/cc
for g in 2.6.0-psx 2.7.2-psx 2.7.2-cdk 2.8.0-psx 2.8.1-psx; do /opt/gcc-$g/cc1 -quiet -O2 -G0 -mgas v.c -o v.$g.s; mipsel-linux-gnu-as -march=r3000 -mips1 -o v.$g.o v.$g.s 2>&1 | head -2; mipsel-linux-gnu-objdump -d v.$g.o | awk -F'\t' '{print $3}' > v.$g.d; wc -l v.$g.d; done
for p in "2.7.2-psx 2.8.0-psx" "2.7.2-psx 2.7.2-cdk" "2.8.0-psx 2.8.1-psx" "2.6.0-psx 2.7.2-psx"; do set -- $p; echo "## $1 vs $2: $(diff v.$1.d v.$2.d | grep -c '^[<>]')"; done
