"""How SO2 addresses its two discs (static, local dumps only). Usage: python -I analysis/disc_map.py [--str]
Data root: $SO2_DATA (main checkout with roms/ and extracted/), default the repo root. Prints:
 1. the boot exe's loader facts, each checked against the exe bytes (assert on mismatch);
 2. per disc: TOC summary, the disc-marker slots 6/7, entries vs. the disc's sector count;
 3. disc 1 <-> disc 2 entry mapping (same index / same bytes);
 4. every call of the exe's disc check 0x80011d38 and its lid-cycle prompt in programs A (idx 1) and C (idx 2576);
 5. with --str: the STR (MDEC movie) entries, frames, size, sectors per frame (reads every raw entry's sectors).
RECOMP_NOTES '2026-10-27 disc 2' explains the results."""
import hashlib, os, struct, sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iso, slz, toc
from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_LITTLE_ENDIAN

ROOT = os.environ.get('SO2_DATA') or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DISC = [os.path.join(ROOT, 'roms', 'disc%d' % n, 'Star Ocean - The Second Story (USA) (Disc %d).bin' % n) for n in (1, 2)]
MD = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 + CS_MODE_LITTLE_ENDIAN)
OVL = 0x8002F810                                   # programs A/B/C load address (ANALYSIS.md 3)

def text(img, base, a, n=1):
    o = a - base
    return ['%s %s' % (i.mnemonic, i.op_str) for i in MD.disasm(img[o:o + 4 * n], a)]

def check(img, base, a, want):
    got = text(img, base, a)[0]
    assert got == want, '%08x: %r != %r' % (a, got, want)
    return '%08x  %s' % (a, got)

# (address, instruction, meaning) in the boot exe SCUS_944.21 (both discs carry the same bytes)
EXE_FACTS = [
    (0x80011C44, 'addiu $v0, $zero, 0x600', 'TOC load: slot 0 := 0x600 = CdlLOC 00:06:00 = LBA 300, before the TOC is read'),
    (0x80011C60, 'jal 0x80011654', '  seek(ctx, 0) -> the TOC sectors'),
    (0x80011CC4, 'jal 0x80011aac', '  then decrypt 0x1200 slots in place (docs/SO2BIN_TOC.md)'),
    (0x800116B8, 'lwl $v0, 7($s0)', 'seek(ctx, i): copies slot word ctx+4+4i ...'),
    (0x800116C4, 'swl $v0, 3($s1)', '  ... as the CdlLOC {min,sec,frame} BCD -> CdControl(CdlSetloc): absolute MSF on the inserted disc'),
    (0x80011B9C, 'sll $a1, $a1, 2', 'size(ctx, i) = (slot.byte3 << 8 | sz[i]) << 11'),
    (0x80011D3C, 'lui $v1, 0x3482', 'disc_is(ctx, n): if TOC magic 0x34829314 ...'),
    (0x80011D58, 'addiu $a1, $a1, 5', '  ... return size(slot n+5) != 0 (else 2): disc 1 = slot 6, disc 2 = slot 7'),
    (0x80011DB8, 'addiu $a0, $zero, 0xe', 'cd_stop(ctx): CdlSetmode 0 ...'),
    (0x80011DE4, 'addiu $a0, $zero, 8', '  ... CdlStop (retried while stat & 0x11)'),
    (0x80011E4C, 'andi $v0, $v0, 0x10', 'lid_wait(): CdlGetstat until ShellOpen (0x10) is set, then until it is clear'),
    (0x80011EDC, 'slti $v0, $s0, 0x709', 'disc_ready(ctx): up to 1800 VSyncs: Getstat until motor on (0x02) ...'),
    (0x80011F0C, 'addiu $a0, $zero, 0x13', '  ... CdlGetTN ok (no 0x11 bits) ...'),
    (0x80011F40, 'addiu $a0, $zero, 0x1b', '  ... CdlReadS at 00:06:00 (LBA 300), 30 tries ...'),
    (0x8001201C, 'jal 0x80011c20', '  ... then re-reads and decrypts the TOC of the NEW disc; returns 1 ok, 2 retry, 0 error'),
]
# the prompt routine: (program, entry index, address of the 0x80011d78 / lid_wait / disc_ready / disc_is calls)
PROMPT = {'A': (1, [0x8003C738, 0x8003C7B8, 0x8003C7E4, 0x8003C7FC]),
          'C': (2576, [0x8006FAA4, 0x8006FB24, 0x8006FB50, 0x8006FB68])}

def prog(d, w, sz, idx):
    lba, n = toc.ent(w, sz, idx)
    return slz.dec(d.read(lba, n * 2048))[0]

def jal_sites(img, base, target):
    want = 3 << 26 | (target & 0x0FFFFFFF) >> 2
    return [base + 4 * i for i, x in enumerate(struct.unpack('<%dI' % (len(img) // 4), img[:len(img) // 4 * 4])) if x == want]

def main():
    sys.stdout.reconfigure(encoding="utf-8")
    discs = [iso.Disc(p) for p in DISC]
    exe = open(os.path.join(ROOT, 'extracted', 'SCUS_944.21'), 'rb').read()[0x800:]
    print('== 1. boot exe loader (checked against SCUS_944.21)')
    for a, ins, why in EXE_FACTS: print(check(exe, 0x80010000, a, ins), ' ;', why)

    print('== 2. per-disc TOC (read from LBA 300 of each disc)')
    tocs, md5 = [], []
    for n, d in enumerate(discs, 1):
        vol, files = d.walk()
        w, sz = toc.load(d)
        assert w[0] == 0x34829314
        ents = {i: toc.ent(w, sz, i) for i in range(1, 0x1200) if toc.ent(w, sz, i)[1]}
        end = max(l + s for l, s in ents.values())
        print('disc %d: %s, %d sectors, ISO files %s' % (n, vol, d.n, ' '.join('%s@%d' % (f, l) for f, l, _, _ in files)))
        print('  %d entries, max index %d, LBA %d..%d (last+size %d <= %d sectors: %s)'
              % (len(ents), max(ents), min(l for l, _ in ents.values()), end - 1, end, d.n, end <= d.n))
        for i in (6, 7):
            l, s = toc.ent(w, sz, i)
            print('  slot %d: %s' % (i, 'empty' if not s else 'LBA %d, %d sector: %r'
                                     % (l, s, d.read(l, 64).split(b'\r')[0].decode('shift_jis', 'replace'))))
        tocs.append((w, sz, ents))
        md5.append({i: hashlib.md5(d.read(l, s * 2048)).hexdigest() for i, (l, s) in ents.items()})

    print('== 3. disc 1 <-> disc 2')
    e1, e2 = tocs[0][2], tocs[1][2]
    same = [i for i in e2 if i in e1 and md5[0][i] == md5[1][i]]
    print('  %d indices on both discs with identical bytes (%d at the same LBA)'
          % (len(same), sum(e1[i][0] == e2[i][0] for i in same)))
    print('  disc-2 indices whose bytes differ or are new: %s' % ' '.join(str(i) for i in sorted(set(e2) - set(same))))
    print('  disc-1 indices absent on disc 2: %s' % ' '.join(str(i) for i in sorted(set(e1) - set(e2))))

    print('== 4. disc checks / prompt in the programs (disc 1 bytes)')
    w, sz, _ = tocs[0]
    for name, (idx, sites) in PROMPT.items():
        img = prog(discs[0], w, sz, idx)
        for a, t in zip(sites, ('jal 0x80011d78', 'jal 0x80011e24', 'jal 0x80011eb4', 'jal 0x80011d38')):
            check(img, OVL, a, t)
        calls = jal_sites(img, OVL, 0x80011D38)
        print('  program %s (idx %d): prompt loop %08x..%08x checked; disc_is() called at %s'
              % (name, idx, sites[0], sites[-1], ' '.join('%08x(%s)' % (c, text(img, OVL, c + 4)[0]) for c in calls)))

    if '--str' in sys.argv:
        print('== 5. STR entries (header 0x0160/0x8001 in user data; idx lba sectors frames WxH sectors/frame)')
        for n, (d, (w, sz, ents)) in enumerate(zip(discs, tocs), 1):
            for i, (l, s) in sorted(ents.items()):
                if d.read(l, 4) != b'\x60\x01\x01\x80': continue
                frames, dims, spf = set(), set(), set()
                for k in range(s):
                    h = d.read(l + k, 20)
                    if h[:4] != b'\x60\x01\x01\x80': continue
                    _, _, _, cnt, fr, _, wd, ht = struct.unpack('<4H2I2H', h)
                    frames.add(fr); dims.add((wd, ht)); spf.add(cnt)
                print('  disc %d idx %d lba %d %d sectors: %d frames, %s, %s sectors/frame'
                      % (n, i, l, s, len(frames), ','.join('%dx%d' % x for x in sorted(dims)),
                         '-'.join(str(x) for x in (min(spf), max(spf)))))

if __name__ == '__main__':
    main()
