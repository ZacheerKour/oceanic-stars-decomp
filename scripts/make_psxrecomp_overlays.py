"""config/overlays.json -> psxrecomp AOT profile ("psxrecomp AOT methods v1"), default recomp/aot/overlays.json.
Run: python -I scripts/make_psxrecomp_overlays.py [out.json]
Known-real fields (checked against tools/ref/psxrecomp/tools/aot_overlay_pipeline.py): schema, game_id, disc_set[{name,disc_hashes}],
expected_records, images[{method, ...}], checks[]. GUESSED (no upstream support): the image method "slz_members" and its fields;
the disc hashes are placeholders. Keys starting with x_ are our own notes. See docs/overlay_schema.md."""
import os, sys, json
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'recomp', 'aot', 'overlays.json')
ov = json.load(open(os.path.join(ROOT, 'config', 'overlays.json')))['overlays']

members, seen, exe = [], {}, None
for o in ov:
    if o['id'] == 'boot_exe':
        exe = o; continue
    key = (o['load_addr'], o['sha256'])
    if key in seen:  # identical bytes at the same address: one record, extra ids become aliases (psxrecomp dedupes by (base, bytes))
        seen[key]['aliases'].append(o['id']); continue
    seen[key] = dict(name=o['id'], toc_entry=o['toc_entry'], chunk=o['chunk'], load_addr=o['load_addr'],
                     size=o['size'], decoded_sha256=o['sha256'], aliases=[], x_confidence=o['confidence'], x_role=o['role'])
    members.append(seen[key])

doc = dict(
    schema='psxrecomp AOT methods v1', game_id='SCUS-94421',
    x_todo=['TODO(verify) disc_hashes: sha256 of each original data track', 'GUESSED: images[].method "slz_members" does not exist upstream; '
            'it is the proposed SLZ-in-SO2.BIN extractor (docs/SLZ.md, docs/SO2BIN_TOC.md)',
            'GUESSED: expected_records = 1 exe + unique (load_addr, sha256) members; upstream may count compositions/aliases differently'],
    disc_set=[dict(name='disc1', disc_hashes=dict(sha256='TODO(verify)')), dict(name='disc2', disc_hashes=dict(sha256='TODO(verify)'))],
    expected_records=1 + len(members),
    images=[
        dict(method='psx_exe', files=['SCUS_944.21']),
        dict(method='slz_members', container='SO2.BIN', toc_decrypt='so2bin_v1', discs_identical=True,
             members=members),
    ],
    checks=[])
os.makedirs(os.path.dirname(out), exist_ok=True)
json.dump(doc, open(out, 'w', newline='\n'), indent=1); open(out, 'a').write('\n')
print(f'{out}: 1 exe + {len(members)} unique overlay images ({len(ov) - 1} chunks)')
