"""Static census of BIOS/kernel/hardware use by the game code.  Prints addresses and numbers only (no game bytes).
    SO2_DATA=<checkout with extracted/ + analysis/out/d1> python -I analysis/bios_usage.py [--md]
Inputs (local, gitignored): extracted/SCUS_944.21, analysis/out/d1/*.dec; optional OpenBIOS image
tools/ref/psxrecomp/bios/openbios.bin (+ recompiler/seeds/openbios_elf_seeds.json for symbol names) under SO2_DATA.
Programs: exe @0x80010000, A/B/C @0x8002F810 (as analysis/indirect.py); 'other' = every other d1/*.dec, unaddressed union.
Method: (1) BIOS calls = `addiu/ori t2,zero,0xA0/B0/C0` followed (<=2 words) by jr/jalr t2, with `addiu/ori t1,zero,N`
within -3..+3 words (delay slot included); also `jal 0xA0/0xB0/0xC0`.  (2) hw registers: forward constant tracking of
lui/addiu/ori/addu/move inside straight-line runs, every lw/sw/lh/sh/lb/sb whose effective address is 0x1F80xxxx.
(3) GTE: cop2 command / mfc2,cfc2,mtc2,ctc2,lwc2,swc2 census.  Data inside images yields rare false positives.
(4) Psy-Q kernel patchers (GetB0Table/GetC0Table + writes at entry+offset), resolved against the retail dump
roms/scph5501.bin (layout only, never printed as bytes) and OpenBIOS; OpenBIOS coverage of every function number used
and the system-table words (0x150/0x154 device table) it writes.  --selftest checks the call/patch decoders."""
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


def words(b):
    return struct.unpack('<%dI' % (len(b) // 4), b[:len(b) // 4 * 4])


def table_patches(base, b, calls):
    """Psy-Q libapi kernel patchers: after B0(0x56) GetC0Table / B0(0x57) GetB0Table the code loads one table entry
    (lw v0,4*i(v0)) and writes at fixed offsets from that function's address.  -> [(site, table, entry, [(op, off)])]
    shortcut: offsets are taken until $v0 is first rewritten (an addi v0,v0,imm is recorded as the last 'ptr');
    a copy loop's own pointer bumps and later stores are then left out, so the word count is read by hand."""
    w, out = words(b), []
    for pc, t, fn, kind in calls:
        if t != 0xB0 or fn not in (0x56, 0x57): continue
        i = (pc - base) // 4 + 2
        while i < len(w) and not ((w[i] >> 16) == 0x8C42): i += 1           # lw v0,imm(v0)
        if i >= len(w) or i - (pc - base) // 4 > 6: continue
        ent, ops = (w[i] & 0xffff) // 4, []
        for x in w[i + 1:i + 40]:
            op, rs, rt, im = x >> 26, (x >> 21) & 31, (x >> 16) & 31, S(x & 0xffff)
            if op in (0x2b, 0x23) and rs == 2: ops.append(('sw' if op == 0x2b else 'lw', im))   # sw/lw rt,imm(v0)
            if op in (8, 9) and rs == 2 and rt in (2, 3) and im != 4: ops.append(('ptr', im))   # addi v0/v1,v0,imm (not a loop bump)
            if x == 0x03e00008 or (op in (8, 9, 0x0d, 0x0f, 0x23) and rt == 2) or (op == 0 and (x >> 11) & 31 == 2): break
        out.append((pc, 'C0' if fn == 0x56 else 'B0', ent, ops))
    return out


def retail_layout(path):
    """Retail SCPH-5xxx kernel facts read from the user's own dump (addresses only, no bytes printed).
    Kernel part 2 (ROM 0x10000..0x18000) is copied to RAM 0x500; B0/C0 tables sit at RAM 0x874/0x674."""
    r = open(path, 'rb').read()
    k = lambda a: struct.unpack_from('<I', r, 0x10000 + a - 0x500)[0]
    w = words(r)
    dcb = [hex(4 * i) for i, x in enumerate(w) if x >> 26 == 0x2b and (x & 0xffff) in (0x150, 0x154)
           and any(w[j] >> 26 == 0x0f and (w[j] & 0xffff) == 0xa000 for j in range(max(0, i - 6), i))]
    return {'B0': lambda i: k(0x874 + 4 * i), 'C0': lambda i: k(0x674 + 4 * i), 'dcb_stores': dcb, 'word': k}


def openbios_layout(path, seeds):
    """OpenBIOS: the A0/B0/C0 tables are one ROM array (A0 0xC0 + C0 0x20 + B0 0x60 entries), anchored on the
    psxtodigit symbol (A0 0x0A) and checked against getFreeEvCBSlot (C0 4) and openEvent (B0 8)."""
    r = open(path, 'rb').read()
    w = words(r)
    sym = {int(s['address'], 16): s['label'] for s in json.load(open(seeds))['seeds']}
    inv = {v: k for k, v in sym.items()}
    a0 = w.index(inv['psxtodigit']) - 0x0A
    c0, b0 = a0 + 0xC0, a0 + 0xE0
    assert sym.get(w[c0 + 4]) == 'getFreeEvCBSlot' and sym.get(w[b0 + 8]) == 'openEvent', 'OpenBIOS table anchor moved'
    ent = lambda t, i: w[{'A0': a0, 'B0': b0, 'C0': c0}[t] + i]
    # a RAM entry that is a 3-instruction trampoline into the ROM 'unimplemented' handler = not implemented
    unimpl = inv.get('unimplemented')
    def name(t, i):
        x = ent(t, i)
        if x in sym: return sym[x]
        o = 0x1E4D4 + x - 0x500                         # kernel .data LMA (bios/OpenBIOS.toml): RAM 0x500 <- ROM 0x1E4D4
        if 0 <= o < len(r) - 8 and unimpl is not None:
            hi, lo = struct.unpack_from('<2I', r, o)
            if hi >> 16 == 0x3C02 and lo >> 16 == 0x2442 and ((hi & 0xffff) << 16) + S(lo & 0xffff) == unimpl:
                return 'UNIMPLEMENTED'
        return 'ram_%04X' % x
    sysw = {}                                            # sw rt,0x40..0x54(v1) after addiu v1,v1,0x100 (setupFileIO)
    for i, x in enumerate(w):
        if x >> 26 == 9 and (x & 0xffff) == 0x100 and ((x >> 16) & 31) == ((x >> 21) & 31) == 3:
            regs = {}
            for y in w[i + 1:i + 16]:
                if y >> 26 == 0x0f: regs[(y >> 16) & 31] = (y & 0xffff) << 16
                elif y >> 26 == 9: regs[(y >> 16) & 31] = (regs.get((y >> 21) & 31, 0) + S(y & 0xffff)) & 0xffffffff
                elif y >> 26 == 0x2b and (y >> 21) & 31 == 3:
                    sysw[0x100 + (y & 0xffff)] = regs.get((y >> 16) & 31)
            break
    return {'name': name, 'ent': ent, 'sys': sysw}


def selftest():
    # addiu t1,zero,0x57 ; addiu t2,zero,0xB0 ; jalr t2 ; nop ; lw v0,0x16c(v0) ; addi v1,v0,0x62c ; sw zero,0(v1) ; jr ra
    b = struct.pack('<8I', 0x24090057, 0x240A00B0, 0x0140F809, 0, 0x8C42016C, 0x2043062C, 0xAC600000, 0x03E00008)
    c = scan_calls(0x80000000, b)
    assert c == [(0x80000004, 0xB0, 0x57, 'jalr')], c
    assert table_patches(0x80000000, b, c) == [(0x80000004, 'B0', 0x5B, [('ptr', 0x62C)])]
    print('selftest ok')
    return 0


def main():
    if '--selftest' in sys.argv: return selftest()
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
    print('\n## kernel patchers (GetC0Table/GetB0Table, then writes at entry+offset)')
    patches = []
    for k, (base, b) in imgs.items():
        for pc, t, e, ops in table_patches(base, b, allcalls[k]):
            patches.append((k, pc, t, e, ops))
            print('%s %08X %s[0x%02X] %s' % (k, pc, t, e, ' '.join('%s+0x%X' % o for o in ops) or '(pointer moved: by hand)'))
    used = sorted({(TABLES[t][0], fn) for c in allcalls.values() for _, t, fn, _ in c if fn is not None})
    retail = PD('roms', 'scph5501.bin')
    if os.path.exists(retail):
        R = retail_layout(retail)
        print('\n## retail SCPH-5501 layout (local dump; addresses only)')
        print('device table: sw 0x150/0x154 via lui 0xA000 at ROM', ' '.join(R['dcb_stores']),
              '(0x150 = table base, 0x154 = size in bytes = count*0x50)')
        for k, pc, t, e, ops in patches:
            f = R[t](e)
            print('%s %08X %s[0x%02X]=%04X -> %s' % (k, pc, t, e, f, ' '.join('%s %04X' % (o, f + d) for o, d in ops)))
    ob = PD('tools', 'ref', 'psxrecomp', 'bios', 'openbios.bin')
    if os.path.exists(ob):
        O = openbios_layout(ob, PD('tools', 'ref', 'psxrecomp', 'recompiler', 'seeds', 'openbios_elf_seeds.json'))
        print('\n## OpenBIOS coverage of every function number used (exe/A/B/C)')
        for t, fn in used:
            n = O['name'](t, fn)
            print('%s(0x%02X) %-28s OpenBIOS: %s%s' % (t, fn, TABLES[{'A0': 0xA0, 'B0': 0xB0, 'C0': 0xC0}[t]][1].get(fn, '?'), n,
                                                   '   <-- GAP' if n == 'UNIMPLEMENTED' else ''))
        print('system table words OpenBIOS writes at boot:', ' '.join('%X=%X' % kv for kv in sorted(O['sys'].items())),
              '(game reads 0x154 as size; OpenBIOS stores an END address there)' if O['sys'].get(0x154, 0) > O['sys'].get(0x150, 0) else '')
        for k, pc, t, e, ops in patches:
            f = O['ent'](t, e) & 0x1fffffff
            print('OpenBIOS %s %08X %s[0x%02X]=%04X -> %s' % (k, pc, t, e, f, ' '.join('%s %04X' % (o, f + d) for o, d in ops)))
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
