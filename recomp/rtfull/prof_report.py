"""Map prof.bin (stubs/psxrt_prof.c: uint32 RVAs of the emulation thread's RIP) to functions with the MSVC /MAP file and
print the top functions plus a share per area (object file).
    python -I recomp/rtfull/prof_report.py <prof.bin> <psxrecomp-game.map> [top]"""
import bisect, collections, re, struct, sys


def load_map(path):
    syms, base = [], 0x140000000
    for line in open(path, errors='replace'):
        m = re.search(r'Preferred load address is ([0-9a-fA-F]+)', line)
        if m:
            base = int(m.group(1), 16)
        # " 0001:00000000  name  00000001400010a0 f   obj.obj"
        m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s+(?:f\s+)?(?:i\s+)?(\S+)', line)
        if m and int(m.group(2), 16) >= base:
            syms.append((int(m.group(2), 16) - base, m.group(1), m.group(3)))
    syms.sort()
    return syms


def main():
    data = open(sys.argv[1], 'rb').read()
    rvas = struct.unpack(f'<{len(data) // 4}I', data[:len(data) // 4 * 4])
    syms = load_map(sys.argv[2])
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 25
    keys = [s[0] for s in syms]
    fn, obj = collections.Counter(), collections.Counter()
    for r in rvas:
        if r >= 0xFFFFFFF0:
            fn['<outside exe: DLL/kernel>'] += 1; obj['<outside exe>'] += 1; continue
        i = bisect.bisect_right(keys, r) - 1
        name, o = (syms[i][1], syms[i][2]) if i >= 0 else ('?', '?')
        fn[name] += 1
        obj[o] += 1
    n = len(rvas)
    print(f'{n} samples')
    for k, v in obj.most_common(15):
        print(f'obj {100 * v / n:5.1f}%  {k}')
    for k, v in fn.most_common(top):
        print(f'fn  {100 * v / n:5.1f}%  {k}')


if __name__ == '__main__':
    main()
