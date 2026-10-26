"""Print disc/exe facts (hashes, sectors, PS-X EXE header, SYSTEM.CNF). Usage: python -I disc_facts.py <disc.bin>... (cue beside it) ; local data only."""
import sys, os, hashlib, struct
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iso
def h(p, algo):
    m = hashlib.new(algo)
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 22), b''): m.update(c)
    return m.hexdigest()
for p in sys.argv[1:]:
    cue = p[:-4] + '.cue'
    print('==', os.path.basename(p), os.path.getsize(p), 'B')
    for a in ('sha256', 'sha1', 'md5'): print(a, h(p, a))
    print('cue sha256', h(cue, 'sha256'), os.path.getsize(cue), 'B')
    d = iso.Disc(p); vol, fs = d.walk()
    print('sectors', d.n, 'size/2352', os.path.getsize(p) / 2352, 'vol', vol)
    for nm, lba, sz, fl in sorted(fs):
        if nm.upper().startswith(('/SYSTEM.CNF', '/SCUS', '/SO2')):
            data = d.read(lba, sz)
            print(nm, 'lba', lba, 'size', sz, 'sha256', hashlib.sha256(data).hexdigest())
            if 'CNF' in nm.upper(): print(repr(data.decode('latin1')))
            if nm.upper().startswith('/SCUS'):
                assert data[:8] == b'PS-X EXE'
                pc, gp, ld, ts, _, _, _, _, sp, spo = struct.unpack('<10I', data[0x10:0x38])
                print('pc %08x gp %08x load %08x text_size %08x sp %08x sp_off %08x' % (pc, gp, ld, ts, sp, spo), 'len', len(data))
