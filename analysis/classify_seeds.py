"""Classify the functions psxrecomp's own discovery (the `heur` static run) finds beyond config/seeds_overlays.txt, and write the final
per-chunk seed files config/seeds_final/<CHUNK>.txt = our seeds + verified extras.  Needs the local discs and the heur output of
recomp/aot/run_static_all.py (`<copy>/_work/static_heur`, generated C, gitignored).  Run from the repo root:
    SO2_DATA=<main checkout> python -I analysis/classify_seeds.py <copy>/_work/static_heur
A heur extra is accepted when analysis/seedcheck.py finds real code: a valid walk ending in jr ra AND (a jal from accepted code OR an
`addiu sp,sp,-N` prologue).  Rejects: data decoded as code, no caller/prologue, or an interior of another accepted function (no-split).
Writes config/seeds_final/<CHUNK>.txt (addresses only) and config/seeds_final/report.tsv (address + class per extra)."""
import os, re, sys, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_seeds as M, seedcheck


def heur_funcs(d):
    """Function entries in the generated C of one chunk (definitions that are not CPS resume wrappers; includes the few isolated fragments)."""
    s = set()
    for f in glob.glob(os.path.join(d, 'overlays_static_*.c')):
        prev = ''
        for l in open(f, errors='replace'):
            m = re.match(r'^void ov_\w+?_func_([0-9A-F]{8})\(CPUState\* cpu\)$', l.rstrip())
            if m and not prev.startswith('/* CPS block resume'): s.add(int(m.group(1), 16))
            if l.strip(): prev = l
    return s


def ours(path):
    mods, cur = {}, None
    for l in open(path):
        m = re.match(r'# module (\S+) ', l)
        if m: cur = mods.setdefault(m.group(1), set())
        elif cur is not None and l.strip() and not l.startswith('#'): cur.add(int(l, 16))
    return mods


def main(heur_dir):
    out = M.P('config', 'seeds_final'); os.makedirs(out, exist_ok=True)
    O = ours(M.P('config', 'seeds_overlays.txt'))
    rep, tot = [], []
    for o, b in M.load_mods():
        cid, base = o['id'], int(o['load_addr'], 16)
        w, mine = seedcheck.words(b), O[cid]
        hd = os.path.join(heur_dir, cid.upper())
        heur = heur_funcs(hd) if os.path.isdir(hd) else set()
        heur = {a for a in heur if base <= a < base + len(b)}
        extras = heur - mine
        ent, found, jt, bad = seedcheck.verify(w, base, extras | mine, mine)
        spans = {a: (lambda r: r and r[0])(seedcheck.walk(w, base, a)) for a in ent}
        acc = set(mine)
        covered = set().union(*[v for v in spans.values() if v])

        def leaf(a):   # frameless leaf: valid walk, no call, ends in jr ra, and starts right after covered code's `jr ra` + delay slot
            r = seedcheck.walk(w, base, a)
            p = ((a - base) >> 2) - 1
            while p > 1 and w[p] == 0 and w[p - 1] != 0x03e00008: p -= 1   # alignment padding
            q = p - 1 if w[p - 1] == 0x03e00008 else p
            return (r, q) if r and not r[1] and w[max(r[0]) - 1] == 0x03e00008 and w[q] == 0x03e00008 and q in covered and (a - base) >> 2 not in covered else None

        pend, todo = [], sorted(extras)
        for a in todo:
            i = (a - base) >> 2
            if a in found and a in ent:
                inner = next((e for e in sorted(ent, reverse=True) if e < a and spans.get(e) and i in spans[e]), None)
                if inner is not None and a not in jt: rep.append((cid, a, 'reject interior')); continue
                acc.add(a); rep.append((cid, a, 'accept called' if a in jt else 'accept prologue'))
            else: pend.append(a)
        again = True
        while again:      # leaves chain: one accepted leaf makes the next one's predecessor covered
            again = False
            for a in list(pend):
                r = leaf(a)
                if r: acc.add(a); covered |= r[0][0]; pend.remove(a); rep.append((cid, a, 'accept leaf')); again = True
        for a in pend:
            rep.append((cid, a, 'reject interior' if (a - base) >> 2 in covered else 'reject ' + bad.get(a, 'not code (invalid opcode / no end / off module)')))
        with open(os.path.join(out, cid.upper() + '.txt'), 'w', newline='\n') as f:
            f.write(f'# {cid.upper()} load={o["load_addr"]} size={o["size"]}: {len(acc)} verified function entries (config/seeds_overlays.txt + verified psxrecomp-discovered extras)\n')
            f.writelines(f'{a:08x}\n' for a in sorted(acc))
        tot.append((cid.upper(), len(mine), len(heur), len(extras), len(acc) - len(mine)))
    with open(os.path.join(out, 'report.tsv'), 'w', newline='\n') as f:
        f.write('# chunk\taddr\tclass (heur extras only: functions psxrecomp discovers that config/seeds_overlays.txt lacks)\n')
        f.writelines(f'{c.upper()}\t{a:08x}\t{k}\n' for c, a, k in rep)
    print('chunk ours heur extras accepted')
    for t in tot:
        if t[3]: print(*t)
    from collections import Counter
    print(Counter(k for _, _, k in rep))


if __name__ == '__main__':
    main(sys.argv[1])
