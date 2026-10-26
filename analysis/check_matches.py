"""Cross-check config/matches.tsv against config/symbols.toml (confirmed, name, size, no symbol inside a matched body), src/<name>.c
and the e1c0 (program A) section of config/seeds_overlays.txt: a new matches.tsv row fails here until make_seeds.py has run.
    python -I analysis/check_matches.py [analyzer functions.tsv]   (optional: psxrecomp-analyze output for program A, seeded run)
No game data needed. Prints addresses/sizes only; exit 1 on any problem (an analyzer size difference alone is reported, not fatal)."""
import re, sys, tomllib
from pathlib import Path

root = Path(__file__).resolve().parent.parent
A_LOAD, A_SIZE = 0x8002F810, 283604   # config/overlays.json e1c0
m = {}
for l in (root / 'config/matches.tsv').read_text().splitlines():
    if l and not l.startswith('#'):
        n, a, s = l.split('\t')[:3]
        m[int(a, 16)] = (n, int(s))
syms = {f['pc']: f for f in tomllib.loads((root / 'config/symbols.toml').read_text())['func']}
seeds, cur = set(), None
for l in (root / 'config/seeds_overlays.txt').read_text().splitlines():
    if l.startswith('# module '): cur = l.split()[2]
    elif cur == 'e1c0' and l.strip(): seeds.add(int(l, 16))

bad = []
spans = sorted(m.items())
for (a, (n, s)), nxt in zip(spans, spans[1:] + [(A_LOAD + A_SIZE, None)]):
    if n != f'func_{a:08X}': bad.append(f'{n}: name/address disagree ({a:08X})')
    if a % 4 or s % 4 or s <= 0: bad.append(f'{n}: unaligned or empty')
    if not (A_LOAD <= a and a + s <= A_LOAD + A_SIZE): bad.append(f'{n}: outside e1c0')
    if a + s > nxt[0]: bad.append(f'{n}: overlaps next match {nxt[0]:08X}')
    y = syms.get(a)
    if not y or y['name'] != n or y['status'] != 'confirmed' or y.get('size') != s:
        bad.append(f'{n}: symbols.toml entry missing/different (stale: run SO2_DATA=<main checkout> python -I analysis/make_seeds.py)')
    src = root / 'src' / f'{n}.c'   # the symbol must name the function its matched source defines
    if not src.exists() or not re.search(rf'\b{n}\s*\(', src.read_text()): bad.append(f'{n}: src/{n}.c missing or does not define {n}')
    inside = sorted(p for p in syms if a < p < a + s)
    if inside: bad.append(f'{n}: symbols inside the matched body: ' + ' '.join(f'{p:08X}' for p in inside))
    if a not in seeds: bad.append(f'{n}: not an e1c0 seed')
    inner = sorted(t for t in seeds if a < t < a + s)
    if inner: bad.append(f'{n}: seeds inside the matched body: ' + ' '.join(f'{t:08x}' for t in inner))
extra = [f"{p:08X}" for p, y in syms.items() if y['status'] == 'confirmed' and p not in m]
if extra: bad.append('confirmed symbols not in matches.tsv: ' + ' '.join(extra))
print(f'matches {len(m)}; confirmed symbols {sum(y["status"] == "confirmed" for y in syms.values())}; e1c0 seeds {len(seeds)}')
if len(sys.argv) > 1:
    fn = {int(r[0], 16): int(r[1]) for r in (x.split('\t') for x in Path(sys.argv[1]).read_text().splitlines()[1:] if x)}
    diff = [(a, n, s, fn.get(a)) for a, (n, s) in spans if fn.get(a) != s]
    print(f'analyzer: {len(spans) - len(diff)}/{len(spans)} matched functions are analyzer functions of equal size')
    for a, n, s, g in diff:
        cut = sorted(t for t in fn if a < t < a + s)
        print(f'  {n}: matched {s} B, analyzer {g} B; analyzer starts inside: ' + ' '.join(f'{t:08X}' for t in cut))
for b in bad: print('PROBLEM', b)
sys.exit(1 if bad else 0)
