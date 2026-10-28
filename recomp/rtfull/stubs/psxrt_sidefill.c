/* Side fill (docs/notes/2026-10-28-window-sidefill.md): settings.ini side_fill = off | blur | art -> PSXRT_SIDE_FILL, and the local
 * art images in PSXRT_SIDEFILL_DIR (play\sidefill\*.png, never committed). The GL drawing is psxrt_sidefill.inc, compiled inside
 * gpu_gl_renderer.c; this TU does the file work so that one does not need windows.h. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

int psxrt_sidefill_mode(void) {   /* 0 off, 1 blur (default), 2 art */
    const char *e = getenv("PSXRT_SIDE_FILL");
    return !e ? 1 : !strcmp(e, "off") ? 0 : !strcmp(e, "art") ? 2 : 1;
}

static unsigned char *load_png(const wchar_t *path, int *w, int *h) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = (n > 0 && n <= (64L << 20)) ? (unsigned char *)malloc((size_t)n) : NULL;
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    int c = 0;
    unsigned char *px = NULL;
    if (stbi_info_from_memory(buf, (int)n, w, h, &c) && *w > 0 && *h > 0 && *w <= 8192 && *h <= 8192)
        px = stbi_load_from_memory(buf, (int)n, w, h, &c, 4);
    free(buf);
    return px;
}

/* One random PNG of the folder (random per boot), RGBA, first row = top; NULL when the folder is unset, empty or unreadable.
 * A file that does not decode is skipped and the next one tried. Caller frees with free(). */
unsigned char *psxrt_sidefill_pick(int *w, int *h) {
    const wchar_t *dir = _wgetenv(L"PSXRT_SIDEFILL_DIR");
    if (!dir || !*dir) return NULL;
    wchar_t pat[MAX_PATH + 8];
    if (_snwprintf(pat, MAX_PATH, L"%ls%lc*.png", dir, (wchar_t)92) < 0) return NULL;
    static wchar_t names[64][MAX_PATH];
    int n = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h0 = FindFirstFileW(pat, &fd);
    if (h0 == INVALID_HANDLE_VALUE) return NULL;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < 64 && _snwprintf(names[n], MAX_PATH, L"%ls%lc%ls", dir, (wchar_t)92, fd.cFileName) >= 0) n++;
    } while (FindNextFileW(h0, &fd));
    FindClose(h0);
    if (!n) return NULL;
    srand((unsigned)time(NULL) ^ GetCurrentProcessId());
    int start = rand() % n;
    for (int i = 0; i < n; i++) {
        unsigned char *px = load_png(names[(start + i) % n], w, h);
        if (px) return px;
    }
    return NULL;
}
