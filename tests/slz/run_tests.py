"""Round-trip + vector tests for analysis/slz.py.  Run from anywhere:  python -I tests/slz/run_tests.py
Checks: (1) every vector decodes (all chunks) to its expected bytes/sha256, (2) analysis/slz_encode.py reproduces
the vector bytes, (3) fresh encoder->decoder round trips on generated data, (4) malformed streams fail."""
import os, sys, json, hashlib
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'analysis'))
import slz, slz_encode as E
sys.path.insert(0, HERE)
import gen_vectors as G

fails = 0
def check(ok, msg):
    global fails
    if not ok:
        fails += 1; print('FAIL', msg)

man = json.load(open(os.path.join(HERE, 'manifest.json')))
for m in man:
    blob = open(os.path.join(HERE, 'vectors', m['file']), 'rb').read()
    outs = slz.dec_all(blob)
    check(len(outs) == len(m['chunks']), f"{m['name']}: chunk count {len(outs)}")
    for o, c in zip(outs, m['chunks']):
        exp = open(os.path.join(HERE, 'vectors', c['expected']), 'rb').read()
        check(o == exp and len(o) == c['size'] and hashlib.sha256(o).hexdigest() == c['sha256'], f"{m['name']}: output mismatch")
    if m['encoder_reproduces']:
        ch = [(open(os.path.join(HERE, 'vectors', c['expected']), 'rb').read(), c['ver']) for c in m['chunks']]
        check(E.encode_entry(ch) == blob, f"{m['name']}: encoder output differs from vector")
print(f'{len(man)} vectors checked')

# fresh round trips: patterned + random data, all versions
n = 0
for seed in range(40):
    r = G.lcg(1 + seed * 37, seed)
    for data in (r, r + r, bytes(b & 3 for b in r) * 3, b'\0' * seed + r[:seed]):
        for v in (0, 1, 2):
            n += 1
            check(slz.dec(E.encode_chunk(data, v))[0] == data, f'roundtrip v{v} seed {seed} len {len(data)}')
print(n, 'round trips')

# malformed: distance beyond produced output must raise (reference decoder behaviour)
bad = bytes.fromhex('534c5a01 04000000 04000000 00000000 00 0500'.replace(' ', ''))  # ref dist 5 with empty output
try:
    slz.dec(bad); check(False, 'bad distance accepted')
except IndexError:
    pass
check(slz.dec(b'SLZ\x00' + bytes(4) + (3).to_bytes(4, 'little') + bytes(4) + b'abc')[0] == b'abc', 'stored')
try:
    slz.dec(b'XYZ\x00' + bytes(12)); check(False, 'bad magic accepted')
except AssertionError:
    pass
print('FAILED' if fails else 'OK', fails)
sys.exit(1 if fails else 0)
