# SO2 compatibility additions for OpenBIOS (recomp/bios)

Source of truth for the numbers: `docs/BIOS_USAGE.md` and `python -I analysis/bios_usage.py`.
OpenBIOS source is not in the clone (only `openbios.bin`, `OpenBIOS.toml`, ELF symbol seeds), so this is an addition to be
merged into an OpenBIOS source build, not a binary patch. `so2_compat.c` is untested and uncompiled.

## Behaviour to implement
1. **Device table pointer.** `[0x150]` = address of an array of 0x50-byte device control blocks, `[0x154]` = array size in
   bytes. NULL name = free slot. Both words must be valid before the first game instruction (the game reads them in the
   libcard/file wrappers at A 0x8004B130, 0x8004B2B0 and C 0x800713B0, 0x80071530). OpenBIOS: set from its static array.
2. **DCB layout** (already identical in OpenBIOS and the retail BIOS, checked on the `bu` and `cdrom` records): +00 name ptr,
   +04 flags, +08 block size, +0C description ptr, +10 init, +14 open, +18 inout, +1C close, +20 ioctl, +24 read, +28 write,
   +2C erase, +30 undelete, +34 **firstfile**, +38 nextfile, +3C format, +40 chdir, +44 rename, +48 deinit, +4C test.
   No change needed; do not reorder.
3. **firstfile handler contract.** The game saves DCB+0x34 of the `bu` device, later calls it directly as
   `handler(a0 = game FCB, a1 = path without device prefix, a2 = caller buffer)`, and may overwrite DCB+0x34 with the saved
   value. OpenBIOS `dev_bu_firstFile` must accept that calling convention (verify in test 3).
4. **A0 0x3A** behaves as exit(code) (alias of A0 0x06).

## Test plan (needs a running OpenBIOS build; none of this has been run)
1. Boot to the first game frame; read RAM 0x150/0x154: base inside RAM, size = N*0x50, first non-NULL record is `tty`/`cdrom`.
2. Break on A 0x8004B344 and C 0x800715C4: v0 must be a code address (the saved handler), never 0.
3. Memory-card directory listing: call the game's save-menu path with an empty and a formatted card image; the firstfile
   result (v0) and the direntry buffer must match the same run on a retail BIOS (record the SCPH-5501 result locally; do not commit).
4. Break on stores to 0x28..0x3C and to 0x4A68..0x4A7C: the libcard patch (exe 0x80023B74/0x80023B7C) must hit neither
   0x28 (it would mean EH+0x70 was still zero at patch time) nor anything but B0 slots 0x27..0x2B (reserved) at 0x4A68.
5. `A0(0x3A)` with a0 = -1 does not print "unimplemented" and does not return.
6. Events/threads: log OpenEvent/EnableEvent/WaitEvent/TestEvent/DeliverEvent/CloseEvent args and returns during boot+first menu;
   compare against the same trace on retail (class, spec, mode, handler, returned handle).
