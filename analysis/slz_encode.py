"""Simple valid SLZ encoder (versions 0, 1, 2) used to build synthetic test vectors. Not size-optimal (greedy, brute force).
Spec: docs/SLZ.md.  Round-trip check: tests/slz/run_tests.py.  Contains no game data."""
import struct

MAX_DIST = 4095


def tokens(data, ver):
    """Greedy tokenisation -> list of ('lit',b) | ('ref',dist,len) | ('fill',byte,len). v1 refs: len 3..18; v2 refs: len 3..17 + fills."""
    maxlen = 18 if ver == 1 else 17
    t, i, n = [], 0, len(data)
    while i < n:
        if ver == 2:
            r = 1
            while i + r < n and data[i + r] == data[i] and r < 274:
                r += 1
            if r >= 4:
                t.append(('fill', data[i], r)); i += r; continue
        best_len, best_d = 0, 0
        for d in range(1, min(MAX_DIST, i) + 1):
            k = 0
            while k < maxlen and i + k < n and data[i + k - d] == data[i + k]:  # overlap allowed: reads freshly written bytes
                k += 1
            if k > best_len:
                best_len, best_d = k, d
        if best_len >= 3:
            t.append(('ref', best_d, best_len)); i += best_len
        else:
            t.append(('lit', data[i])); i += 1
    return t


def pack(items):
    """items: list of (is_literal, bytes). Emit flag bytes (LSB first, 1 = literal) interleaved with item bytes."""
    out, i = bytearray(), 0
    while i < len(items):
        grp = items[i:i + 8]; i += 8
        out.append(sum(1 << k for k, (lit, _) in enumerate(grp) if lit))
        for _, b in grp:
            out += b
    return bytes(out)


def encode_payload(data, ver):
    if ver == 0:
        return bytes(data)
    items = []
    for t in tokens(data, ver):
        if t[0] == 'lit':
            items.append((1, bytes([t[1]])))
        elif t[0] == 'ref':
            _, d, ln = t
            items.append((0, bytes([d & 255, (ln - 3) << 4 | d >> 8])))
        else:
            _, b, ln = t
            if ln <= 18:
                items.append((0, bytes([b, 0xF0 | (ln - 3)])))
            else:
                items.append((0, bytes([ln - 0x13, 0xF0, b])))
    if ver == 1:
        items.append((0, b'\x00\x00'))  # terminator: offset 0
    return pack(items)


def encode_chunk(data, ver, last=True):
    """One chunk: 16-byte header + payload (+ pad to 4 when another chunk follows)."""
    p = encode_payload(data, ver)
    nxt = 0 if last else (16 + len(p) + 3) & ~3
    c = b'SLZ' + bytes([ver]) + struct.pack('<III', len(p), len(data), nxt) + p
    return c + b'\0' * (nxt - len(c)) if nxt else c


def encode_entry(chunks):
    """chunks: list of (data, ver) -> multi-chunk SLZ entry."""
    return b''.join(encode_chunk(d, v, k == len(chunks) - 1) for k, (d, v) in enumerate(chunks))
