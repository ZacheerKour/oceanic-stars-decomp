"""If the local (gitignored) discs exist, decode every overlay in config/overlays.tsv from SO2.BIN and verify its md5
(and sha256 when config/overlays.json has it).  Skips cleanly (exit 0) when roms\ is absent.  Run: python -I tests/slz/check_local.py"""
import os, sys, hashlib, csv, json
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'analysis'))
disc = os.path.join(ROOT, 'roms', 'disc1', 'Star Ocean - The Second Story (USA) (Disc 1).bin')
if not os.path.exists(disc):
    print('SKIP: local disc image not found (roms/disc1); nothing verified'); sys.exit(0)
import iso, toc, slz
D = iso.Disc(disc); w, sz = toc.load(D)
sha = {}
jp = os.path.join(ROOT, 'config', 'overlays.json')
if os.path.exists(jp):
    sha = {o['id']: o['sha256'] for o in json.load(open(jp))['overlays'] if o.get('sha256')}
cache, ok, bad, skipped = {}, 0, 0, 0
for r in csv.reader((l for l in open(os.path.join(ROOT, 'config', 'overlays.tsv')) if not l.startswith('#')), delimiter='\t'):
    ent, chunk, md5 = r[0], int(r[2]), r[5]
    if ent == 'boot_exe':
        data = open(os.path.join(ROOT, 'extracted', 'SCUS_944.21'), 'rb').read()[0x800:0x800 + int(r[4])] \
            if os.path.exists(os.path.join(ROOT, 'extracted', 'SCUS_944.21')) else None
        if data is None: skipped += 1; continue
    else:
        i = int(ent)
        if i not in cache:
            l, n = toc.ent(w, sz, i); cache[i] = slz.dec_all(D.read(l, n * 2048))
        data = cache[i][chunk]
    good = hashlib.md5(data).hexdigest() == md5 and len(data) == int(r[4])
    key = ent if ent == 'boot_exe' else f'e{ent}c{chunk}'
    if key in sha: good = good and hashlib.sha256(data).hexdigest() == sha[key]
    ok += good; bad += not good
    if not good: print('MISMATCH', ent, chunk)
print(f'overlays verified: {ok} ok, {bad} bad, {skipped} skipped')
sys.exit(1 if bad else 0)
