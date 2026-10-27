/* so2_compat.c - Star Ocean 2 compatibility additions for PCSX-Redux OpenBIOS (pinned 55fbf04).
 * ORIGINAL code, written from observed behaviour (docs/BIOS_USAGE.md), not copied from any BIOS.
 * UNTESTED: not compiled here (no mipsel toolchain on this machine). Integration points are named below;
 * addresses in comments are for the pinned psxrecomp bios/openbios.bin and move if the BIOS is rebuilt.
 */
#include <stdint.h>

#define KVAR(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define DCB_SIZE 0x50u

/* ---- 1. Publish the device table at the fixed kernel variables 0x150 (base) / 0x154 (byte size) ----
 * The game reads these two words directly, walks 0x50-byte records until base+size and treats a NULL name as a free
 * slot, then reads/writes DCB+0x34 (firstfile in this layout, identical in OpenBIOS and retail).
 * OpenBIOS keeps the table as a static array (pinned build: RAM 0x5F18..0x6238, 16 records) and never writes
 * 0x150/0x154.  Call once from OpenBIOS init, after the array exists and before any game code runs
 * (it is static storage, so base/size never change; devices added later only fill NULL-name slots). */
void so2_publish_device_table(void *table_base, uint32_t record_count)
{
    KVAR(0x150) = (uint32_t)(uintptr_t)table_base;
    KVAR(0x154) = record_count * DCB_SIZE;
}

/* ---- 2. A0 0x3A (alias of exit) ----
 * OpenBIOS routes A0 0x3A to its "unimplemented" logger.  The game calls it once (exe 0x8001FE18, argument -1):
 * a fatal exit.  Define it as exit(code): same function as A0 0x06 / B0 0x38.  Table patch (in the A0 table
 * initialiser): a0_table[0x3A] = a0_table[0x06]; */
void so2_patch_a0_exit_alias(void **a0_table)
{
    a0_table[0x3A] = a0_table[0x06];
}
