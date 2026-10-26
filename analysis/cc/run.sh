set -e
cd /work/analysis/cc
for g in 2.6.0-psx 2.7.2-psx 2.7.2-cdk 2.8.0-psx 2.8.1-psx; do
  for o in -O2; do
  /opt/gcc-$g/cc1 -quiet $o -G0 -mgas -mgpOPT t.c -o t.$g.s 2>/dev/null || /opt/gcc-$g/cc1 -quiet $o t.c -o t.$g.s
  done
  echo "$g $(grep -c . t.$g.s)"
done
