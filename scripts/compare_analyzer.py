"""Compare psxrecomp-analyze output for main program A (SO2.BIN entry 1) with our matches and seeds.
Run: python -I scripts/compare_analyzer.py [sandbox_out_dir]   (default recomp/sandbox/_out; reads <dir>/A/{seeded,exact}/functions.tsv)
Prints addresses/sizes/counts only. Exit 1 if any matched function is missing or has a different size in the seeded run."""
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
out = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'recomp/sandbox/_out_run2'

def rows(p):
    ls = [l.rstrip('\n').split('\t') for l in p.read_text().splitlines() if l.strip()]
    return {int(r[0], 16): int(r[1]) for r in ls[1:]}  # header: addr size name ...

matches = {}
for l in (root / 'config/matches.tsv').read_text().splitlines():
    f = l.split('\t')
    if f and not l.startswith('#') and len(f) >= 3:
        matches[f[0]] = (int(f[1], 16), int(f[2]))
seeds, cur = set(), None
for l in (root / 'config/seeds_overlays.txt').read_text().splitlines():
    if l.startswith('# module '): cur = l.split()[2]
    elif cur == 'e1c0' and l.strip() and not l.startswith('#'): seeds.add(int(l, 16))

bad = 0
for mode in ('seeded', 'exact'):
    fn = rows(out / 'A' / mode / 'functions.tsv')
    print(f'== A {mode}: {len(fn)} functions; matches: {len(matches)}')
    ok = 0
    for name, (addr, size) in sorted(matches.items(), key=lambda kv: kv[1][0]):
        got = fn.get(addr)
        if got == size: ok += 1
        elif mode == 'seeded':
            bad += 1; print(f'  MISMATCH {name} {addr:08X}: ' + ('not a function start' if got is None else f'analyzer size {got}, matched size {size}'))
        else:
            print(f'  {name} {addr:08X}: ' + ('not found' if got is None else f'size {got} vs {size}'))
    print(f'  matched functions found with equal size: {ok}/{len(matches)}')
    a = set(fn)
    print(f'  our {len(seeds)} seeds: {len(seeds & a)} are analyzer function starts, {len(seeds - a)} are not; analyzer-only starts: {len(a - seeds)}')
sys.exit(1 if bad else 0)
