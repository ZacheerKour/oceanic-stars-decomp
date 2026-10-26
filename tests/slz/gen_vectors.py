"""Regenerate the SYNTHETIC SLZ test vectors (tests/slz/vectors + manifest.json). Deterministic, no game data.
Run from anywhere:  python -I tests/slz/gen_vectors.py"""
import os, sys, json, hashlib
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'analysis'))  # -I drops the script dir
import slz_encode as E

V = os.path.join(HERE, 'vectors')


def lcg(n, seed=12345):
    out = bytearray()
    for _ in range(n):
        seed = (seed * 1103515245 + 12345) & 0x7fffffff
        out.append(seed >> 16 & 255)
    return bytes(out)


def unique_run(n):  # no repeated 3-gram for n <= 256
    return bytes(range(n))


def maxdist(extra):
    head = b'\xfe\xfd\xfc'
    mid = bytes(b for b in lcg(4400) if b < 0xf0)[:4095 - 3 + extra]  # bytes < 0xf0 so the head never occurs inside
    return head + mid + head


def max_ref_dist(data, ver):
    return max([t[1] for t in E.tokens(data, ver) if t[0] == 'ref'] or [0])


CASES = []  # (name, [(data, ver)])
for v in (0, 1, 2):
    CASES.append((f'empty_v{v}', [(b'', v)]))
for v in (0, 1, 2):
    CASES.append((f'literals_v{v}', [(unique_run(100), v)]))
CASES.append(('overlap_v1', [(b'ab' * 100 + b'xyz', 1)]))
CASES.append(('overlap_v2', [(b'abc' * 80 + b'q', 2)]))
CASES.append(('long_len18_v1', [(b'k' + b'k' * 17 + b'#' + lcg(30), 1)]))
CASES.append(('fill_v2', [(b'\0' * 300 + b'x' * 10 + b'\7' * 19 + b'\7' * 18 + b'z' * 4 + b'z' * 3 + b'!', 2)]))
CASES.append(('maxdist_v1', [(maxdist(0), 1)]))
CASES.append(('maxdist_v2', [(maxdist(0), 2)]))
CASES.append(('beyond_window_v1', [(maxdist(1), 1)]))  # distance 4096: must NOT be a ref
CASES.append(('multichunk_mixed', [(lcg(50), 0), (b'abcabcabc' * 20 + lcg(20), 1), (b'\0' * 100 + lcg(33, 7), 2), (b'', 2)]))
CASES.append(('multichunk_v2_pair', [(b'hello hello hello hello ', 2), (b'world world world world ', 2)]))

# Hand-assembled streams (bytes written by hand from docs/SLZ.md, independent of the encoder).
HAND = [
    ('hand_v1_ref_term', bytes.fromhex('534c5a01 06000000 06000000 00000000 01 41 0120 0000'.replace(' ', '')), [b'A' * 6]),
    ('hand_v2_fill3', bytes.fromhex('534c5a02 05000000 14000000 00000000 01 5a 00f0 51'.replace(' ', '')), [b'Z' + b'Q' * 19]),
    ('hand_v2_fill2', bytes.fromhex('534c5a02 04000000 09000000 00000000 01 5a 51f5'.replace(' ', '')), [b'Z' + b'Q' * 8]),
]


def main():
    os.makedirs(V, exist_ok=True)
    man = []
    def emit(name, blob, outs, enc):
        open(f'{V}/{name}.slz', 'wb').write(blob)
        chunks = []
        for k, (o, ver) in enumerate(outs):
            fn = f'{name}.{k}.out'
            open(f'{V}/{fn}', 'wb').write(o)
            chunks.append(dict(ver=ver, expected=fn, size=len(o), sha256=hashlib.sha256(o).hexdigest()))
        man.append(dict(name=name, file=f'{name}.slz', encoder_reproduces=enc, chunks=chunks))
    for name, ch in CASES:
        blob = E.encode_entry(ch)
        if name.startswith('maxdist'):
            assert max_ref_dist(ch[0][0], ch[0][1]) == 4095, name
        if name.startswith('beyond'):
            assert max_ref_dist(ch[0][0], ch[0][1]) <= 4095
        emit(name, blob, ch, True)
    for name, blob, outs in HAND:
        emit(name, blob, [(o, blob[3]) for o in outs], False)
    json.dump(man, open(os.path.join(HERE, 'manifest.json'), 'w'), indent=1)
    print('wrote', len(man), 'vectors')


if __name__ == '__main__':
    main()
