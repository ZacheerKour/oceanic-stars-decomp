"""Function-entry seeds + symbols.toml from the LOCAL (gitignored) discs/extracted exe.  Run from the repo root:
    python -I analysis/make_seeds.py
Writes config/seeds.txt (boot exe, psxrecomp seeds format: one hex address per line), config/seeds_overlays.txt (per-module
sections) and config/symbols.toml (config/matches.tsv byte-matched functions = confirmed, with size, splat function labels of the main overlay = guessed).
Output has addresses and names only.  Heuristic: jal targets inside a module + `addiu sp,sp,-N` prologues (false positives
possible in data; psxrecomp validates seeds).  Also merges the call-type targets of config/indirect_targets.tsv
(written by analysis/indirect.py; optional, idempotent).  Needs roms/disc1 + extracted/SCUS_944.21; splat labels need build/asm/main_code.s."""
import os, re, sys, json, struct
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(ROOT, 'analysis'))
import slz, seedcheck

EXE_BASE, EXE_END = 0x80010000, 0x8002f800
P = lambda *a: os.path.join(ROOT, *a)
# gitignored inputs (roms/, extracted/, build/asm/) may live in another checkout, e.g. the main repo when run from a worktree
PD = lambda *a: os.path.join(os.environ.get('SO2_DATA', ROOT), *a)


def scan(b, base):
    n = len(b) // 4
    w = struct.unpack('<%dI' % n, b[:n * 4])
    jal = {((x & 0x3ffffff) << 2) | 0x80000000 for x in w if x >> 26 == 3}
    # an `addiu sp,sp,-N` right behind a branch/jump is that branch's delay slot (gcc fills it with the frame setup), not an entry
    # unless something calls it (RECOMP_NOTES '2026-10-26 Host generation B/C')
    slot = lambda x: x >> 26 in (1, 2, 3, 4, 5, 6, 7) or (x >> 26 == 0 and x & 0x3f in (8, 9))
    pro = {base + 4 * i for i, x in enumerate(w) if x >> 16 == 0x27bd and x & 0x8000
           and (i == 0 or not slot(w[i - 1]) or base + 4 * i in jal)}
    return jal, {pre_prologue(w, base, a) if a not in jal else a for a in pro}


def pre_prologue(w, base, a, back=12):
    """gcc schedules global loads / early-out tests before `addiu sp`: if the previous `jr ra` (+ delay slot) is at most
    `back` instructions up and nothing between is a jump, call or trap, the function starts after that delay slot
    (RECOMP_NOTES '2026-10-26 Analyzer seeds were never read')."""
    i = (a - base) // 4
    for k in range(i - 1, max(i - back - 3, -1), -1):
        x = w[k]
        if x == 0x03e00008:                      # jr ra
            s = k + 2
            while s < i and w[s] == 0: s += 1    # alignment nops between functions
            return base + 4 * s
        if x >> 26 in (2, 3) or (x >> 26 == 0 and x & 0x3f in (8, 9, 12, 13)):  # j, jal, jr, jalr, syscall, break
            return a
    return a


def stubs(b, base):
    """BIOS A0/B0/C0 call stubs `addiu t2,zero,0xN0; jr t2; addiu t1,zero,fn`: the generator's exact-entry walk only accepts them as
    callable when marked `call_root` (else "interior seed has no host function" and they fall to the interpreter)."""
    w = struct.unpack('<%dI' % (len(b) // 4), b[:len(b) // 4 * 4])
    return [base + 4 * i for i in range(len(w) - 2)
            if w[i] >> 8 == 0x240a00 and w[i] & 0xff in (0xa0, 0xb0, 0xc0) and w[i + 1] == 0x01400008 and w[i + 2] >> 8 == 0x240900]


MOD = {'A': 'e1c0', 'B': 'e2129c0', 'C': 'e2576c0'}
SEED_KINDS = {'const_target', 'static_slot_init', 'static_slot_stored', 'fnptr_table', 'arg_register', 'struct_member',
              'vtable_dispatch', 'global_struct_member'}   # call-type kinds only; jump_table_* targets are inside a function


def indirect_seeds():
    """Call targets from config/indirect_targets.tsv (written by analysis/indirect.py) -> ({exe addrs}, {module id: {addrs}}).
    A target token is `addr` (in the site's own image, or the exe), `P:addr` (program P, from a boot-exe site) or `<module id>:addr` (an overlay module)."""
    exe, mods = set(), {m: set() for m in MOD.values()}
    global TRUSTED; TRUSTED = set()   # (module id, addr) of high-confidence rows: these become call_root when no jal/prologue found them
    f = P('config', 'indirect_targets.tsv')
    if not os.path.exists(f): return exe, mods
    for l in open(f):
        if l.startswith('#'): continue
        pc, prog, kind, tg, conf = l.rstrip().split(chr(9))[:5]
        if kind not in SEED_KINDS or not tg: continue
        for t in tg.split(','):
            pp, a = (t.split(':') if ':' in t else (prog, t)); a = int(a, 16)
            if EXE_BASE <= a < EXE_END: exe.add(a)
            elif (pp in MOD or pp.startswith('e')) and a >= 0x8002f810 and a % 4 == 0: mods.setdefault(MOD.get(pp, pp), set()).add(a); conf == 'high' and TRUSTED.add((MOD.get(pp, pp), a))
    return exe, mods


def extra_seeds():
    """Hand-verified entries from config/seeds_extra.txt: {module id: {addr}} (`# module <id>` sections, `addr  # evidence` lines)."""
    out, cur = {}, None
    for l in open(P('config', 'seeds_extra.txt')):
        if l.startswith('# module '): cur = l.split()[2]; out[cur] = set()
        elif l.strip() and not l.startswith('#'): out[cur].add(int(l.split()[0], 16))
    return out


def load_mods():
    """[(overlays.json entry, decompressed bytes)] for each distinct module (local discs only)."""
    import iso, toc
    D = iso.Disc(PD('roms', 'disc1', 'Star Ocean - The Second Story (USA) (Disc 1).bin')); w, sz = toc.load(D)
    mods, seen, cache = [], set(), {}
    for o in json.load(open(P('config', 'overlays.json')))['overlays']:
        if o['id'] == 'boot_exe' or (o['load_addr'], o['md5']) in seen:
            continue
        seen.add((o['load_addr'], o['md5']))
        e = o['toc_entry']
        if e not in cache:
            l, n = toc.ent(w, sz, e); cache[e] = slz.dec_all(D.read(l, n * 2048))
        mods.append((o, cache[e][o['chunk']]))
    return mods


def main():
    exe = open(PD('extracted', 'SCUS_944.21'), 'rb').read()[0x800:0x800 + 0x1f800]
    mods = load_mods()
    matched = [l.rstrip('\n').split('\t') for l in open(P('config', 'matches.tsv')) if not l.startswith('#')]
    mset = {int(m[1], 16): (m[0], int(m[2])) for m in matched}
    # an `addiu sp` that gcc scheduled into a byte-matched body of program A is not an entry (RECOMP_NOTES 2026-10-26)
    inside = lambda t: any(a < t < a + n for a, (_, n) in mset.items())
    jal0, pro0 = scan(exe, EXE_BASE)
    inexe = lambda t: EXE_BASE <= t < EXE_END and t % 4 == 0
    exe_seeds = {t for t in jal0 if inexe(t)} | pro0 | {0x80010008}
    sections, cross, scanned = [], 0, []
    mods_by_id = [(o['id'], b) for o, b in mods]
    ix_exe, ix_mod = indirect_seeds()
    extra = extra_seeds()
    for o, b in mods:
        base = int(o['load_addr'], 16)
        jal, pro = scan(b, base)
        drop = {t for t in pro if inside(t)} - jal if o['id'] == 'e1c0' else set()
        # only the module's own bytes seed it, and a jal counts only when it sits in code reached from a verified entry
        # (a jal word in data seeded wrong entries, e.g. E3107C0 0x80040004; RECOMP_NOTES 2026-10-27 overlay seeds)
        trusted = {a for a in set(ix_mod.get(o['id'], ())) | extra.get(o['id'], set()) | (set(mset) if o['id'] == 'e1c0' else set())
                   if base <= a < base + len(b) and a % 4 == 0}
        trusted |= {a for a in stubs(b, base) if a in jal}   # BIOS stubs end in `jr t2`, so the walk cannot accept them
        own, found, vj, _ = seedcheck.verify(seedcheck.words(b), base, pro - drop, trusted)
        cross_t = {t for t in vj if inexe(t)}
        exe_seeds |= cross_t; cross += len(cross_t)
        if o['id'] == 'e1c0':
            print('e1c0: prologue seeds inside matched functions dropped:', ' '.join(f'{t:08x}' for t in sorted(drop)))
        sections.append((o, base, sorted(own))); scanned.append(found)
    # call_root: BIOS stubs, plus high-confidence indirect-only call targets (no jal, no prologue found: the exact walk cannot start a host function there unless trusted)
    croots = {o['id']: sorted(set(a for a in stubs(b, int(o['load_addr'], 16)) if a in ents) |
                              {a for m, a in TRUSTED if m == o['id'] and int(o['load_addr'], 16) <= a < int(o['load_addr'], 16) + len(b) and a not in sc})
              for (o, b), (_, _, ents), sc in zip(mods, sections, scanned)}
    exe_seeds |= ix_exe
    sections = [(o, base, sorted(set(ents) | extra.get(o['id'], set()) | {a for a in ix_mod.get(o['id'], ()) if base <= a < base + len(dict(mods_by_id)[o['id']])}))
                for o, base, ents in sections]
    main_a = next(s for s in sections if s[0]['id'] == 'e1c0')
    # splat labels (local build/asm only)
    splat = set()
    asm = PD('build', 'asm', 'main_code.s')
    if os.path.exists(asm):
        splat = {int(m, 16) for m in re.findall(r'^glabel func_([0-9A-Fa-f]{8})', open(asm).read(), re.M)}
        main_a = (main_a[0], main_a[1], sorted(set(main_a[2]) | splat))
        sections = [main_a if s[0]['id'] == 'e1c0' else s for s in sections]
    with open(P('config', 'seeds.txt'), 'w', newline='\n') as f:
        f.write(f'# Boot exe SCUS_944.21 (0x80010000..0x8002f800) function-entry seeds: {len(exe_seeds)} addresses.\n'
                '# Source: analysis/make_seeds.py = jal targets (exe + all overlays) + addiu-sp prologues + header entry pc. Heuristic.\n')
        f.writelines(f'{a:08x}\n' for a in sorted(exe_seeds))
    with open(P('config', 'seeds_overlays.txt'), 'w', newline='\n') as f:
        f.write('# Per-module function-entry seeds (module id = overlays.json id). One address per line; a "# module" line starts a section.\n'
                '# NOT for psxrecomp [recompiler] seeds (it only accepts the exe image); feed per-overlay via the AOT profile entries.\n')
        for o, base, ents in sections:
            f.write(f"# module {o['id']} load={o['load_addr']} size={o['size']} seeds={len(ents)}\n")
            f.writelines(f'{a:08x}\n' for a in ents)
    with open(P('config', 'seeds_callroot.txt'), 'w', newline=chr(10)) as f:   # read by recomp/sandbox/stage_overlays.py
        f.write('# call_root directives per module (psxrecomp seed directive; replaces the plain seed line of the same address).' + chr(10) +
                '# Generated by analysis/make_seeds.py: BIOS A0/B0/C0 stubs that are seeds (RECOMP_NOTES 2026-10-26 Host generation B/C).' + chr(10))
        for mid, cr in croots.items():
            if cr:
                f.write(f'# module {mid}' + chr(10))
                f.writelines(f'call_root 0x{a:08x}' + chr(10) for a in cr)
    syms = [(0x80010008, 'BootEntry', 'guessed', 'PS-X header initial pc')]
    syms += [(a, mset[a][0], 'confirmed', f'program A (e1c0) only; byte-matched {mset[a][1]} B, gcc-2.7.2-psx -O2 -G0 (config/matches.tsv)',
              mset[a][1]) for a in sorted(mset)]
    syms += [(a, f'func_{a:08X}', 'guessed', 'splat label, main program A') for a in sorted(splat) if a not in mset]
    with open(P('config', 'symbols.toml'), 'w', newline='\n') as f:
        f.write('# psxrecomp symbols.toml (status vocabulary: guessed | confirmed | hot). Generated by analysis/make_seeds.py.\n'
                '# pc/name/status only. emit=false everywhere until a symbol is proven safe to own for AOT.\n')
        for pc, name, st, note, *size in syms:
            sz = f'size = {size[0]}\n' if size else ''  # extra key: psxrecomp load_symbols reads only pc/name/status/note/emit
            f.write(f'\n[[func]]\npc = 0x{pc:08X}\nname = "{name}"\n{sz}emit = false\nstatus = "{st}"\nnote = "{note}"\n')
    print(f'exe seeds {len(exe_seeds)} (of which cross-module call targets added: {len(exe_seeds) - len(jal0 & {t for t in jal0 if inexe(t)} | pro0)})')
    print(f'modules {len(sections)}; splat labels {len(splat)}; symbols {len(syms)} (confirmed {len(mset)})')
    for o, base, ents in sections:
        print(f"  {o['id']:>10} {o['load_addr']} {o['size']:>7} B  {len(ents):>5} seeds  {o['confidence']}")


if __name__ == '__main__':
    main()
