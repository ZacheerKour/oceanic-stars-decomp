"""Function-entry seeds + symbols.toml from the LOCAL (gitignored) discs/extracted exe.  Run from the repo root:
    python -I analysis/make_seeds.py
Writes config/seeds.txt (boot exe, psxrecomp seeds format: one hex address per line), config/seeds_overlays.txt (per-module
sections) and config/symbols.toml (29 byte-matched functions = confirmed, splat function labels of the main overlay = guessed).
Output has addresses and names only.  Heuristic: jal targets inside a module + `addiu sp,sp,-N` prologues (false positives
possible in data; psxrecomp validates seeds).  Also merges the call-type targets of config/indirect_targets.tsv
(written by analysis/indirect.py; optional, idempotent).  Needs roms/disc1 + extracted/SCUS_944.21; splat labels need build/asm/main_code.s."""
import os, re, sys, json, struct
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(ROOT, 'analysis'))
import slz

EXE_BASE, EXE_END = 0x80010000, 0x8002f800
P = lambda *a: os.path.join(ROOT, *a)


def scan(b, base):
    n = len(b) // 4
    w = struct.unpack('<%dI' % n, b[:n * 4])
    jal = {((x & 0x3ffffff) << 2) | 0x80000000 for x in w if x >> 26 == 3}
    pro = {base + 4 * i for i, x in enumerate(w) if x >> 16 == 0x27bd and x & 0x8000}
    return jal, pro


MOD = {'A': 'e1c0', 'B': 'e2129c0', 'C': 'e2576c0'}
SEED_KINDS = {'const_target', 'static_slot_init', 'static_slot_stored', 'fnptr_table', 'arg_register', 'struct_member',
              'vtable_dispatch', 'global_struct_member'}   # call-type kinds only; jump_table_* targets are inside a function


def indirect_seeds():
    """Call targets from config/indirect_targets.tsv (written by analysis/indirect.py) -> ({exe addrs}, {module id: {addrs}}).
    A target token is `addr` (in the site's own image, or the exe) or `P:addr` (program P, from a boot-exe site)."""
    exe, mods = set(), {m: set() for m in MOD.values()}
    f = P('config', 'indirect_targets.tsv')
    if not os.path.exists(f): return exe, mods
    for l in open(f):
        if l.startswith('#'): continue
        pc, prog, kind, tg = l.rstrip().split(chr(9))[:4]
        if kind not in SEED_KINDS or not tg: continue
        for t in tg.split(','):
            pp, a = (t.split(':') if ':' in t else (prog, t)); a = int(a, 16)
            if EXE_BASE <= a < EXE_END: exe.add(a)
            elif pp in MOD and a >= 0x8002f810 and a % 4 == 0: mods[MOD[pp]].add(a)
    return exe, mods


def main():
    import iso, toc
    exe = open(P('extracted', 'SCUS_944.21'), 'rb').read()[0x800:0x800 + 0x1f800]
    D = iso.Disc(P('roms', 'disc1', 'Star Ocean - The Second Story (USA) (Disc 1).bin')); w, sz = toc.load(D)
    mods, seen, cache = [], set(), {}
    for o in json.load(open(P('config', 'overlays.json')))['overlays']:
        if o['id'] == 'boot_exe' or (o['load_addr'], o['md5']) in seen:
            continue
        seen.add((o['load_addr'], o['md5']))
        e = o['toc_entry']
        if e not in cache:
            l, n = toc.ent(w, sz, e); cache[e] = slz.dec_all(D.read(l, n * 2048))
        mods.append((o, cache[e][o['chunk']]))
    jal0, pro0 = scan(exe, EXE_BASE)
    inexe = lambda t: EXE_BASE <= t < EXE_END and t % 4 == 0
    exe_seeds = {t for t in jal0 if inexe(t)} | pro0 | {0x80010008}
    sections, cross = [], 0
    mods_by_id = [(o['id'], b) for o, b in mods]
    for o, b in mods:
        base = int(o['load_addr'], 16)
        jal, pro = scan(b, base)
        cross_t = {t for t in jal if inexe(t)}
        exe_seeds |= cross_t; cross += len(cross_t)
        own = {t for t in jal if base <= t < base + len(b) and t % 4 == 0} | pro
        sections.append((o, base, sorted(own)))
    ix_exe, ix_mod = indirect_seeds()
    exe_seeds |= ix_exe
    sections = [(o, base, sorted(set(ents) | {a for a in ix_mod.get(o['id'], ()) if a < base + len(dict(mods_by_id)[o['id']])}))
                for o, base, ents in sections]
    main_a = next(s for s in sections if s[0]['id'] == 'e1c0')
    # splat labels (local build/asm only)
    splat = set()
    asm = P('build', 'asm', 'main_code.s')
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
    matched = [l.split('\t') for l in open(P('config', 'matches.tsv')) if not l.startswith('#')]
    mset = {int(m[1], 16): m[0] for m in matched}
    syms = [(0x80010008, 'BootEntry', 'guessed', 'PS-X header initial pc')]
    syms += [(a, mset[a], 'confirmed', 'byte-matched, gcc-2.7.2-psx -O2 -G0 (config/matches.tsv)') for a in sorted(mset)]
    syms += [(a, f'func_{a:08X}', 'guessed', 'splat label, main program A') for a in sorted(splat) if a not in mset]
    with open(P('config', 'symbols.toml'), 'w', newline='\n') as f:
        f.write('# psxrecomp symbols.toml (status vocabulary: guessed | confirmed | hot). Generated by analysis/make_seeds.py.\n'
                '# pc/name/status only. emit=false everywhere until a symbol is proven safe to own for AOT.\n')
        for pc, name, st, note in syms:
            f.write(f'\n[[func]]\npc = 0x{pc:08X}\nname = "{name}"\nemit = false\nstatus = "{st}"\nnote = "{note}"\n')
    print(f'exe seeds {len(exe_seeds)} (of which cross-module call targets added: {len(exe_seeds) - len(jal0 & {t for t in jal0 if inexe(t)} | pro0)})')
    print(f'modules {len(sections)}; splat labels {len(splat)}; symbols {len(syms)} (confirmed {len(mset)})')
    for o, base, ents in sections:
        print(f"  {o['id']:>10} {o['load_addr']} {o['size']:>7} B  {len(ents):>5} seeds  {o['confidence']}")


if __name__ == '__main__':
    main()
