# BIOS / kernel / hardware usage of the game (static, 2026-10-26)

Reproduce: `SO2_DATA=<main checkout> python -I analysis/bios_usage.py` (addresses and numbers only).
Sources: boot exe (0x80010000) and programs A/B/C (0x8002F810) decoded from local data, OpenBIOS `bios/openbios.bin` plus its ELF symbol seeds
(the OpenBIOS source itself is **not** in the clone), the user's retail SCPH-5501 (read locally, nothing from it is committed here).
Names for function numbers are the standard PS1 BIOS table. Nothing was executed. Counts include a few data false positives (images mix data).

## 1. BIOS function numbers called (pattern `addiu t2,0xA0/B0/C0; jr/jalr t2; addiu t1,N`)
The exe holds the wrappers (A0 at 0x80023B20 / 0x80024A60-A98, B0 at 0x80023C00-0x80025134, C0 at 0x80024BA0, 0x80025060/70). A and C (identical code) hold their own
libcard/file wrappers; B only GetC0Table; the other 356 d1 overlays add nothing but one more copy of the A/C set in one module.

| Program | Calls (table fn: name) |
|---|---|
| exe | A0 3A _exit (one caller, 0x8001FE18, a0=-1), 44 FlushCache, 49 GPU_cw, 70 _bu_init, 72 _96_remove; B0 07 DeliverEvent, 08 OpenEvent, 09 CloseEvent, 0A WaitEvent, 0B TestEvent, 0C EnableEvent, 0D DisableEvent, 12 InitPad, 13 StartPad, 14 StopPad, 15 PAD_init, 17 ReturnFromException, 18 ResetEntryInt, 19 HookEntryInt, 35 FileWrite, 4A InitCard, 4B StartCard, 4C StopCard, 56 GetC0Table (x2), 57 GetB0Table (x3), 5B ChangeClearPad; C0 02 SysEnqIntRP, 03 SysDeqIntRP, 0A ChangeClearRCnt |
| A | A0 AB _card_info (0x8004AF94), AC _card_async_load_directory (0x8004AFA4); B0 32 FileOpen (0x8004B064), 33 FileSeek (0x8004B074), 34 FileRead (0x8004B084), 36 FileClose (0x8004B094), 41 format (0x8004B0A4), 42 firstfile (0x8004B3E4), 43 nextfile (0x8004B0B4), 4E write_card_sector (0x8004AFB4), 50 allow_new_card (0x8004AFC4), 56 GetC0Table (0x8004AEC8) |
| B | B0 56 GetC0Table (0x8006AF64) |
| C | same set as A at +0x6140 (e.g. 0x800711A4, 0x800712E4..0x80071334, 0x800715F4, 0x80070C28) |

No `jal 0xA0/0xB0/0xC0` sites exist. The stubs A 0x8004B074/84/94 are B0 33/34/36 (FileSeek/FileRead/FileClose), as the gap list guessed.
`syscall`: exe has EnterCriticalSection (a0=1) and ExitCriticalSection (a0=2), one each. cop0: only `mfc0 SR` / `rfe` in the exe; A/B/C census hits are mostly data.

### Device handler call (A 0x8004B344, C 0x800715C4)
It is not a call into the BIOS device driver: the code at A 0x8004B0D4 copies a device name, walks the device table, and **saves DCB+0x34 of the matching record
into a game global** (A 0x8004EFE4, C 0x8007F880, via `lw 0x34`); 0x8004B270 walks it again, stores a game value back into DCB+0x34 on a name match, and otherwise
calls the saved pointer (`jalr`, args a0=caller, a1=path, a2=buffer). DCB+0x34 is **firstfile** in the layout shared by OpenBIOS and retail (checked on the `bu` and `cdrom`
records: +00 name, +04 flags, +08 block size, +0C desc, +10 init, +14 open, +18 inout, +1C close, +20 ioctl, +24 read, +28 write, +2C erase, +30 undelete,
+34 firstfile, +38 nextfile, +3C format, +40 chdir, +44 rename, +48 deinit, +4C test; stride 0x50). The device table is read from kernel words **0x150 (base) / 0x154 (byte size)**
at A 0x8004B130/40, 0x8004B1D8/E8, 0x8004B2B0/C0 and the same in C (+0x6280).

## 2. Who implements what
OpenBIOS tables (RAM 0x464C in the pinned image: A0 192, C0 32, B0 97 entries; common error stub `0x2AFC` logs "unimplemented", `psxdummy` returns 0):
**every function above is implemented except A0 0x3A** (error stub). Event, pad, card and file calls resolve to real code (`openEvent`..`disableEvent`, kernel RAM
routines for InitPad/StartPad/StopPad/PAD_init/InitCard/StartCard/StopCard/ChangeClearPad/SysEnqIntRP, `psxopen`/`psxread`.., `firstFile`, `mcWriteSector`, `mcAllowNewCard`,
`getC0table`/`getB0table`, `cardInfo`, `buReadTOC`). OpenBIOS also lacks B0 0x46 undelete, 0x54/0x55 GetLast(File)Error, 0x59 testdevice (unused by the game).
psxrecomp HLE (`runtime/src/bios_hle.c`): only B0 07..0D events and the boot-shell skip, refused for OpenBIOS (no DeliverEvent anchor), off in game.toml. Everything else is LLE, i.e. recompiled OpenBIOS.

### Differences that matter (retail SCPH-5501 vs OpenBIOS)
1. **Device table pointer (hard blocker).** Retail stores `[0x150]=0x6EE0`, `[0x154]=count*0x50` during init. OpenBIOS keeps its table as a static array (RAM 0x5F18..0x6238,
   16 records) and never writes 0x150/0x154 (no store with those offsets exists in its image). Game would read 0/0, skip the walk, then `jalr` a zero saved handler at A 0x8004B344.
   Fix: `recomp/bios/so2_compat.c: so2_publish_device_table`.
2. **Card exception hook.** libcard (exe 0x80023B50..0x80023E6C, runs InitCard first) reads two instruction immediates at `C0[6]+0x70/+0x74` (the exception handler), builds a target and writes a
   5-word jump stub at target+0x28, and also writes 5 words at `B0[0x5B]+0x9C8`. Retail installs the 4-word stub there at boot (kernel RAM 0x63BC); OpenBIOS installs it at the first
   card access (RAM 0x281C, per OpenBIOS.toml). Since InitCard precedes the patch this works if InitCard installs it; if not, the target is 0x28 (low-RAM scribble). In OpenBIOS
   `B0[0x5B]+0x9C8 = 0x4A68` falls on B0 slots 0x27..0x2B, reserved and unused, so harmless. Needs the runtime check (test plan 4).
3. A0 0x3A: error stub vs exit. Fix: alias of A0 0x06.
4. Event/thread semantics (EvCB 0x120/0x124, TCB 0x108/0x110): documented ABI, believed compatible, **not compared** (retail vs OpenBIOS trace needed, test plan 6).
5. Retail A0 table in ROM holds pre-patch stubs for 0..9 (patched at runtime); not relevant to the game.

## 3. Hardware
Register-base constants found as data words (libraries use them through pointer tables): exe -> SIO0 0x1F801040, IRQ 0x1F801070/74, DMA ch2 GPU, ch3 CD, ch4 SPU, ch6 OTC plus 0x1F8010F0/F4,
timers 0x1F801100/10/14, CD-ROM 0x1F801800-03, GPU 0x1F801810/14, SPU 0x1F801C00, mem control 0x1F801014/18/20. Direct constant-folded accesses in the exe: timer counters 0x1F801120/24/28 (reads), DMA 0x1F801098.
A's data block 0x800509DC-0x80061C58 (MDEC tables, I/O list incl. 0x1F801080-0x1F8010F0 per RECOMP_NOTES) is excluded; MDEC 0x1F801820 therefore not seen in the exe. Scratchpad 0x1F8000xx is used heavily by B (101 distinct words) and C.
GTE (B / C; A and exe hits are data): RTPS, RTPT, NCLIP (C), MVMVA (23/22), NCDS/NCDT (C), NCCS, NCCT, SQR, AVSZ3, AVSZ4, GPF, GPL, OP; mtc2/ctc2/mfc2/cfc2 plus lwc2/swc2 (192/109 in B). Reserved cmds 0x08/0x17 are data.
Memory card goes through the BIOS card API (A0 AB/AC, B0 4A..4E/50) plus the firstfile hook; pad through B0 12..15; timers through C0 0A and the root-counter reads.

## 4. Blockers for booting without a Sony BIOS, ranked
1. Device table at 0x150/0x154 missing in OpenBIOS (crash or silent save failure at the first card/file touch). Addition written, untested.
2. Libcard exception-hook install order/scribble (silent corruption at 0x28 if EH+0x70 is empty at patch time). Runtime check.
3. firstfile handler calling convention for direct calls (OpenBIOS `dev_bu_firstFile` must take a0=FCB, a1=path, a2=entry).
4. Event/pad/card semantic parity (no evidence of difference, none of equality).
5. A0 0x3A exit (one fatal path). Addition written.
6. Hardware (GTE, GPU, DMA, CD, SPU, MDEC, SIO): runtime tasks, not BIOS gaps; the game needs no BIOS-provided data tables beyond those above.
