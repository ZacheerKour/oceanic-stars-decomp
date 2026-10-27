/* Headless evidence (the TCP debug server with its screenshot command is compiled out): with PSXRT_VRAM_DUMP_SEC=<n> set,
 * a background thread writes the whole 1024x512 VRAM to vram_<k>.ppm in the working directory every n seconds.
 * Reads are racy (diagnostics only). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint16_t gpu_vram_peek(int x, int y);
__declspec(dllimport) void *__stdcall CreateThread(void *, size_t, unsigned long(__stdcall *)(void *), void *, unsigned long, unsigned long *);
__declspec(dllimport) void __stdcall Sleep(unsigned long);

static unsigned long __stdcall diag_thread(void *arg) {
    unsigned sec = (unsigned)(uintptr_t)arg;
    static uint8_t row[1024 * 3];
    for (unsigned k = 0;; k++) {
        Sleep(sec * 1000u);
        char name[32];
        snprintf(name, sizeof name, "vram_%02u.ppm", k);
        FILE *f = fopen(name, "wb");
        if (!f) continue;
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
        fprintf(stderr, "[psxrt-diag] wrote %s\n", name);
    }
    return 0;
}

static void diag_start(void) {
    const char *e = getenv("PSXRT_VRAM_DUMP_SEC");
    unsigned sec = e ? (unsigned)atoi(e) : 0u;
    if (sec) CreateThread(0, 0, diag_thread, (void *)(uintptr_t)sec, 0, 0);
}
#pragma section(".CRT$XCU", read)
#pragma comment(linker, "/include:psxrt_diag_init")
__declspec(allocate(".CRT$XCU")) void (*psxrt_diag_init)(void) = diag_start;
