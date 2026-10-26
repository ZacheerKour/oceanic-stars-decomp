"""Host side, stdlib only (run as: python -I stage_overlays.py <repo> <stage_dir>).
Wraps the decompressed main programs A/B/C (SO2.BIN entries 1/2129/2576) in a minimal PS-X EXE header and
writes per-program seed lists into <stage_dir>. Prints only names, sizes and hashes."""
import hashlib, os, struct, sys
from pathlib import Path

REPO, STAGE = Path(sys.argv[1]), Path(sys.argv[2])
DATA = Path(os.environ.get('SO2_DATA', REPO))  # gitignored analysis/out lives in the main checkout
LOAD, SP = 0x8002F810, 0x801FFFF0
# name -> (decompressed file, TOC entry, seeds module id in config/seeds_overlays.txt)
PROGS = {'A': ('0001_312.dec', 1, 'e1c0'), 'B': ('2129_24323.dec', 2129, 'e2129c0'), 'C': ('2576_30736.dec', 2576, 'e2576c0')}

ovl = {}
for ln in (REPO / 'config/overlays.tsv').read_text().splitlines():
    f = ln.split('\t')
    if not ln.startswith('#') and len(f) > 5 and f[2] == '0' and f[3] == '0x8002f810':
        ovl[int(f[0])] = (int(f[4]), f[5])
seeds = {}
cur = None
for ln in (REPO / 'config/seeds_overlays.txt').read_text().splitlines():
    if ln.startswith('# module '):
        cur = ln.split()[2]; seeds[cur] = []
    elif ln.strip() and not ln.startswith('#') and cur:
        seeds[cur].append(ln.strip())

for name, (fn, entry, mod) in PROGS.items():
    raw = (DATA / 'analysis/out/d1' / fn).read_bytes()
    size, md5 = ovl[entry]
    if len(raw) != size or hashlib.md5(raw).hexdigest() != md5:
        sys.exit(f'{name}: size/md5 mismatch vs config/overlays.tsv')
    assert len(raw) % 4 == 0
    hdr = bytearray(2048)
    hdr[:8] = b'PS-X EXE'
    # pc = load address: the first function of every program (first line of its seed list); gp 0 as in the boot exe
    struct.pack_into('<IIII', hdr, 0x10, LOAD, 0, LOAD, len(raw))
    struct.pack_into('<I', hdr, 0x30, SP)
    exe = bytes(hdr) + raw
    s = [a for a in seeds[mod] if LOAD <= int(a, 16) < LOAD + len(raw)]
    assert s and int(s[0], 16) == LOAD, f'{name}: seed list does not start at load address'
    (STAGE / f'{name}.exe').write_bytes(exe)
    # 0x prefix: psxrecomp-analyze parses seeds with stoul base 0 (bare hex is rejected or read as decimal)
    (STAGE / f'{name}.seeds.txt').write_text(''.join(f'0x{a}\n' for a in s))
    print(f'{name} entry={entry} exe_sha256={hashlib.sha256(exe).hexdigest()} seeds={len(s)}')
