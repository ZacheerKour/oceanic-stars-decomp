/* psxrt_disc.c - scripted CD lid cycles for headless disc-swap tests (off unless PSXRT_DISC_SCRIPT is set).
 *
 * PSXRT_DISC_SCRIPT=<file>: lines "<vblank> <disc>" (disc 1-based roster entry, 0 = reinsert the same disc), in
 * ascending order. At that VBlank the runtime's own lid path runs (psx_disc_tray_cycle: ShellOpen, image swap, 2 s
 * timed close), exactly as the F10 disc menu would. After every cycle the next 64 SetLoc LBA changes are logged, and
 * every SetLoc to LBA 300 (the SO2.BIN TOC, which the game re-reads after a lid close: exe 0x80011eb4 -> 0x80011c20).
 * Hooked into psx_disc_tray_auto_poll (main.cpp calls it once per VBlank on the emu thread) by stubs/psxrt_disc.cmake.
 * RECOMP_NOTES '2026-10-27 disc 2'. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cdrom.h"
#include "psx_disc_tray.h"

#define MAX_STEPS 16
static struct { uint32_t vb; int disc; } s_steps[MAX_STEPS];
static int s_n = -1, s_next, s_watch, s_last_lba = -2;
static uint32_t s_vb;

static int load(const char *path) {
    FILE *f = fopen(path, "r");
    char line[128];
    int n = 0;
    if (!f) { fprintf(stderr, "[psxrt-disc] cannot open %s\n", path); return 0; }
    while (n < MAX_STEPS && fgets(line, sizeof line, f)) {
        unsigned vb; int disc;
        if (line[0] == '#' || sscanf(line, "%u %d", &vb, &disc) != 2) continue;
        if (n && vb < s_steps[n - 1].vb) { fprintf(stderr, "[psxrt-disc] out of order: %s", line); continue; }
        s_steps[n].vb = vb; s_steps[n].disc = disc; n++;
    }
    fclose(f);
    fprintf(stderr, "[psxrt-disc] %d steps from %s, roster %d, mounted %d\n", n, path,
            psx_disc_tray_disc_count(), psx_disc_tray_mounted_disc());
    return n;
}

void psxrt_disc_vblank(void) {
    uint32_t vb = s_vb++;
    if (s_n < 0) { const char *p = getenv("PSXRT_DISC_SCRIPT"); s_n = p && *p ? load(p) : 0; }
    if (!s_n) return;
    if (s_next < s_n && vb >= s_steps[s_next].vb) {
        int disc = s_steps[s_next++].disc;
        int rc = psx_disc_tray_cycle(disc, PSX_DISC_TRAY_ORIGIN_DEBUG_SELECT);
        fprintf(stderr, "[psxrt-disc] vblank %u cycle(disc %d) -> %s, mounted %d\n", vb, disc,
                psx_disc_tray_result_name(rc), psx_disc_tray_mounted_disc());
        s_watch = 64;
    }
    int lba = cdrom_get_setloc_lba();
    if (lba != s_last_lba) {
        if (s_watch > 0 || lba == 300)
            fprintf(stderr, "[psxrt-disc] vblank %u setloc %d (mounted %d)\n", vb, lba, psx_disc_tray_mounted_disc());
        if (s_watch > 0) s_watch--;
        s_last_lba = lba;
    }
}
