"""Static census of BIOS/kernel/hardware use by the game code.  Prints addresses and numbers only (no game bytes).
    SO2_DATA=<checkout with extracted/ + analysis/out/d1> python -I analysis/bios_usage.py [--md]
Inputs (local, gitignored): extracted/SCUS_944.21, analysis/out/d1/*.dec; optional OpenBIOS image
tools/ref/psxrecomp/bios/openbios.bin (+ recompiler/seeds/openbios_elf_seeds.json for symbol names) under SO2_DATA.
Programs: exe @0x80010000, A/B/C @0x8002F810 (as analysis/indirect.py); 'other' = every other d1/*.dec, unaddressed union.
Method: (1) BIOS calls = `addiu/ori t2,zero,0xA0/B0/C0` followed (<=2 words) by jr/jalr t2, with `addiu/ori t1,zero,N`
within -3..+3 words (delay slot included); also `jal 0xA0/0xB0/0xC0`.  (2) hw registers: forward constant tracking of
lui/addiu/ori/addu/move inside straight-line runs, every lw/sw/lh/sh/lb/sb whose effective address is 0x1F80xxxx.
(3) GTE: cop2 command / mfc2,cfc2,mtc2,ctc2,lwc2,swc2 census.  Data inside images yields rare false positives."""
import os, sys, struct, glob, json, collections

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DATA = os.environ.get('SO2_DATA', ROOT)
PD = lambda *a: os.path.join(DATA, *a)
LOAD = 0x8002F810
PROGS = {'A': '0001_312.dec', 'B': '2129_24323.dec', 'C': '2576_30736.dec'}
EXCL = {'A': (0x800509DC, 0x80061C58)}  # MDEC tables + data, not code

A0 = {0: 'FileOpen', 1: 'FileSeek', 2: 'FileRead', 3: 'FileWrite', 4: 'FileClose', 5: 'FileIoctl', 6: 'exit',
      7: 'FileGetDeviceFlag', 8: 'FileGetc', 9: 'FilePutc', 10: 'todigit', 11: 'atof', 12: 'strtoul', 13: 'strtol',
      14: 'abs', 15: 'labs', 16: 'atoi', 17: 'atol', 18: 'atob', 19: 'setjmp', 20: 'longjmp', 21: 'strcat',
      22: 'strncat', 23: 'strcmp', 24: 'strncmp', 25: 'strcpy', 26: 'strncpy', 27: 'strlen', 28: 'index',
      29: 'rindex', 30: 'strchr', 31: 'strrchr', 32: 'strpbrk', 33: 'strspn', 34: 'strcspn', 35: 'strtok',
      36: 'strstr', 37: 'toupper', 38: 'tolower', 39: 'bcopy', 40: 'bzero', 41: 'bcmp', 42: 'memcpy',
      43: 'memset', 44: 'memmove', 45: 'memcmp', 46: 'memchr', 47: 'rand', 48: 'srand', 49: 'qsort',
      50: 'strtod', 51: 'malloc', 52: 'free', 53: 'lsearch', 54: 'bsearch', 55: 'calloc', 56: 'realloc',
      57: 'InitHeap', 58: '_exit', 59: 'getchar', 60: 'putchar', 61: 'gets', 62: 'puts', 63: 'printf',
      64: 'SystemErrorUnresolvedException', 65: 'LoadTest', 66: 'Load', 67: 'Exec', 68: 'FlushCache',
      69: 'init_a0_b0_c0_vectors', 70: 'GPU_dw', 71: 'gpu_send_dma', 72: 'SendGP1Command', 73: 'GPU_cw',
      74: 'GPU_cwp', 75: 'send_gpu_linked_list', 76: 'gpu_abort_dma', 77: 'GetGPUStatus', 78: 'gpu_sync',
      81: 'LoadExec', 82: 'GetSysSp', 84: '_96_init', 85: '_bu_init', 86: '_96_remove', 91: 'dev_tty_init',
      92: 'dev_tty_open', 93: 'dev_tty_in_out', 94: 'dev_tty_ioctl', 95: 'dev_cd_open', 96: 'dev_cd_read',
      97: 'dev_cd_close', 98: 'dev_cd_firstfile', 99: 'dev_cd_nextfile', 100: 'dev_cd_chdir', 101: 'dev_card_open',
      102: 'dev_card_read', 103: 'dev_card_write', 104: 'dev_card_close', 105: 'dev_card_firstfile',
      106: 'dev_card_nextfile', 107: 'dev_card_erase', 108: 'dev_card_undelete', 109: 'dev_card_format',
      110: 'dev_card_rename', 112: '_bu_init', 113: '_96_init', 114: '_96_remove', 120: 'CdAsyncSeekL',
      124: 'CdAsyncGetStatus', 126: 'CdAsyncReadSector', 129: 'CdAsyncSetMode', 144: 'CdromIoIrqFunc1',
      145: 'CdromDmaIrqFunc1', 146: 'CdromIoIrqFunc2', 147: 'CdromDmaIrqFunc2', 148: 'CdromGetInt5errCode',
      149: 'CdInitSubFunc', 150: 'AddCDROMDevice', 151: 'AddMemCardDevice', 152: 'AddDuartTtyDevice',
      153: 'AddDummyTtyDevice', 156: 'SetConf', 157: 'GetConf', 158: 'SetCdromIrqAutoAbort', 159: 'SetMemSize',
      160: 'WarmBoot', 161: 'SystemErrorBootOrDiskFailure', 162: 'EnqueueCdIntr', 163: 'DequeueCdIntr',
      164: 'CdGetLbn', 165: 'CdReadSector', 166: 'CdGetStatus', 167: 'bu_callback_okay',
      168: 'bu_callback_err_write', 169: 'bu_callback_err_busy', 170: 'bu_callback_err_eject', 171: '_card_info',
      172: '_card_async_load_directory', 173: 'set_card_auto_format', 174: 'bu_callback_err_prev_write',
      175: 'card_write_test', 178: 'ioabort_raw', 180: 'GetSystemInfo'}
B0 = {0: 'alloc_kernel_memory', 1: 'free_kernel_memory', 2: 'init_timer', 3: 'get_timer', 4: 'enable_timer_irq',
      5: 'disable_timer_irq', 6: 'restart_timer', 7: 'DeliverEvent', 8: 'OpenEvent', 9: 'CloseEvent',
      10: 'WaitEvent', 11: 'TestEvent', 12: 'EnableEvent', 13: 'DisableEvent', 14: 'OpenThread',
      15: 'CloseThread', 16: 'ChangeThread', 17: 'jump_to_00000000h', 18: 'InitPad', 19: 'StartPad',
      20: 'StopPad', 21: 'PAD_init', 22: 'PAD_dr', 23: 'ReturnFromException', 24: 'ResetEntryInt',
      25: 'HookEntryInt', 32: 'UnDeliverEvent', 50: 'FileOpen', 51: 'FileSeek', 52: 'FileRead', 53: 'FileWrite',
      54: 'FileClose', 55: 'FileIoctl', 56: 'exit', 57: 'FileGetDeviceFlag', 58: 'FileGetc', 59: 'FilePutc',
      60: 'getchar', 61: 'putchar', 62: 'gets', 63: 'puts', 64: 'cd', 65: 'format', 66: 'firstfile',
      67: 'nextfile', 68: 'rename', 69: 'delete', 70: 'undelete', 71: 'AddDevice', 72: 'RemoveDevice',
      73: 'PrintInstalledDevices', 74: 'InitCard', 75: 'StartCard', 76: 'StopCard', 77: '_card_info_subfunc',
      78: 'write_card_sector', 79: 'read_card_sector', 80: 'allow_new_card', 81: 'Krom2RawAdd',
      83: 'Krom2Offset', 84: 'GetLastError', 85: 'GetLastFileError', 86: 'GetC0Table', 87: 'GetB0Table',
      88: 'get_bu_callback_port', 89: 'testdevice', 91: 'ChangeClearPad', 92: 'get_card_status',
      93: 'wait_card_status'}
C0 = {0: 'EnqueueTimerAndVblankIrqs', 1: 'EnqueueSyscallHandler', 2: 'SysEnqIntRP', 3: 'SysDeqIntRP',
      4: 'get_free_EvCB_slot', 5: 'get_free_TCB_slot', 6: 'ExceptionHandler', 7: 'InstallExceptionHandlers',
      8: 'SysInitMemory', 9: 'SysInitKernelVariables', 10: 'ChangeClearRCnt', 11: 'SystemError',
      12: 'InitDefInt', 13: 'SetIrqAutoAck', 25: 'SetCustomExitFromException'}
TABLES = {0xA0: ('A0', A0), 0xB0: ('B0', B0), 0xC0: ('C0', C0)}
GTE = {0x01: 'RTPS', 0x06: 'NCLIP', 0x0C: 'OP', 0x10: 'DPCS', 0x11: 'INTPL', 0x12: 'MVMVA', 0x13: 'NCDS',
       0x14: 'CDP', 0x16: 'NCDT', 0x1B: 'NCCS', 0x1C: 'CC', 0x1E: 'NCS', 0x20: 'NCT', 0x28: 'SQR', 0x29: 'DCPL',
       0x2A: 'DPCT', 0x2D: 'AVSZ3', 0x2E: 'AVSZ4', 0x30: 'RTPT', 0x3D: 'GPF', 0x3E: 'GPL', 0x3F: 'NCCT'}
HW = [(0x1F801000, 0x1F801024, 'mem-ctrl'), (0x1F801040, 0x1F80105F, 'SIO0 pad/card'), (0x1F801060, 0x1F801063, 'RAM size'),
      (0x1F801070, 0x1F801077, 'IRQ stat/mask'), (0x1F801080, 0x1F8010FF, 'DMA'), (0x1F801100, 0x1F80112F, 'timers'),
      (0x1F801800, 0x1F801803, 'CD-ROM'), (0x1F801810, 0x1F801817, 'GPU'), (0x1F801820, 0x1F801827, 'MDEC'),
      (0x1F801C00, 0x1F801FFF, 'SPU'), (0x1F800000, 0x1F8003FF, 'scratchpad'), (0x1F802000, 0x1F80207F, 'expansion2')]
DMA_CH = ['MDECin', 'MDECout', 'GPU', 'CDROM', 'SPU', 'PIO', 'OTC']
S = lambda x: x - 0x10000 if x & 0x8000 else x


def hwname(a):
    for lo, hi, n in HW:
        if lo <= a <= hi: return n
    return 'other'


def load():
    exe = open(PD('extracted', 'SCUS_944.21'), 'rb').read()
    imgs = {'exe': (0x80010000, exe[0x800:])}
    for k, f in PROGS.items(): imgs[k] = (LOAD, open(PD('analysis', 'out', 'd1', f), 'rb').read())
    other = {}
    for p in sorted(glob.glob(PD('analysis', 'out', 'd1', '*.dec'))):
        if os.path.basename(p) not in PROGS.values(): other[os.path.basename(p)] = open(p, 'rb').read()
    return imgs, other


def scan_calls(base, b, excl=None):
    """-> list of (site_pc, table, fn or None, kind)"""
    n = len(b) // 4
    w = struct.unpack('<%dI' % n, b[:n * 4])
    out = []
    for i, x in enumerate(w):
        pc = base + 4 * i
        if excl and excl[0] <= pc < excl[1]: continue
        t = None
        if x in (0x0c000028, 0x0c00002c, 0x0c000030):
            t = {0x0c000028: 0xA0, 0x0c00002c: 0xB0, 0x0c000030: 0xC0}[x]; kind = 'jal'; lo, hi = i - 3, i + 1
        elif (x >> 16) in (0x240A, 0x340A) and (x & 0xffff) in (0xA0, 0xB0, 0xC0) and ((x >> 21) & 31) == 0:
            if i + 1 < n and w[i + 1] in (0x01400008, 0x0140f809):
                t = x & 0xffff; kind = 'jr' if w[i + 1] == 0x01400008 else 'jalr'; lo, hi = i - 3, i + 3
            else:
                continue
        else:
            continue
        fn = None
        for j in range(max(0, lo), min(n, hi + 1)):
            y = w[j]
            if (y >> 16) in (0x2409, 0x3409): fn = y & 0xffff
        out.append((pc, t, fn, kind))
    return out


def scan_hw(base, b, excl=None):
    n = len(b) // 4
    w = struct.unpack('<%dI' % n, b[:n * 4])
    r = {}
    acc = collections.Counter()  # (addr, 'r'/'w', size) -> count
    sites = collections.defaultdict(list)
    for i, x in enumerate(w):
        pc = base + 4 * i
        op, rs, rt, rd, im = x >> 26, (x >> 21) & 31, (x >> 16) & 31, (x >> 11) & 31, x & 0xffff
        if op == 0x0f: r[rt] = im << 16
        elif op in (8, 9) and rs in r: r[rt] = (r[rs] + S(im)) & 0xffffffff
        elif op == 0x0d and rs in r: r[rt] = r[rs] | im
        elif op == 0 and (x & 63) in (0x21, 0x25) and rt == 0 and rs in r and rd: r[rd] = r[rs]
        elif op in (0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2b) and rs in r:
            a = (r[rs] + S(im)) & 0xffffffff
            if 0x1F800000 <= a < 0x1F810000 and not (excl and excl[0] <= pc < excl[1]):
                acc[(a, 'w' if op >= 0x28 else 'r', {0x20: 1, 0x24: 1, 0x28: 1, 0x21: 2, 0x25: 2, 0x29: 2}.get(op, 4))] += 1
                sites[a].append(pc)
            if op in (0x20, 0x21, 0x23, 0x24, 0x25): r.pop(rt, None)
        else:
            # any other write to a register invalidates it (best effort)
            d = None
            if op == 0 and (x & 63) not in (8, 0x18, 0x19, 0x1a, 0x1b, 0xc, 0xd): d = rd
            elif op in (0x0a, 0x0b, 0x0c, 0x0e, 0x32, 0x30, 0x22, 0x26, 0x12) and op != 0x12: d = rt
            elif op == 0x12 and rs in (0, 2): d = rt
            elif op == 0x10 and rs == 0: d = rt
            if d is not None: r.pop(d, None)
        # j/jr end a straight run; calls clobber caller-saved registers (delay slot is not tracked)
        if op == 2 or (op == 0 and (x & 63) == 8): r.clear()
        elif op == 3 or (op == 0 and (x & 63) == 9):
            for k in (2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31): r.pop(k, None)
    return acc, sites


def scan_gte(base, b, excl=None):
    n = len(b) // 4
    w = struct.unpack('<%dI' % n, b[:n * 4])
    cmd, mv, mem = collections.Counter(), collections.Counter(), collections.Counter()
    for i, x in enumerate(w):
        pc = base + 4 * i
        if excl and excl[0] <= pc < excl[1]: continue
        op = x >> 26
        if op == 0x12:
            if x & (1 << 25): cmd[x & 0x3f] += 1
            else: mv[{0: 'mfc2', 2: 'cfc2', 4: 'mtc2', 6: 'ctc2'}.get((x >> 21) & 31, 'other')] += 1
        elif op == 0x32: mem['lwc2'] += 1
        elif op == 0x3a: mem['swc2'] += 1
    return cmd, mv, mem


def scan_misc(base, b, excl=None):
    n = len(b) // 4
    w = struct.unpack('<%dI' % n, b[:n * 4])
    c = collections.Counter()
    crit = collections.Counter()
    for i, x in enumerate(w):
        pc = base + 4 * i
        if excl and excl[0] <= pc < excl[1]: continue
        if x >> 26 == 0 and (x & 63) == 0x0c:
            c['syscall'] += 1
            for j in range(max(0, i - 3), i + 2):
                if (w[j] >> 16) == 0x2404 and (w[j] & 0xffff) in (1, 2, 3, 4): crit[w[j] & 0xffff] += 1
        elif x >> 26 == 0 and (x & 63) == 0x0d: c['break'] += 1
        elif x >> 26 == 0x10 and ((x >> 21) & 31) == 4: c['mtc0 r%d' % ((x >> 11) & 31)] += 1
        elif x >> 26 == 0x10 and ((x >> 21) & 31) == 0: c['mfc0 r%d' % ((x >> 11) & 31)] += 1
        elif x == 0x42000010: c['rfe'] += 1
        elif x >> 26 == 0x10 and (x & 0x02000000) and (x & 63) == 0x10: c['rfe'] += 1
    return c, crit


def device_sites(base, b):
    """sites that read the BIOS device table pointers 0x150/0x154 (lw rt,0x150(zero)) and jalr after a DCB+0x34 walk"""
    n = len(b) // 4
    w = struct.unpack('<%dI' % n, b[:n * 4])
    return [base + 4 * i for i, x in enumerate(w) if (x >> 26) == 0x23 and ((x >> 21) & 31) == 0 and (x & 0xffff) in (0x150, 0x154)], \
           [base + 4 * i for i, x in enumerate(w) if (x >> 26) in (0x23, 0x2b) and (x & 0xffff) == 0x34 and ((x >> 21) & 31) not in (29, 0)]


def lowram_refs(base, b):
    """lw/sw/lh/sh/lb/sb with base $zero and offset < 0x10000: direct kernel-variable access (0x100.. tables, 0x80 vectors)"""
    n = len(b) // 4
    w = struct.unpack('<%dI' % n, b[:n * 4])
    c = collections.defaultdict(list)
    for i, x in enumerate(w):
        if (x >> 26) in (0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2b) and ((x >> 21) & 31) == 0 and 0 < (x & 0xffff) < 0x8000:
            c[x & 0xffff].append(base + 4 * i)
    return c


def open_tables():
    """OpenBIOS A0/B0/C0 tables (RAM 0x464c: A0 192 entries, C0 32, B0 97) -> {(table,fn): ('impl', sym)|'dummy'|'unimpl'}"""
    f = PD('tools', 'ref', 'psxrecomp', 'bios', 'openbios.bin'); sf = PD('tools', 'ref', 'psxrecomp', 'recompiler', 'seeds', 'openbios_elf_seeds.json')
    if not (os.path.exists(f) and os.path.exists(sf)): return None
    d = open(f, 'rb').read()
    lab = {int(x['address'], 16): x['label'] for x in json.load(open(sf))['seeds']}
    w = [struct.unpack_from('<I', d, 0x1E4D4 + 0x464c - 0x500 + 4 * i)[0] for i in range(321)]
    out = {}
    for t, b0, n in ((0xA0, 0, 192), (0xC0, 192, 32), (0xB0, 224, 97)):
        for fn in range(n):
            v = w[b0 + fn]
            out[(t, fn)] = 'unimpl' if v == 0x2afc else 'dummy' if lab.get(v) == 'psxdummy' else lab.get(v, 'kernelRAM@%X' % v)
    return out


def main():
    md = '--md' in sys.argv
    imgs, other = load()
    print('# programs: ' + ', '.join('%s %d B' % (k, len(v[1])) for k, v in imgs.items()) + '; other overlays %d' % len(other))
    allcalls = {}
    for k, (base, b) in imgs.items():
        calls = scan_calls(base, b, EXCL.get(k))
        allcalls[k] = calls
        print('\n## BIOS calls: %s' % k)
        by = collections.defaultdict(list)
        for pc, t, fn, kind in calls: by[(t, fn)].append(pc)
        for (t, fn), pcs in sorted(by.items(), key=lambda kv: (kv[0][0], -1 if kv[0][1] is None else kv[0][1])):
            tn, tab = TABLES[t]
            print('%s(%s) %-30s sites=%d  %s' % (tn, '??' if fn is None else '0x%02X' % fn, tab.get(fn, '?'), len(pcs),
                                                ' '.join('%08X' % p for p in pcs[:6])))
    print('\n## BIOS calls: other overlays (union of table,fn; module count)')
    u = collections.defaultdict(set)
    for name, b in other.items():
        for pc, t, fn, kind in scan_calls(0x8002F810, b): u[(t, fn)].add(name)
    for (t, fn), s in sorted(u.items(), key=lambda kv: (kv[0][0], -1 if kv[0][1] is None else kv[0][1])):
        tn, tab = TABLES[t]
        print('%s(%s) %-30s modules=%d' % (tn, '??' if fn is None else '0x%02X' % fn, tab.get(fn, '?'), len(s)))
    ot = open_tables()
    if ot:
        print('\n## OpenBIOS status of every (table,fn) the game calls (unimpl = common error stub 0x2AFC, dummy = returns 0)')
        used = collections.defaultdict(set)
        for k, calls in allcalls.items():
            for pc, t, fn, kind in calls: used[(t, fn)].add(k)
        for (t, fn), who in sorted(used.items(), key=lambda kv: (kv[0][0], kv[0][1] if kv[0][1] is not None else -1)):
            tn, tab = TABLES[t]
            st = ot.get((t, fn), 'n/a') if fn is not None else 'n/a'
            print('%s(%s) %-28s %-26s used by %s' % (tn, '??' if fn is None else '0x%02X' % fn, tab.get(fn, '?'), st, ','.join(sorted(who))))
    print('\n## direct low-RAM kernel variable accesses (base $zero, offset<0x8000), offset: site count')
    for k, (base, b) in imgs.items():
        c = lowram_refs(base, b)
        print(k, ' '.join('%X:%d' % (o, len(v)) for o, v in sorted(c.items())))
    print('\n## device-table walks (lw 0x150/0x154($zero)) and DCB+0x34 accesses')
    for k, (base, b) in imgs.items():
        a, c = device_sites(base, b)
        print(k, 'dev-table loads:', ' '.join('%08X' % x for x in a), '| +0x34 mem ops:', len(c))
    for k, (base, b) in imgs.items():
        acc, sites = scan_hw(base, b, EXCL.get(k))
        print('\n## hardware regs (0x1F80xxxx): %s' % k)
        agg = collections.defaultdict(lambda: [0, 0])
        for (a, rw, sz), cnt in acc.items(): agg[a][0 if rw == 'r' else 1] += cnt
        sp = [a for a in agg if hwname(a) == 'scratchpad']
        if sp: print('scratchpad 1F8000xx: %d distinct addrs, %d accesses (not a device)' % (len(sp), sum(sum(agg[a]) for a in sp)))
        for a in sorted(agg):
            if hwname(a) == 'scratchpad': continue
            print('%08X %-14s r=%d w=%d  e.g. %s' % (a, hwname(a), agg[a][0], agg[a][1], ' '.join('%08X' % p for p in sites[a][:3])))
    print('\n## data words that are 0x1F801000-0x1F8020xx constants (register base tables), count per image')
    for k, (base, b) in imgs.items():
        n = len(b) // 4
        w = struct.unpack('<%dI' % n, b[:n * 4])
        hits = collections.Counter(x for i, x in enumerate(w) if 0x1F801000 <= x < 0x1F802100
                                   and not (EXCL.get(k) and EXCL[k][0] <= base + 4 * i < EXCL[k][1]))
        print(k, ' '.join('%08X(%s)x%d' % (a, hwname(a), c) for a, c in sorted(hits.items())))
    print('\n## GTE / syscall / cop0')
    tot = collections.Counter()
    for k, (base, b) in imgs.items():
        cmd, mv, mem = scan_gte(base, b, EXCL.get(k))
        c, crit = scan_misc(base, b, EXCL.get(k))
        print(k, 'cop2 cmds:', ', '.join('%s=%d' % (GTE.get(f, '0x%02X' % f), n) for f, n in sorted(cmd.items())),
              '| moves:', dict(mv), dict(mem), '| misc:', dict(c), '| syscall a0:', dict(crit))
    return 0


if __name__ == '__main__':
    sys.exit(main())
