"""psxrecomp AOT profile with SLZ overlays as `prepared_files` (RECOMP_NOTES.md '2026-10-26 SLZ overlays into the AOT pipeline').
    python -I analysis/aot_profile.py [out.json]      (default recomp/aot/overlays.json; SO2_DATA = checkout holding roms/, default this one)
Every unique (load_addr, sha256) code chunk of config/overlays.json becomes one prepared file
{method "so2_slz", source SO2.BIN + byte offset of its chunk header} and is placed by a `fixed_address_files` image.
The SLZ decoder is analysis/slz.py `prepared` (the prepared_files decoder contract). Each prepared entry is re-checked here
exactly as tools/prepared_files.py checks it (consumed, source_sha256, size, sha256). Without the disc only the self-test runs.
Writes offsets, sizes and hashes only (no game bytes). Disc 1 only: disc 2 has no SO2.BIN in its ISO directory (ANALYSIS.md 1)."""
import os, sys, json, struct, hashlib
from collections import defaultdict
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(ROOT, 'analysis'))
import slz

DATA = os.environ.get('SO2_DATA', ROOT)
DISC1 = os.path.join(DATA, 'roms', 'disc1', 'Star Ocean - The Second Story (USA) (Disc 1).bin')
DISC1_SHA256 = 'ca9afe72699acd1994260fd00bedb0b017ac30678bfdc2b2b1022112fa872f33'   # analysis/disc_facts.py
sha = lambda b: hashlib.sha256(b).hexdigest()


def selftest():
    """slz.prepared on the synthetic vectors (first chunk) + rejection of malformed input."""
    v = os.path.join(ROOT, 'tests', 'slz', 'vectors')
    man = json.load(open(os.path.join(ROOT, 'tests', 'slz', 'manifest.json')))
    for m in man:
        blob = open(os.path.join(v, m['file']), 'rb').read()
        out, used = slz.prepared(blob)
        assert sha(out) == m['chunks'][0]['sha256'], m['name']
        assert used == 16 + struct.unpack_from('<I', blob, 4)[0] <= len(blob), m['name']
    for bad in (b'XYZ\x00' + bytes(12), bytes.fromhex('534c5a0104000000040000000000000000' '0500'),
                b'SLZ\x00' + (3).to_bytes(4, 'little') * 2 + bytes(4) + b'ab',          # v0 truncated
                b'SLZ\x02' + (2).to_bytes(4, 'little') + (5).to_bytes(4, 'little') + bytes(4) + b'\xff\x41\x42',  # payload overrun
                b'SLZ\x03' + (2).to_bytes(4, 'little') * 2 + bytes(4) + b'\xff\x41',  # unknown version
                b'SLZ\x02' + (2).to_bytes(4, 'little') + (0x200001).to_bytes(4, 'little') + bytes(4) + b'\xff\x41'):  # dsize > RAM
        try:
            slz.prepared(bad); raise AssertionError(f'accepted {bad.hex()}')
        except ValueError:
            pass
    return len(man)


def main():
    n = selftest()
    print(f'self-test: slz.prepared OK on {n} vectors + 6 malformed inputs')
    if not os.path.exists(DISC1):
        print('SKIP: no local disc 1 (roms/disc1); profile not written'); return 0
    import iso, toc
    D = iso.Disc(DISC1)
    so2 = next((l, s) for p, l, s, _ in D.walk()[1] if p.upper() == '/SO2.BIN;1')
    blob = D.read(*so2)
    w, sz = toc.load(D)
    prepared, places, seen = [], defaultdict(list), {}
    for o in json.load(open(os.path.join(ROOT, 'config', 'overlays.json')))['overlays']:
        if o['id'] == 'boot_exe':
            continue
        key = (o['load_addr'], o['sha256'])
        if key in seen:   # identical bytes at the same address: one producer (psxrecomp dedupes by (base, bytes))
            seen[key]['x_aliases'].append(o['id']); continue
        lba, _ = toc.ent(w, sz, o['toc_entry'])
        off = (lba - so2[0]) * 2048
        for _ in range(o['chunk']):
            off += struct.unpack_from('<I', blob, off + 12)[0]
        assert 0 <= off < so2[1], o['id']
        out, used = slz.prepared(blob[off:])
        assert len(out) == o['size'] and sha(out) == o['sha256'], f"{o['id']}: decoded bytes differ from config/overlays.json"
        rec = dict(name=f"SLZ/{o['id'].upper()}", method='so2_slz', source=dict(file='SO2.BIN', file_offset=hex(off)),
                   consumed=hex(used), source_sha256=sha(blob[off:off + used]), size=hex(len(out)), sha256=o['sha256'],
                   x_aliases=[], x_confidence=o['confidence'], x_role=o['role'])
        seen[key] = rec
        prepared.append(rec)
        places[o['load_addr']].append(rec['name'])
    # re-check every entry the way tools/prepared_files.py prepare() does
    for p in prepared:
        start, used = int(p['source']['file_offset'], 16), int(p['consumed'], 16)
        out, c = slz.prepared(blob[start:])
        assert (c, sha(blob[start:start + c]), len(out), sha(out)) == (used, p['source_sha256'], int(p['size'], 16), p['sha256']), p['name']
    images = [dict(method='psx_exe', files=['SCUS_944.21'])]
    images += [dict(method='fixed_address_files', load_addr=a, files=f, allow_missing=True) for a, f in sorted(places.items())]
    doc = dict(
        schema='psxrecomp AOT methods v1', game_id='SCUS-94421',
        x_notes=['needs the so2_slz prepared_files method registered in tools/prepared_files.py METHODS: '
                 "'so2_slz': (slz.prepared, ()) with analysis/slz.py importable (RECOMP_NOTES.md 2026-10-26)",
                 'disc 1 only: disc 2 has no SO2.BIN directory entry; its code chunks are byte-identical, so the shards serve both',
                 'TODO(verify) expected_records: 1 boot exe + one per unique chunk assumes the generic extractor finds the exe '
                 'as one recipe and no overlay; first extract run prints the real count'],
        disc_hashes=dict(sha256=DISC1_SHA256),
        prepared_files=prepared, images=images, expected_records=1 + len(prepared))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'recomp', 'aot', 'overlays.json')
    with open(out, 'w', newline='\n') as f:
        json.dump(doc, f, indent=1); f.write('\n')
    print(f'{out}: {len(prepared)} prepared SLZ chunks at {len(places)} load addresses, '
          f"{sum(len(p['x_aliases']) for p in prepared)} aliases; all re-decoded and hash-checked")
    return 0


if __name__ == '__main__':
    sys.exit(main())
