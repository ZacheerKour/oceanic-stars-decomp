"""Overlay map generator.  Run from the repo root:  python -I analysis/overlay_map.py
Reads both disc images (roms/disc{1,2}/*.bin), finds every MIPS-code entry/chunk, infers its load address,
groups identical modules and writes config/overlays.tsv (no game bytes: ids, addresses, sizes, md5 only).
Prints the tables used in ANALYSIS.md section 6.
"""
import os, sys, struct, hashlib, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I removes the script dir
import numpy as np
import iso, toc, slz

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DECOMP_JAL = 0x800121a8   # exe: SLZ-decompress chunk (handle, src, dst, chunk)
ALLOC_JAL = 0x800130ec    # exe: heap alloc (used by the "steer heap to fixed address" idiom)
MAIN_BASE = 0x8002f810
OPS_OK = [0, 9, 15, 35, 43, 4, 5, 3, 13, 12, 8, 32, 36, 40, 33, 37, 41, 10, 11, 7, 6, 2]


def words(b):
    n = len(b) // 4
    return np.frombuffer(b[:n * 4], '<u4')


def feat(b):
    if len(b) < 64:
        return 0, 0, 0.0
    w = words(b)
    return (int((w == 0x03e00008).sum()), int(((w >> 16 == 0x27bd) & ((w & 0x8000) != 0)).sum()),
            float(np.isin(w >> 26, OPS_OK).mean()))


def is_code(f):
    return f[0] >= 1 and f[1] >= 1 and f[2] >= 0.7


def fstarts(w):
    F = {0} | {int(i + 2) * 4 for i in np.nonzero(w == 0x03e00008)[0]}
    F |= {int(i) * 4 for i in np.nonzero((w >> 16 == 0x27bd) & ((w & 0x8000) != 0))[0]}
    return F


def check(b, B):
    """consistency of image b at load address B: jal targets inside the image must be function starts,
    every j target must be inside the image."""
    w = words(b); L = len(w) * 4; F = fstarts(w)
    jal = np.unique(((w[w >> 26 == 3] & 0x3ffffff) << 2) | 0x80000000)
    jj = np.unique(((w[w >> 26 == 2] & 0x3ffffff) << 2) | 0x80000000)
    inr = [int(t) for t in jal if B <= t < B + L]
    return dict(jal=len(jal), inr=len(inr), hit=sum(1 for t in inr if t - B in F),
                j=len(jj), jin=sum(1 for t in jj if B <= t < B + L))


def vote(b):
    """best base by jal-target/function-start alignment; returns (B, check) or None"""
    w = words(b); L = len(w) * 4; F = sorted(fstarts(w))
    jal = np.unique(((w[w >> 26 == 3] & 0x3ffffff) << 2) | 0x80000000)
    C = collections.Counter()
    for t in jal:
        for f in F:
            B = int(t) - f
            if 0x80010000 <= B <= 0x80200000 and B % 4 == 0:
                C[B] += 1
    best = None
    for B, _ in C.most_common(300):
        c = check(b, B)
        if c['hit'] < 1:
            continue
        sc = c['hit'] - 0.5 * (c['inr'] - c['hit']) + (3 * c['jin'] / c['j'] if c['j'] >= 3 else 0)
        if best is None or sc > best[0]:
            best = (sc, B, c)
    return best and (best[1], best[2])


def s16(v):
    return v - 0x10000 if v >= 0x8000 else v


def lit_consts(b):
    """lui+ori/addiu constants: {value: [offsets]} ; and the subset used as 3rd arg of the decompress call /
    followed by an alloc call (heap-steer idiom)"""
    w = [int(x) for x in words(b)]
    allc = collections.defaultdict(list); dec = collections.defaultdict(list); steer = collections.defaultdict(list)
    for i, x in enumerate(w):
        if x >> 26 != 15:
            continue
        r = (x >> 16) & 31; hi = (x & 0xffff) << 16
        for j in range(i + 1, min(len(w), i + 4)):
            y = w[j]
            if (y >> 21) & 31 == r and y >> 26 in (9, 13) and (y >> 16) & 31 == r:
                v = ((hi + s16(y & 0xffff)) & 0xffffffff) if y >> 26 == 9 else hi | (y & 0xffff)
                if not 0x80010000 <= v < 0x80200000:
                    break
                allc[v].append(i * 4)
                for k in range(j, min(len(w), j + 12)):
                    if w[k] == 0x0c000000 | (DECOMP_JAL & 0x0fffffff) >> 2 and r == 6:
                        dec[v].append(i * 4)
                    if w[k] == 0x0c000000 | (ALLOC_JAL & 0x0fffffff) >> 2 and r in (16, 17, 18):
                        steer[v].append(i * 4)
                break
    return allc, dec, steer


def main():
    P = {n: os.path.join(ROOT, 'roms', f'disc{n}', f'Star Ocean - The Second Story (USA) (Disc {n}).bin') for n in (1, 2)}
    D = {n: iso.Disc(P[n]) for n in P}
    T = {n: toc.load(D[n]) for n in P}
    # boot exe (disc 1)
    _, files = D[1].walk()
    exe_l, exe_s = [(l, s) for nm, l, s, fl in files if 'SCUS_944.21' in nm][0][:2]
    exe = D[1].read(exe_l, exe_s)[0x800:]

    ents = {}   # (disc, idx) -> dict
    census = collections.Counter(); raw_code = []
    for n in (1, 2):
        w, sz = T[n]
        for i in range(1, 0x1200):
            l, c = toc.ent(w, sz, i)
            if c == 0:
                continue
            b = D[n].read(l, c * 2048)
            e = dict(disc=n, idx=i, lba=l, sec=c, raw_md5=hashlib.md5(b).hexdigest(), chunks=[])
            d1 = ents.get((1, i))
            e['same_d1'] = bool(n == 2 and d1 and d1['raw_md5'] == e['raw_md5'])
            if e['same_d1']:
                e['chunks'] = d1['chunks']; e['kind'] = d1['kind']; ents[(n, i)] = e; continue
            if b[:3] == b'SLZ':
                off = 0
                while True:
                    o, nx = slz.dec(b, off)
                    f = feat(o)
                    e['chunks'].append(dict(ver=b[off + 3], size=len(o), md5=hashlib.md5(o).hexdigest(), f=f,
                                            code=is_code(f), data=o if is_code(f) else None))
                    if not nx:
                        break
                    off += nx
                e['kind'] = 'slz' + ('-chain' if len(e['chunks']) > 1 else '')
            else:
                u = struct.unpack_from('<I', b, 0)[0]
                f = feat(b)
                if is_code(f) and f[2] > 0.8:
                    raw_code.append((n, i)); e['kind'] = 'raw-code'
                elif u == 0x80010160:
                    e['kind'] = 'str-video'
                elif u == 0x10:
                    e['kind'] = 'raw-tim'
                elif b[:4] in (b'pBAV', b'pQES'):
                    e['kind'] = 'raw-vab/seq'
                elif 0 < u <= len(b) and u % 4 == 0 and u < 0x4000 and len(b) >= u:
                    e['kind'] = 'raw-offset-table'
                elif 0 < u < 0x1000:
                    e['kind'] = 'raw-count-table'
                else:
                    e['kind'] = 'raw-other'
            ents[(n, i)] = e
        for k, v in collections.Counter(e['kind'] for (d, _), e in ents.items() if d == n).items():
            census[(n, k)] = v

    # ---- code images ----
    imgs = {'exe': (0x80010000, exe)}
    for (n, i), e in ents.items():
        if n == 1 and e['chunks'] and e['chunks'][0]['code']:
            imgs[i] = (None, e['chunks'][0]['data'])
    lits = {k: lit_consts(v[1]) for k, v in imgs.items()}
    dec_consts = collections.defaultdict(set); steer_consts = collections.defaultdict(set)
    for k, (a, d, s) in lits.items():
        for v in d: dec_consts[v].add(k)
        for v in s: steer_consts[v].add(k)

    # ---- load-address rules from the loader analysis (verified below against literals in the images) ----
    # entry/chunk -> (addr, loader, role)
    G_AREA = (2982, 2986, 2990, 2994, 2998)
    rules = []   # (predicate(idx,chunk), addr or None(use vote), loader text, role text)
    def R(pred, addr, loader, role): rules.append((pred, addr, loader, role))
    R(lambda i, k: i == 1, MAIN_BASE, 'exe 0x80010650 -> 0x800105b0 (boot, entry 1)', 'main program A (boot, entry 1)')
    R(lambda i, k: i == 2129, MAIN_BASE, 'main-switch 0x800105b0 (called from 2576/entry 1 via imm 2129)', 'main program B (battle: owns the 1977-1984 and 2130-2158 loaders)')
    R(lambda i, k: i in (2576, 3100), MAIN_BASE, 'main-switch 0x800105b0 (imm 2576/3100 in entry 1 and 2129)', 'main program C (field: owns the area-group loader; 3100 is an identical copy)')
    R(lambda i, k: i == 3107, MAIN_BASE, 'main-switch 0x800105b0 (2576 loads 3107 into heap var 0x800756f4, passes it at 0x800682f4)', 'main program D (85 KB, loaded through the main-switch)')
    R(lambda i, k: i in (2981, 2985, 2989, 2993, 2997), 0x800d1b28, '2576 area-group loader 0x80030a94: group base 2980+4n, part 1 -> 0x800d1b28', 'common field module (5 identical copies, one per group)')
    R(lambda i, k: i in G_AREA, 0x8007e000, '2576 area-group loader 0x80030a94: group base 2980+4n, part 2 -> 0x8007e000', 'area module (group part 2)')
    R(lambda i, k: i == 3001, 0x80088000, 'area modules (2982..3010 at 0x8007e000) load group 3000 part 1 -> 0x80088000 (literal in 6 modules)', 'sub-module of area modules')
    R(lambda i, k: i in (3103,), 0x801e9000, '2576@0x80055098: heap-steer to 0x801e9000, then decompress (entry 3103 via imm 0xc1f at 2576@0x8004b124)', 'large overlay in top-of-RAM heap')
    R(lambda i, k: i in (3104, 3105, 3106, 3108, 3109), 0x801f9800, '2576@0x80055114: heap-steer to 0x801f9800 (table 3104,3106,3108,3109 at 2576 data 0x800744d6; 3105 via register)', 'small overlays sharing one heap slot')
    R(lambda i, k: i == 1981, 0x80085518, '2129@0x800468bc literal a2=0x80085518, chunk 0 (imm 1981)', 'battle overlay (follows 1984/1)')
    R(lambda i, k: i == 1984 and k == 0, 0x800715d0, '2129@0x80045a00 literal a2=0x800715d0, chunk 0 (imm 1984)', 'battle overlay main part')
    R(lambda i, k: i == 1984 and k == 1, 0x80081728, '2129@0x80046208 literal a2=0x80081728, chunk 1', 'battle overlay part 2 (directly follows chunk 0)')
    R(lambda i, k: i == 1984 and k >= 2, 0x8008c1f4, '2129@0x80071bec: scene switch picks chunk 2..10, literal a2=0x8008c1f4 (mutually exclusive)', 'per-scene battle handler (9 variants, same slot)')
    R(lambda i, k: 2131 <= i <= 2159 and i % 2 == 1, 'slot', '2129@0x80045ae4 steers heap to 0x801ee000; chunk k relocated for 0x801ee000+0x4000*k (partner data entry i-1 listed in 2129 table +0x5569c)', 'battle enemy-set code, 4 slot-relocated copies in one entry')
    R(lambda i, k: i in (3004, 3006, 3008, 3010, 3012, 3014, 3016, 3018, 3020, 3022, 3101, 3102, 4191, 4328, 4332) or (3112 <= i <= 3174 and i % 2 == 0), None, 'imm refs in 2576/2129/entry 1; area-module slot 0x8007e000 (jal vote; not traced to a call site)', 'area/scene module (0x8007e000 slot)')

    out = []   # rows
    notes = []
    def lookup(i, k):
        for pred, addr, ld, role in rules:
            if pred(i, k):
                return addr, ld, role
        return None, 'unknown', 'code (unclassified)'
    slot_base = max(steer_consts, key=lambda v: v == 0x801ee000) if 0x801ee000 in steer_consts else 0x801ee000

    for (n, i), e in sorted(ents.items(), key=lambda t: (t[0][1], t[0][0])):
        for k, ch in enumerate(e['chunks']):
            if not ch['code']:
                continue
            if e['same_d1'] and False:
                continue
            addr, ld, role = lookup(i, k)
            data = ch['data'] if ch['data'] is not None else ents[(1, i)]['chunks'][k]['data']
            v = vote(data)
            basis = ''
            if addr == 'slot':
                addr = slot_base + 0x4000 * k; basis = 'SLOT'
            if addr is None:
                if v: addr = v[0]; basis = 'VOTE'
            elif not basis:
                basis = 'LIT' if (addr in dec_consts or addr in steer_consts or addr == MAIN_BASE) else 'RULE'
            c = check(data, addr) if addr else None
            if v and addr and v[0] == addr and basis != 'VOTE': basis += '+VOTE'
            # confidence: loader-confirmed address needs some structural support; vote-only needs strong support
            ratio = (c['hit'] / c['inr']) if c and c['inr'] else 1.0
            loader_ev = any(t in basis for t in ('LIT', 'SLOT', 'RULE'))
            if addr is None: conf = 'unknown'
            elif loader_ev:
                conf = 'high' if c['hit'] >= 3 and ratio >= 0.8 else ('medium' if ratio >= 0.6 else 'low')
            else:
                conf = 'high' if c['hit'] >= 10 and ratio >= 0.8 else ('medium' if c['hit'] >= 3 and ratio >= 0.8 else 'low')
            out.append(dict(idx=i, disc=n, chunk=k, size=ch['size'], md5=ch['md5'], addr=addr, basis=basis, conf=conf,
                            loader=ld, role=role, check=c, ver=ch['ver'], lba=e['lba'], sec=e['sec'], same_d1=e['same_d1']))

    # ---- merge disc rows: disc 2 rows identical to disc 1 (same entry/chunk/md5) -> disc "1+2"
    key1 = {(r['idx'], r['chunk']): r for r in out if r['disc'] == 1}
    rows = []
    for r in out:
        if r['disc'] == 2:
            r1 = key1.get((r['idx'], r['chunk']))
            if r1 and r1['md5'] == r['md5']:
                r1['disc'] = '1+2'; continue
        rows.append(r)
    for r in rows:
        if isinstance(r['disc'], int): r['disc'] = str(r['disc'])
    # duplicates (identical md5)
    byhash = collections.defaultdict(list)
    for r in rows: byhash[r['md5']].append(r)
    for r in rows:
        g = sorted({x['idx'] for x in byhash[r['md5']]} - {r['idx']})
        r['dups'] = ','.join(map(str, g[:6])) + ('..(+%d)' % (len(g) - 6) if len(g) > 6 else '')

    # ---- config/overlays.tsv
    path = os.path.join(ROOT, 'config', 'overlays.tsv')
    with open(path, 'w', newline='\n') as f:
        f.write('# entry\tdisc\tchunk\tload_addr\tsize\tmd5(decompressed)\trole\tconfidence\tbasis\tidentical_entries\tloader\n')
        f.write('# entries are SO2.BIN TOC indices; size/md5 are of the decompressed chunk; load_addr 0x0 = unknown\n')
        f.write('boot_exe\t1+2\t0\t0x80010000\t%d\t%s\tSCUS_944.21 text (identical on both discs)\thigh\tPS-X header\t\tboot (BIOS)\n' % (len(exe), hashlib.md5(exe).hexdigest()))
        for r in rows:
            f.write('%d\t%s\t%d\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\n' % (r['idx'], r['disc'], r['chunk'], hex(r['addr']) if r['addr'] else '0x0', r['size'], r['md5'], r['role'], r['conf'], r['basis'], r['dups'], r['loader']))

    # ---- report ----
    print('## census (non-empty entries per kind)')
    for (n, k), v in sorted(census.items()): print(f'disc{n}\t{k}\t{v}')
    print('raw entries that look like code:', raw_code or 'none')
    d2only = [i for (n, i), e in ents.items() if n == 2 and not e['same_d1']]
    print('disc-2 entries differing/new vs disc 1:', len(d2only), 'kinds', dict(collections.Counter(ents[(2, i)]['kind'] for i in d2only)))
    print('disc-2 code rows differing:', [(r['idx'], r['chunk']) for r in rows if r['disc'] == '2'])
    print('decompress-call literal destinations:', {hex(k): sorted(map(str, v)) for k, v in sorted(dec_consts.items()) if k not in (0x80070000,)})
    print('heap-steer destinations (>=0x801e0000):', {hex(k): sorted(map(str, v)) for k, v in sorted(steer_consts.items()) if k >= 0x801e0000})
    def rng(xs):
        xs = sorted(xs); res = []; i = 0
        while i < len(xs):
            j = i
            step = (xs[i + 1] - xs[i]) if i + 1 < len(xs) and xs[i + 1] - xs[i] in (1, 2) else 0
            while step and j + 1 < len(xs) and xs[j + 1] - xs[j] == step: j += 1
            res.append(str(xs[i]) if j == i else f'{xs[i]}..{xs[j]}' + (' step 2' if step == 2 else ''))
            i = j + 1
        return ','.join(res)
    print('\n## groups (role -> entries)')
    print('role\tentries\taddr\t#entries\t#distinct\tsize range\tconf\tbasis\tloader')
    G = collections.OrderedDict()
    for r in rows: G.setdefault((r['role'], r['loader']), []).append(r)
    for (role, ld), rs in G.items():
        ids = sorted({r['idx'] for r in rs})
        print('\t'.join([role, rng(ids), ','.join(sorted({hex(r['addr']) if r['addr'] else '?' for r in rs})), str(len(ids)),
              str(len({r['md5'] for r in rs})), '%d-%d' % (min(r['size'] for r in rs), max(r['size'] for r in rs)),
              '/'.join(sorted({r['conf'] for r in rs})), '/'.join(sorted({r['basis'] for r in rs})), ld]))
    if '-v' in sys.argv:
        print('\n## per entry/chunk')
        for r in rows: print(r['idx'], r['disc'], r['chunk'], hex(r['addr']) if r['addr'] else '?', r['size'], r['conf'], r['basis'], r['check'], r['dups'])
    print('\nrows:', len(rows), 'written', os.path.relpath(path, ROOT))
    return rows


if __name__ == '__main__':
    main()
