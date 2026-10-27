/* Run evidence (the TCP debug server with its screenshot command is compiled out): with PSXRT_VRAM_DUMP_SEC=<n> set,
 * the whole 1024x512 VRAM is written to vram_<k>.ppm in the working directory every n wall seconds.
 * Called once per guest frame from the pad hook (stubs/psxrt_io.c), i.e. on the emulation thread: a background
 * thread deadlocked the GL backend (gpu_vram_peek syncs the GL FBO down, which needs the GL thread). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint16_t gpu_vram_peek(int x, int y);
__declspec(dllimport) unsigned long long __stdcall GetTickCount64(void);

static void dump(unsigned k, unsigned frame) {
    static uint8_t row[1024 * 3];
    char name[32];
    snprintf(name, sizeof name, "vram_%02u.ppm", k);
    FILE *f = fopen(name, "wb");
    if (!f) return;
    fprintf(f, "P6\n1024 512\n255\n");
    for (int y = 0; y < 512; y++) {
        for (int x = 0; x < 1024; x++) {
            uint16_t p = gpu_vram_peek(x, y);
            row[x * 3 + 0] = (uint8_t)((p & 31) << 3);
            row[x * 3 + 1] = (uint8_t)(((p >> 5) & 31) << 3);
            row[x * 3 + 2] = (uint8_t)(((p >> 10) & 31) << 3);
        }
        fwrite(row, 1, sizeof row, f);
    }
    fclose(f);
    fprintf(stderr, "[psxrt-diag] wrote %s at frame %u\n", name, frame);
}

void psxrt_diag_frame(unsigned frame) {
    static int sec = -1;
    static unsigned long long next;
    static unsigned k;
    if (sec < 0) {
        const char *e = getenv("PSXRT_VRAM_DUMP_SEC");
        sec = e ? atoi(e) : 0;
        next = GetTickCount64() + (unsigned long long)sec * 1000u;
    }
    if (sec <= 0 || GetTickCount64() < next) return;
    next += (unsigned long long)sec * 1000u;
    dump(k++, frame);
}
