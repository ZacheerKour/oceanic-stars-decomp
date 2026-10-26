cd /work/analysis/cc
for g in 2.6.0-psx 2.7.2-psx 2.7.2-cdk 2.8.0-psx 2.8.1-psx; do /opt/gcc-$g/cc1 -quiet -O2 -G0 -mgas u.c -o u.$g.s; grep -v -E '^\s*#|^\s*\.(frame|mask|fmask|set|align|ent|end|type|size|globl)' u.$g.s > u.$g.txt; done
for p in "2.7.2-psx 2.8.0-psx" "2.8.0-psx 2.8.1-psx" "2.7.2-psx 2.7.2-cdk"; do set -- $p; echo "## $1 vs $2"; diff u.$1.txt u.$2.txt|head -40; done
