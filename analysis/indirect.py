"""Classify + resolve the analyzer's unresolved indirect jalr/jr sites (static, best effort).  Run from the repo root:
    python -I analysis/indirect.py [summary.txt]      (default recomp/sandbox/_out_run2/summary.txt, "*_seeded" sections)
Needs the LOCAL gitignored binaries (extracted/SCUS_944.21, analysis/out/d1/*.dec) and the sandbox functions.tsv per program.
Writes config/indirect_targets.tsv (addresses/names only).  Method: per site, forward symbolic evaluation of the register from
the nearest control-flow boundary (lui/addiu/ori/addu/sll/lw/move); the result is a constant, a load from a constant slot, a load
from an indexed table, a load from a struct member, an argument register or a call return value.  Table entries are read from the
image; zero/bss slots are resolved from constant stores (sw of a code address) to that slot.  Argument registers are resolved from
the constant passed at direct `jal` callers (depth 1).  Targets are only emitted when they look like code addresses."""
import os, re, sys, struct, collections

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
P = lambda *a: os.path.join(ROOT, *a)
EXE_BASE, EXE_END, LOAD = 0x80010000, 0x8002f800, 0x8002f810
PROGS = {'A': ('0001_312.dec', 'e1c0'), 'B': ('2129_24323.dec', 'e2129c0'), 'C': ('2576_30736.dec', 'e2576c0')}
RN = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
      's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra']
S = lambda x: x - 0x10000 if x & 0x8000 else x
M32 = 0xffffffff


class Img:
    def __init__(self, exe, prog):
        self.exe, self.prog = exe, prog
        self.end = LOAD + (len(prog) if prog else 0)

    def inprog(self, a): return LOAD <= a < self.end
    def inexe(self, a): return EXE_BASE <= a < EXE_END

    def word(self, a):
        if self.inexe(a) and a % 4 == 0: return struct.unpack_from('<I', self.exe, a - EXE_BASE)[0]
        if self.prog is not None and self.inprog(a) and a % 4 == 0: return struct.unpack_from('<I', self.prog, a - LOAD)[0]
        return None

    def code(self, a):  # plausible code pointer: word aligned, inside exe or this program
        return a % 4 == 0 and (self.inexe(a) or (self.prog is not None and self.inprog(a)))


def add(a, b):
    if a[0] == 'c' and b[0] == 'c': return ('c', (a[1] + b[1]) & M32)
    if a[0] == 'c' and a[1] == 0: return b
    if b[0] == 'c' and b[1] == 0: return a
    return ('add', a, b)


def terms(v, k=0):
    """flatten an add-tree -> (const, [non-const terms])"""
    if v[0] == 'c': return v[1], []
    if v[0] == 'add':
        c1, t1 = terms(v[1]); c2, t2 = terms(v[2]); return (c1 + c2) & M32, t1 + t2
    return 0, [v]


def run(img, words, base, start_i, end_i, init=None, stores=None):
    """linear forward evaluation of words[start_i:end_i]; returns regs.  stores: callback(pc, rt_val, base_val, off)"""
    r = {i: ('reg', RN[i]) for i in range(32)}; r[0] = ('c', 0)
    i = start_i
    while i < end_i:
        w = words[i]; pc = base + 4 * i
        op, rs, rt, rd, sh, fn, im = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63, w & 0xffff
        dst, val = None, ('unk',)
        if op == 0x0f: dst, val = rt, ('c', im << 16)
        elif op in (0x08, 0x09): dst, val = rt, add(r[rs], ('c', S(im) & M32))
        elif op == 0x0d: dst, val = rt, (('c', r[rs][1] | im) if r[rs][0] == 'c' else ('unk',))
        elif op == 0x23: dst, val = rt, ('ld', r[rs], S(im))
        elif op in (0x20, 0x21, 0x24, 0x25, 0x22, 0x26, 0x0a, 0x0b, 0x0c, 0x0e, 0x30, 0x32): dst = rt
        elif op in (0x2b, 0x28, 0x29):
            if stores and op == 0x2b: stores(pc, r[rt], r[rs], S(im))
        elif op == 0 and fn == 0x21 or op == 0 and fn == 0x20: dst, val = rd, add(r[rs], r[rt])
        elif op == 0 and fn == 0x25 and rt == 0: dst, val = rd, r[rs]
        elif op == 0 and fn == 0x25 and rs == 0: dst, val = rd, r[rt]
        elif op == 0 and fn == 0x00 and w: dst, val = rd, (('c', (r[rt][1] << sh) & M32) if r[rt][0] == 'c' else ('sll', r[rt], sh))
        elif op == 0 and fn not in (0x08, 0x09, 0x0c, 0x0d, 0x11, 0x13, 0x18, 0x19, 0x1a, 0x1b) and w: dst = rd
        elif op == 0 and fn in (0x10, 0x12): dst = rd
        elif op in (3,) or (op == 0 and fn == 9):  # call: delay slot first, then clobber
            if i + 1 < end_i:
                i += 1; w2 = words[i]
                o2, rs2, rt2, im2 = w2 >> 26, (w2 >> 21) & 31, (w2 >> 16) & 31, w2 & 0xffff
                if o2 == 0x0f: r[rt2] = ('c', im2 << 16)
                elif o2 in (0x08, 0x09): r[rt2] = add(r[rs2], ('c', S(im2) & M32))
                elif o2 == 0x0d: r[rt2] = ('c', r[rs2][1] | im2) if r[rs2][0] == 'c' else ('unk',)
                elif o2 == 0x23: r[rt2] = ('ld', r[rs2], S(im2))
                elif o2 == 0 and (w2 & 63) == 0x25 and rt2 == 0 and False: pass
                elif o2 == 0 and (w2 & 63) == 0x25 and ((w2 >> 16) & 31) == 0: r[(w2 >> 11) & 31] = r[rs2]
            tgt = ((w & 0x3ffffff) << 2 | 0x80000000) if op == 3 else None
            for k in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31]: r[k] = ('unk',)
            r[2] = ('ret', tgt)
            i += 1; continue
        if dst is not None and dst != 0: r[dst] = val
        i += 1
    return r


def load_sites(path):
    t = open(path).read()
    out = {}
    for s in re.split(r'^== ', t, flags=re.M)[1:]:
        n = s.split('\n')[0].strip()
        if not n.endswith('_seeded'): continue
        m = re.search(r'unresolved indirect sites.*?\n((?:    [0-9A-F]{8} .*\n)+)', s)
        out[n[:-7]] = [(int(l.split()[0], 16), l.split()[2]) for l in m.group(1).splitlines()] if m else []
    return out


def load_funcs(prog):
    f = P('recomp', 'sandbox', '_out_run2', prog, 'seeded', 'functions.tsv')
    fs = []
    for l in list(open(f))[1:]:
        c = l.split('\t'); fs.append((int(c[0], 16), int(c[1])))
    return sorted(fs)


class Ctx:
    """one image (boot exe or program A/B/C): words, functions, branch targets, direct callers, constant stores"""
    def __init__(self, name, exe):
        self.name = name
        self.prog = None if name == 'boot' else open(P('analysis', 'out', 'd1', PROGS[name][0]), 'rb').read()
        self.img = Img(exe, self.prog)
        self.base, data = (EXE_BASE, exe) if self.prog is None else (LOAD, self.prog)
        self.words = words = list(struct.unpack('<%dI' % (len(data) // 4), data))
        self.funcs = load_funcs(name)
        self.starts = {a for a, _ in self.funcs}
        self.fs = [a for a, _ in self.funcs]
        self.junk = {a for a, t in load_tags(name).items() if 'syscall' in t and 'gte' in t}   # functions that are data decoded as code
        self.btarget, self.callers = set(), collections.defaultdict(list)
        for i, w in enumerate(words):
            pc = self.base + 4 * i; op = w >> 26
            if op in (4, 5, 6, 7, 1): self.btarget.add(pc + 4 + 4 * S(w & 0xffff))
            if op == 3: self.callers[((w & 0x3ffffff) << 2) | 0x80000000].append(pc)
        self.slot_st, self.mem_st, self.vt_st, self.vt_abs = (collections.defaultdict(set) for _ in range(4))
        self.arg_st = collections.defaultdict(set)   # slot -> {(func, argreg)}: slot = argument stored by a setter function
        cur = [0]
        def cb(pc, v, b, off):
            if v[0] == 'reg' and v[1] in ('a0', 'a1', 'a2', 'a3') and b[0] == 'c': self.arg_st[(b[1] + off) & M32].add((cur[0], v[1]))
            if v[0] == 'c' and b[0] == 'c' and v[1] not in self.starts and (self.img.inexe(v[1]) or self.img.inprog(v[1])): self.vt_abs[(b[1] + off) & M32].add(v[1])
            if v[0] == 'c' and v[1] not in self.starts and (self.img.inexe(v[1]) or self.img.inprog(v[1])) and v[1] % 4 == 0 and b[0] != 'c':
                self.vt_st[off].add(v[1])
            if v[0] != 'c' or not self.img.code(v[1]) or v[1] < 0x80010000: return
            if b[0] == 'c': self.slot_st[(b[1] + off) & M32].add(v[1])
            else:
                c, t = terms(b)
                if not t or c == 0: self.mem_st[off].add(v[1])
        for a, sz in self.funcs:
            i0 = (a - self.base) // 4
            cur[0] = a
            if 0 <= i0 < len(words) and a not in self.junk: run(self.img, words, self.base, i0, min(len(words), i0 + sz // 4), stores=cb)

    def fstart(self, pc):
        import bisect
        k = bisect.bisect_right(self.fs, pc) - 1
        return self.fs[k] if k >= 0 else self.base

    def ev(self, pc, reg):
        base, words = self.base, self.words
        i = (pc - base) // 4; f = (self.fstart(pc) - base) // 4
        cands = sorted({b for b in self.btarget if base + 4 * f <= b <= pc}, reverse=True)
        for st in [(c - base) // 4 for c in cands[:6]] + [f]:
            v = run(self.img, words, base, st, i)[reg]
            if v[0] != 'reg' or v[1] in ('a0', 'a1', 'a2', 'a3') or st == f: return v
        return v

    def argconsts(self, f, an):
        """constants passed in arg register `an` at direct jal callers of f (callers in this image)"""
        got = set()
        for cp in self.callers.get(f, []):
            v = self.ev(cp, an)
            if v[0] == 'c': got.add(v[1])
        return got


def load_tags(prog):
    f = P('recomp', 'sandbox', '_out_run2', prog, 'seeded', 'functions.tsv')
    return {int(l.split('\t')[0], 16): l.split('\t')[10] for l in list(open(f))[1:] if len(l.split('\t')) > 10}


def main():
    summ = sys.argv[1] if len(sys.argv) > 1 else P('recomp', 'sandbox', '_out_run2', 'summary.txt')
    sites = load_sites(summ)
    exe = open(P('extracted', 'SCUS_944.21'), 'rb').read()[0x800:0x800 + 0x1f800]
    C = {n: Ctx(n, exe) for n in ('boot', 'A', 'B', 'C')}
    # exe-range slots/args are shared by every image: a program's setter call can fill a boot-exe slot
    def exe_slot_targets(slot):
        out = set()
        for cn, c in C.items():
            tag = '' if cn == 'boot' else cn + ':'
            for x in c.slot_st.get(slot, ()): out.add(x if c.img.inexe(x) else tag + '%08x' % x)
            for f, r in C['boot'].arg_st.get(slot, ()):
                for cc in C.values():   # callers of the setter in every image
                    for x in cc.argconsts(f, RN.index(r)):
                        if cc.img.code(x): out.add(x if cc.img.inexe(x) else cc.name + ':%08x' % x)
        return out
    rows = []
    for name, sl in sites.items():
        cx = C[name]
        img, prog, base, words, starts = cx.img, cx.prog, cx.base, cx.words, cx.starts
        slot_st, mem_st, vt_st, vt_st_abs, callers, btarget, fstart = cx.slot_st, cx.mem_st, cx.vt_st, cx.vt_abs, cx.callers, cx.btarget, cx.fstart
        funcs = cx.funcs
        ev = lambda pc, reg: (cx.ev(pc, reg), 0)
        reg_of = {pc: (words[(pc - base) // 4] >> 21) & 31 for pc, _ in sl}
        for pc, op in sl:
            w = words[(pc - base) // 4]
            note, tg, kind, conf = '', [], 'unknown', 'none'
            rs = (w >> 21) & 31
            if fstart(pc) in cx.junk:
                rows.append((pc, name, 'data_as_code', [], 'none', f'inside analyzer pseudo-function {fstart(pc):08x} (syscall+gte tags = data)')); continue
            if w >> 26 != 0 or (w & 63) not in (8, 9):
                rows.append((pc, name, 'bad_decode', [], 'none', 'not a jr/jalr word (data decoded as code)')); continue
            if rs == 0:
                rows.append((pc, name, 'bad_decode', [], 'none', 'jr/jalr $zero: data decoded as code')); continue
            v, st = ev(pc, rs)
            c, t = terms(v) if v[0] in ('c', 'add') else (0, [])
            def table(baseaddr, stride, off_note, guard):
                got = []
                n = guard if guard else 64
                for k in range(n):
                    x = img.word(baseaddr + k * stride)
                    if x is None or not img.code(x) or (op == 'jalr' and not x >= LOAD and not img.inexe(x)): break
                    if op == 'jalr' and x not in starts and prog is not None and not img.inexe(x): break
                    got.append(x)
                return got
            fi = (fstart(pc) - base) // 4
            guard = None
            for j in range((pc - base) // 4 - 1, max(fi, (pc - base) // 4 - 40) - 1, -1):
                g = words[j]
                if g >> 26 == 0x0b: guard = g & 0xffff; break   # sltiu
            if v[0] == 'c':
                kind, tg, conf = 'const_target', [v[1]], 'high'
                if v[1] in (0xa0, 0xb0, 0xc0): kind, tg, conf, note = 'bios_dispatch', [], 'high', 'BIOS vector %02x (function number in $t1)' % v[1]
                elif v[1] == 0: kind, tg, conf, note = 'null_target', [], 'none', 'constant 0 on this path (guarded/uninitialised callback)'
                elif v[1] >= 0xa0000000 or v[1] < 0x80000000: kind, tg, conf, note = 'kernel_ram', [], 'none', 'const target %08x (kernel/uncached alias, not image code)' % v[1]
                elif not img.code(v[1]): kind, tg, conf, note = 'overlay_entry', [], 'none', 'const target %08x outside exe/program (overlay region)' % v[1]
            elif v[0] == 'ld':
                bc, bt = terms(v[1])
                if not bt:  # static slot
                    slot = (bc + v[2]) & M32; x = img.word(slot)
                    if x and img.code(x) and x >= 0x80010000:
                        kind, tg, conf = 'static_slot_init', [x], 'high'
                    elif (slot_st.get(slot) and not img.inexe(slot)) or (img.inexe(slot) and exe_slot_targets(slot)):
                        kind, tg, conf = 'static_slot_stored', sorted(exe_slot_targets(slot) if img.inexe(slot) else slot_st[slot], key=str), 'medium'
                        note = 'slot %08x assigned by const stores / setter-call arguments' % slot
                    else: kind, note = 'static_slot_unassigned', 'slot %08x: zero/bss, no const store in image' % slot
                elif len(bt) == 1 and bt[0][0] == 'sll' and bc:
                    stride = 1 << bt[0][2]; tb = (bc + v[2]) & M32
                    tg = table(tb, stride, '', guard if op == 'jalr' else guard)
                    kind = 'jump_table_unguarded' if op == 'jr' else 'fnptr_table'
                    note = f'table {tb:08x} stride {stride} n={len(tg)}' + (f' guard {guard}' if guard else ' (no sltiu guard found)')
                    conf = ('medium' if guard and len(tg) == guard else 'low') if tg else 'none'
                    if not tg: kind = 'table_unreadable'
                elif len(bt) == 1 and bt[0][0] in ('reg', 'ld', 'ret', 'unk'):
                    o = v[2]
                    if bt[0][0] == 'ld' and bt[0][1][0] == 'c':   # global pointer to a struct of callbacks
                        gs = (bt[0][1][1] + bt[0][2]) & M32
                        cands = {img.word(gs)} | set(slot_st.get(gs, ())) | set(vt_st_abs.get(gs, ()))
                        cand = sorted({x for b in cands if b for x in [img.word((b + o) & M32)] if x and x in starts and b % 4 == 0})
                        kind = 'global_struct_member'; note = f'struct pointer @{gs:08x}, field +{o}; {len(cands - {None, 0})} struct bases, {len(cand)} targets'
                        if cand: tg, conf = cand, 'medium'
                        rows.append((pc, name, kind, tg, conf, note)); continue
                    if bt[0][0] == 'ld':   # ld(ld(X,p),o): C++ virtual call, vptr at +p, slot at +o
                        vb = vt_st.get(bt[0][2], ())
                        cand = sorted({x for b in vb for x in [img.word(b + o)] if x and x in starts})
                        kind = 'vtable_dispatch'; note = f'vptr@+{bt[0][2]} slot@+{o}; {len(vb)} vtable bases stored at +{bt[0][2]}, {len(cand)} distinct targets'
                        if cand: tg, conf = cand, 'low'
                        rows.append((pc, name, kind, tg, conf, note)); continue
                    kind = 'struct_member'; note = f'off {o} base {bt[0][1] if bt[0][0]=="reg" else bt[0][0]}'
                    if bc: note += f'+{bc:x}'
                    cand = sorted(x for x in mem_st.get(o, ()) if x in starts)
                    if cand and len(cand) <= 12: tg, conf = cand, 'low'; note += f'; {len(cand)} const stores at off {o}'
                    elif cand: note += f'; {len(cand)} const stores at off {o} (too many)'
                else: kind, note = 'complex', 'ld(' + repr(v[1])[:60] + ')'
            elif v[0] == 'reg' and v[1] in ('a0', 'a1', 'a2', 'a3'):
                kind = 'arg_register'; f0 = fstart(pc); got = set()
                for cc in (C.values() if img.inexe(f0) else [cx]):
                    got |= {x if cc.img.inexe(x) else cc.name + ':%08x' % x for x in cc.argconsts(f0, RN.index(v[1])) if cc.img.code(x)}
                tg = sorted(got, key=str); conf = 'medium' if tg else 'none'
                note = f'{v[1]} of func {f0:08x}; {len(callers.get(f0, []))} direct callers, {len(tg)} const'
            elif v[0] == 'reg':
                kind, note = 'reg_unknown', f'{v[1]} live-in (set before a branch target)'
                if op == 'jr' and v[1] == 'ra': kind = 'return'
            elif v[0] == 'ret':
                kind, note = 'call_return', 'result of jal %s' % ('%08x' % v[1] if v[1] else 'jalr')
            elif v[0] in ('add', 'sll'):
                kind, note = 'complex', repr(v)[:70]
            rows.append((pc, name, kind, tg, conf, note))
    # hand-verified over every decompressed module (RECOMP_NOTES '2026-10-26 Analyzer seeds were never read'): no code target in any image
    dead = 'slot %08x set only by setter %08x, which no module references (no jal, pointer or lui/addiu); cleared at init, call is beqz-guarded'
    dcb = 'slot %08x = saved BIOS DCB+0x34 handler (device table at kernel 0x150, 0x50-byte records); chains to the BIOS'
    VERIFIED = {(0x8003c498, 'A'): ('static_slot_dead', 'high', dead % (0x8004e170, 0x8003c56c)),
                (0x8003d3f4, 'C'): ('static_slot_dead', 'high', dead % (0x80074368, 0x8003d4c8)),
                (0x8004b344, 'A'): ('bios_dcb_hook', 'high', dcb % 0x8004efe4),
                (0x800715c4, 'C'): ('bios_dcb_hook', 'high', dcb % 0x80078880)}
    rows = [(pc, n, *VERIFIED[pc, n][:1], tg, *VERIFIED[pc, n][1:]) if (pc, n) in VERIFIED and k == 'static_slot_unassigned' else (pc, n, k, tg, cf, nt)
            for pc, n, k, tg, cf, nt in rows]
    with open(P('config', 'indirect_targets.tsv'), 'w', newline='\n') as f:
        f.write('# Indirect call/jump sites unresolved by psxrecomp-analyze (see analysis/indirect.py; RECOMP_NOTES.md "Indirect call resolution").\n'
                '# site_pc\tprogram(boot|A|B|C)\tkind\ttargets\tconfidence\tnote\n')
        for pc, n, k, tg, cf, nt in sorted(rows, key=lambda r: (r[1], r[0])):
            f.write(f'{pc:08x}\t{n}\t{k}\t{",".join(x if isinstance(x, str) else "%08x" % x for x in tg)}\t{cf}\t{nt}\n')
    cnt = collections.Counter((r[1], r[2]) for r in rows)
    for k in sorted(cnt): print(k, cnt[k])
    for n in sites:
        rr = [r for r in rows if r[1] == n]
        print(n, 'sites', len(rr), 'resolved', sum(1 for r in rr if r[3]))


if __name__ == '__main__':
    main()
