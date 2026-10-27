"""Run psxrecomp's `aot_overlay_pipeline.py static` once per SLZ chunk (one-chunk profiles cut from recomp/aot/overlays.json)
and tabulate the generator counts.  Runs against a COPY of the audited psxrecomp clone (never tools/ref/psxrecomp).
    python -E -s recomp/aot/run_static_all.py --mode heur|ours|final --copy <psxrecomp copy> --recompiler <psxrecomp-game.exe>
        --cue <disc 1 cue> --cmake <cmake.exe> [--only E1C0,E2129C0] [--combined NAME]
heur: the pipeline's own entry guesses (prologues, leaves, supplemental seeds, jal roots, root enrichment).
ours: only config/seeds_overlays.txt + config/indirect_targets.tsv call targets + the 3 A BIOS-stub call roots, A data block
      dropped; needs the copy's `x_entries_only` spec key (2 lines in make_fixed_record) and PSX_OVERLAY_ROOT_ENRICHMENT=0.
final: config/seeds_final/<CHUNK>.txt (analysis/classify_seeds.py output) + the A stubs, same pipeline switches as ours.
Writes <copy>/_work/<mode>/<chunk>/ (work dir, log, profile) and <copy>/_work/static_<mode>/<chunk>/ (C), plus
<copy>/_work/<mode>/summary.tsv.  Nothing is deleted; reruns make new disc-aot-* dirs and overwrite the published C."""
import argparse, json, os, re, subprocess, sys, time
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'analysis'))
from make_seeds import indirect_seeds  # noqa: E402  (import has no side effects)

A_STUBS = (0x8004B074, 0x8004B084, 0x8004B094)       # BIOS B0 stubs, jal targets not preceded by jr ra
A_DATA = (0x800509DC, 0x80061C58)                     # A data block (MDEC tables + data), RECOMP_NOTES 2026-10-26
KEYS = {'seeds': r'^  seeds: (\d+)', 'fn_entry_pcs': r'^function_entry_pcs: (\d+)', 'functions': r'^Function definitions: (\d+)',
        'todo': r'^Unsupported instruction TODOs: (\d+)', 'bad': r'^Unknown/bad targets: (\d+)',
        'nosplit': r'^no_split_guard_demoted: (\d+)', 'identities': r'\((\d+) exact function identities total',
        'tus': r'identities total, (\d+) translation', 'failed': r'^PSX_SHARD_RESULT ok=\d+ failed=(\d+)'}


def final_seeds():
    """config/seeds_final/<CHUNK>.txt (analysis/classify_seeds.py): our seeds + verified psxrecomp-discovered extras; the 3 A stubs and the A data filter as in ours."""
    out = {}
    for f in os.listdir(os.path.join(ROOT, 'config', 'seeds_final')):
        if f.endswith('.txt'):
            out[f[:-4]] = {int(l, 16) for l in open(os.path.join(ROOT, 'config', 'seeds_final', f)) if l.strip() and l[0] != '#'}
    out['E1C0'] |= set(A_STUBS)
    return {k: sorted(a for a in v if not (k == 'E1C0' and A_DATA[0] <= a < A_DATA[1])) for k, v in out.items()}


def our_seeds():
    mods, cur = {}, None
    for line in open(os.path.join(ROOT, 'config', 'seeds_overlays.txt')):
        m = re.match(r'# module (\S+) load=(0x[0-9a-f]+) size=(\d+)', line)
        if m:
            cur = m.group(1); mods[cur] = (int(m.group(2), 16), int(m.group(3)), set())
        elif cur and line.strip() and not line.startswith('#'):
            mods[cur][2].add(int(line, 16))
    for mod, addrs in indirect_seeds()[1].items():
        mods[mod][2].update(addrs)
    mods['e1c0'][2].update(A_STUBS)
    out = {}
    for mod, (base, size, s) in mods.items():
        out[mod.upper()] = sorted(a for a in s if base <= a < base + size and a % 4 == 0
                                  and not (mod == 'e1c0' and A_DATA[0] <= a < A_DATA[1]))
    return out


def profile_for(full, names, mode, ours):
    images = []
    for name in names:
        img = next(i for i in full['images'] if 'SLZ/' + name in i['files'])
        image = {**img, 'files': ['SLZ/' + name]}
        if mode in ('ours', 'final'):
            image.update(entries=[f'0x{a:08x}' for a in ours.get(name, [])], x_entries_only=True, supplemental_entries=False)
        images.append(image)
    files = {'SLZ/' + n for n in names}
    p = {k: v for k, v in full.items() if k not in ('prepared_files', 'images')}
    p.update(prepared_files=[f for f in full['prepared_files'] if f['name'] in files], images=images, expected_records=len(names))
    return p


def main():
    ap = argparse.ArgumentParser()
    for a in ('--copy', '--recompiler', '--cue', '--cmake'):
        ap.add_argument(a, required=True)
    ap.add_argument('--mode', choices=('heur', 'ours', 'final'), required=True)
    ap.add_argument('--only', default='')
    ap.add_argument('--combined', default='', help='run all selected chunks as ONE profile, output under this name')
    ap.add_argument('--timeout', type=int, default=3600)
    ap.add_argument('--drop', default='', help='ours: CHUNK:hexaddr,... seeds to leave out (e.g. ones the audit rejects as data)')
    args = ap.parse_args()
    full = json.load(open(os.path.join(ROOT, 'recomp', 'aot', 'overlays.json')))
    names = [f['name'][4:] for f in full['prepared_files']]
    if args.only:
        names = [n for n in names if n in args.only.split(',')]
    ours = {'ours': our_seeds, 'final': final_seeds}.get(args.mode, dict)()
    for item in filter(None, args.drop.split(',')):
        chunk, addr = item.split(':')
        ours[chunk] = [a for a in ours[chunk] if a != int(addr, 16)]
    assert args.mode == 'heur' or set(names) <= set(ours), sorted(set(names) - set(ours))
    work = os.path.join(args.copy, '_work', args.mode)
    os.makedirs(work, exist_ok=True)
    env = dict(os.environ, PSX_OVERLAY_ROOT_ENRICHMENT='1' if args.mode == 'heur' else '0')
    rows = []
    # --combined: one profile with every selected chunk -> one dispatcher (overlays_static.c) the runtime can link;
    # per-chunk runs each emit their own psx_overlay_dispatch and cannot be linked together.
    groups = [(args.combined, names)] if args.combined else [(n, [n]) for n in names]
    for name, members in groups:
        d = os.path.join(work, name)
        os.makedirs(d, exist_ok=True)
        prof = os.path.join(d, 'profile.json')
        json.dump(profile_for(full, members, args.mode, ours), open(prof, 'w'), indent=1)
        cmd = [sys.executable, '-E', '-s', os.path.join(args.copy, 'tools', 'aot_overlay_pipeline.py'), 'static',
               '--profile', prof, '--game-toml', os.path.join(args.copy, '_work', 'proj', 'game.toml'), '--disc', args.cue,
               '--recompiler', args.recompiler, '--work-dir', d, '--out-dir',
               os.path.join(args.copy, '_work', 'static_' + args.mode, name), '--cmake', args.cmake, '--workers', '4']
        t0, rc = time.time(), None
        with open(os.path.join(d, 'run.log'), 'w') as log:
            try:
                rc = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=args.timeout).returncode
            except subprocess.TimeoutExpired:
                rc = 'timeout'
        row = {'chunk': name, 'rc': rc, 'secs': round(time.time() - t0),
               'our_seeds': sum(len(ours.get(m, [])) for m in members) or ''}
        logs = sorted((os.path.join(d, x, 'static-compile.log') for x in os.listdir(d) if x.startswith('disc-aot-')),
                      key=lambda p: os.path.getmtime(p) if os.path.exists(p) else 0)
        text = open(logs[-1], errors='replace').read() if logs and os.path.exists(logs[-1]) else ''
        for k, rx in KEYS.items():
            m = re.search(rx, text, re.M)
            row[k] = m.group(1) if m else ''
        row['warnings'] = len(re.findall(r'warning', text, re.I))
        rows.append(row)
        print('\t'.join(str(v) for v in row.values()), flush=True)
    with open(os.path.join(work, 'summary.tsv'), 'w') as f:
        f.write('\t'.join(rows[0]) + '\n')
        f.writelines('\t'.join(str(v) for v in r.values()) + '\n' for r in rows)
    bad = [r['chunk'] for r in rows if r['rc'] != 0]
    print(f'DONE mode={args.mode} chunks={len(rows)} failed={len(bad)} {",".join(bad)}', flush=True)


if __name__ == '__main__':
    main()
