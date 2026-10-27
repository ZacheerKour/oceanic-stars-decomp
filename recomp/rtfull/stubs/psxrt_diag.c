/* Run evidence (the TCP debug server with its screenshot command is compiled out): with PSXRT_VRAM_DUMP_SEC=<n> set,
 * the whole 1024x512 VRAM is written to vram_<k>.ppm in the working directory every n wall seconds.
 * Called once per guest frame from the pad hook (stubs/psxrt_io.c), i.e. on the emulation thread: a background
 * thread deadlocked the GL backend (gpu_vram_peek syncs the GL FBO down, which needs the GL thread). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

uint16_t gpu_vram_peek(int x, int y);
__declspec(dllimport) unsigned long long __stdcall GetTickCount64(void);

static void dump_to(const char *name, unsigned frame) {
    static uint8_t row[1024 * 3];
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

int present_shot_request(const char *path);   /* main.cpp: staged, written as PNG by the next window present */

/* script DUMP action: vram_<tag>.ppm now, and win_<tag>.png = the composed window (letterbox, scaling, OSD) at the next present */
void psxrt_diag_dump_now(const char *tag, unsigned frame) {
    char name[64];
    snprintf(name, sizeof name, "vram_%s.ppm", tag && *tag ? tag : "x");
    dump_to(name, frame);
    snprintf(name, sizeof name, "win_%s.png", tag && *tag ? tag : "x");
    present_shot_request(name);
}

/* PSXRT_WINDOW_AT=<frame>:<w>x<h>: resize the game window at that guest frame (window-resize redraw / letterbox tests) */
static void window_at(unsigned frame) {
    static unsigned at = ~0u;
    static int w, h;
    if (at == ~0u) {
        const char *e = getenv("PSXRT_WINDOW_AT");
        if (!e || sscanf(e, "%u:%dx%d", &at, &w, &h) != 3) at = 0;
    }
    if (!at || frame != at) return;
    int n = 0;
    SDL_Window **ws = SDL_GetWindows(&n);
    if (ws && n > 0) SDL_SetWindowSize(ws[0], w, h);
    fprintf(stderr, "[psxrt-diag] window -> %dx%d at frame %u (%d windows)\n", w, h, frame, n);
    SDL_free(ws);
}

void psxrt_diag_frame(unsigned frame) {
    window_at(frame);
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
    char name[32];
    snprintf(name, sizeof name, "vram_%02u.ppm", k++);
    dump_to(name, frame);
}

