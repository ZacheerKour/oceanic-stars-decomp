#!/usr/bin/env python3
"""Pull a few dozen pictures off the player's OWN Star Ocean 2 disc images for the wide-screen side panels.

  python -I scripts\\extract_side_art.py [--roms roms] [--out play\\sidefill] [--count 40]

Reads the .bin images under --roms (read-only), decodes the 24-bit title picture and the pre-rendered field backgrounds
(256x256 RGB555 pages inside SO2.BIN's SLZ entries, holes filled by push-pull), drops near-blank / near-duplicate ones, writes PNGs.
Output is the player's local data only: play\\ is gitignored and nothing here is ever committed or shipped.
Stdlib only (PNG written with zlib). Exit 0 if at least one picture was written.
"""
import argparse, glob, math, mmap, os, re, struct, sys, zlib

SEC = 2352
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'analysis'))
import slz, toc  # noqa: E402  (repo decoders: SLZ chunks, SO2.BIN table of contents)


class Disc:
    def __init__(s, path):
        s.f = open(path, 'rb'); s.m = mmap.mmap(s.f.fileno(), 0, access=mmap.ACCESS_READ); s.n = len(s.m) // SEC
    def sec(s, l): return s.m[l * SEC + 24:l * SEC + 24 + 2048]
    def read(s, l, size): return b''.join(s.sec(l + i) for i in range((size + 2047) // 2048))[:size]


# ---- PNG (stdlib) -----------------------------------------------------------------------------------------------
def write_png(path, w, h, rgb):
    raw = b''.join(b'\0' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def ch(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))


def px15(b, w, h):
    o = bytearray(w * h * 3)
    for i in range(w * h):
        v = b[2 * i] | b[2 * i + 1] << 8
        o[3 * i:3 * i + 3] = bytes(((v & 31) * 255 // 31, (v >> 5 & 31) * 255 // 31, (v >> 10 & 31) * 255 // 31))
    return bytes(o)


# ---- picture sources ---------------------------------------------------------------------------------------------
def tim24(b):
    for m in re.finditer(rb'\x10\x00\x00\x00\x03\x00\x00\x00', b):
        o = m.start() + 8
        if o + 12 > len(b): continue
        bs, x, y, iw, ih = struct.unpack_from('<IHHHH', b, o)
        w = iw * 2 // 3
        if bs == 12 + iw * ih * 2 and w >= 320 and ih >= 240 and o + bs <= len(b): yield w, ih, b[o + 12:o + 12 + w * 3 * ih]


def field_pages(d, w, sz, entries):
    """Pre-rendered field backgrounds: SO2.BIN entries 3207.. hold SLZ streams whose 131072-byte chunks are 256x256 RGB555 pages."""
    for i in entries:
        lba, s = toc.ent(w, sz, i)
        if not s: continue
        e = d.read(lba, s * 2048)
        for m in re.finditer(rb'SLZ[\x00\x01\x02]', e[:0x30000]):
            try: chunks = slz.dec_all(e[m.start():])
            except Exception: continue
            for c in chunks:
                if len(c) == 131072: yield i, c
            break


# ---- selection ---------------------------------------------------------------------------------------------------
def luma(rgb): return [(rgb[i] * 3 + rgb[i + 1] * 6 + rgb[i + 2]) // 10 for i in range(0, len(rgb), 3)]


def stats(rgb):
    l = luma(rgb)[::7]; m = sum(l) / len(l)
    return m, math.sqrt(sum((v - m) ** 2 for v in l) / len(l))


def ahash(rgb, w, h):
    l = luma(rgb); g = [l[(y * h // 8 + h // 16) * w + x * w // 8 + w // 16] for y in range(8) for x in range(8)]
    m = sum(g) / 64; return sum(1 << i for i, v in enumerate(g) if v > m)


def fill_holes(rgb, w, h):
    """Pages are atlases with unused (pure black) cells; fill them from the surrounding picture (push-pull, w = h = 2^n).
    -> (rgb, fraction of pixels that were holes)."""
    n = w * h; mask = [1 if rgb[3 * i] + rgb[3 * i + 1] + rgb[3 * i + 2] > 6 else 0 for i in range(n)]
    holes = 1 - sum(mask) / n
    lv = [([rgb[i] * mask[i // 3] for i in range(3 * n)], mask[:], w)]
    while lv[-1][2] > 1:
        v, m, sw = lv[-1]; hw = sw // 2; nv = [0] * (3 * hw * hw); nm = [0] * (hw * hw)
        for y in range(hw):
            for x in range(hw):
                for dy in (0, 1):
                    for dx in (0, 1):
                        j = (2 * y + dy) * sw + 2 * x + dx; nm[y * hw + x] += m[j]
                        for c in range(3): nv[3 * (y * hw + x) + c] += v[3 * j + c]
        lv.append((nv, nm, hw))
    cur = None   # pull: coarse -> fine, holes take the (normalised) coarser value
    for v, m, sw in reversed(lv):
        out = [0.0] * (3 * sw * sw); omask = [0] * (sw * sw)
        for i in range(sw * sw):
            if m[i]:
                for c in range(3): out[3 * i + c] = v[3 * i + c] / m[i]
                omask[i] = 1
            elif cur is not None:
                j = (i // sw // 2) * (sw // 2) + (i % sw) // 2
                for c in range(3): out[3 * i + c] = cur[3 * j + c]
        cur = out
    return bytes(max(0, min(255, int(x))) for x in cur), holes


def main():
    ap = argparse.ArgumentParser(description=__doc__.split(chr(10))[0])
    root = os.path.dirname(HERE)
    ap.add_argument('--roms', default=os.path.join(root, 'roms'))
    ap.add_argument('--out', default=os.path.join(root, 'play', 'sidefill'))
    ap.add_argument('--count', type=int, default=40, help='number of pictures to write')
    ap.add_argument('--max-holes', type=float, default=0.30, help='skip field pages with more unused (black) area than this')
    a = ap.parse_args()
    bins = sorted(p for p in glob.glob(os.path.join(a.roms, '**', '*.bin'), recursive=True) if not os.path.basename(p).lower().startswith('scph'))
    if not bins: print('no disc images (.bin) under', a.roms); return 2
    os.makedirs(a.out, exist_ok=True)
    for old in glob.glob(os.path.join(a.out, 'side_*.png')): os.remove(old)
    discs = [Disc(p) for p in bins]; hashes = []; cand = []   # cand: (tag, w, h, rgb)

    def keep(rgb, w, h):
        m, sd = stats(rgb)
        if sd < 22 or m < 20 or m > 235: return False
        hs = ahash(rgb, w, h)
        if any(bin(hs ^ x).count('1') <= 8 for x in hashes): return False
        hashes.append(hs); return True

    d1 = discs[0]; w, sz = toc.load(d1)
    for i in (18,):                      # the 24-bit title picture (Earth + ship); 19/20 are copies
        lba, n = toc.ent(w, sz, i)
        for tw, th, px in tim24(slz.dec_all(d1.read(lba, n * 2048))[0]):
            if keep(px, tw, th): cand.append(('title', tw, th, px))
    # field backgrounds: spread over the ~900 scene entries, so take every `step`-th one that passes the filters
    scenes = list(range(3207, 4116)); step = max(1, len(scenes) // (a.count * 2))
    for i, c in field_pages(d1, w, sz, scenes[::step]):
        if len(cand) >= a.count: break
        rgb, holes = fill_holes(px15(c, 256, 256), 256, 256)
        if holes <= a.max_holes and keep(rgb, 256, 256): cand.append(('field%d' % i, 256, 256, rgb))
    for k, (tag, tw, th, px) in enumerate(cand[:a.count]):
        p = os.path.join(a.out, 'side_%02d_%s.png' % (k, tag)); write_png(p, tw, th, px); print('  %s %dx%d' % (os.path.basename(p), tw, th))
    print('wrote', min(len(cand), a.count), 'pictures to', a.out)
    return 0 if cand else 1


if __name__ == '__main__':
    sys.exit(main())
