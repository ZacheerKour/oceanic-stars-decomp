"""config/overlays.tsv -> config/overlays.json (stable schema, docs/overlay_schema.md).
Run from the repo root:  python -I analysis/overlay_json.py
sha256 comes from decoding the local discs (roms/, gitignored); without them the sha256 of an existing overlays.json is kept.
Output carries ids, addresses, sizes and hashes only (no game bytes)."""
import os, sys, csv, json, hashlib
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(ROOT, 'analysis'))
import slz

TSV, OUT = os.path.join(ROOT, 'config', 'overlays.tsv'), os.path.join(ROOT, 'config', 'overlays.json')
DISC1 = os.path.join(ROOT, 'roms', 'disc1', 'Star Ocean - The Second Story (USA) (Disc 1).bin')


def oid(entry, chunk):
    return 'boot_exe' if entry == 'boot_exe' else f'e{entry}c{chunk}'


def main():
    old = {}
    if os.path.exists(OUT):
        old = {o['id']: o.get('sha256') for o in json.load(open(OUT))['overlays']}
    D = None
    if os.path.exists(DISC1):
        import iso, toc
        D = iso.Disc(DISC1); w, sz = toc.load(D)
    cache, rows = {}, []
    for r in csv.reader((l for l in open(TSV) if not l.startswith('#')), delimiter='\t'):
        ent, disc, chunk, load, size, md5, role, conf, basis, ident, loader = r[:11]
        chunk, size = int(chunk), int(size)
        i = oid(ent, chunk)
        sha = old.get(i)
        if D and ent != 'boot_exe':
            e = int(ent)
            if e not in cache:
                l, n = toc.ent(w, sz, e); cache[e] = slz.dec_all(D.read(l, n * 2048))
            sha = hashlib.sha256(cache[e][chunk]).hexdigest()
        elif D and ent == 'boot_exe':
            p = os.path.join(ROOT, 'extracted', 'SCUS_944.21')
            if os.path.exists(p):
                sha = hashlib.sha256(open(p, 'rb').read()[0x800:0x800 + size]).hexdigest()
        rows.append(dict(
            id=i, discs=[int(x) for x in disc.split('+')],
            toc_entry=None if ent == 'boot_exe' else int(ent), chunk=chunk,
            load_addr=load, size=size, sha256=sha, md5=md5, role=role, confidence=conf, basis=basis,
            identical_to=[oid(x, chunk) for x in ident.replace(' ', '').split(',') if x],
            loader=loader or None))
    # symmetric/transitive identity by md5 (the tsv column only lists some partners)
    by = {}
    for o in rows:
        by.setdefault(o['md5'], []).append(o['id'])
    for o in rows:
        o['identical_to'] = sorted(set(o['identical_to']) | {x for x in by[o['md5']] if x != o['id']})
    doc = dict(schema='so2 overlays v1', game='Star Ocean: The Second Story (USA)', container='SO2.BIN',
               serials=['SCUS-94421', 'SCUS-94422'], overlays=rows)
    json.dump(doc, open(OUT, 'w', newline='\n'), indent=1); open(OUT, 'a').write('\n')
    print(len(rows), 'overlays;', sum(1 for o in rows if o['sha256']), 'with sha256')


if __name__ == '__main__':
    main()
