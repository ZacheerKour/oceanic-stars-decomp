/* Software PS1 GPU: 1 MB VRAM, GP0 command parser, CPU/DMA image transfers and a small
 * rasteriser (flat/gouraud/textured polygons, rectangles, lines), plus PPM dumps of VRAM.
 * Our own code. shortcut: no dithering, no mask bit, no texture window, no interlaced
 * field skipping and no timing; good enough to see frames, not to be pixel exact. */
#include <stdio.h>
#include <string.h>
#include "host.h"

static uint16_t vram[512 * 1024];
#define VR(x, y) vram[((y) & 511) * 1024 + ((x) & 1023)]

static uint32_t fifo[256]; static int nfifo, need;      /* current GP0 command */
static int poly_line;                                     /* polyline: read until terminator */
static int ld_x, ld_y, ld_w, ld_h, ld_i, ld_n, loading; /* A0 CPU->VRAM */
static int st_x, st_y, st_w, st_h, st_i, st_n;           /* C0 VRAM->CPU */
static uint32_t texpage, gp1_mode, disp_x, disp_y, dma_dir, disp_off = 1;
static int da_x0, da_y0, da_x1 = 1023, da_y1 = 511, off_x, off_y;
uint64_t g_gpu_prims;

/* ---- pixels ------------------------------------------------------------------------ */
static void plot(int x, int y, uint16_t c, int semi, int mode) {
    uint16_t b; int r, g, bl, br, bg, bb;
    if (x < da_x0 || x > da_x1 || y < da_y0 || y > da_y1) return;
    if (!semi) { VR(x, y) = c; return; }
    b = VR(x, y);
    r = c & 31; g = c >> 5 & 31; bl = c >> 10 & 31; br = b & 31; bg = b >> 5 & 31; bb = b >> 10 & 31;
    switch (mode) {
    case 0: r = (br + r) >> 1; g = (bg + g) >> 1; bl = (bb + bl) >> 1; break;
    case 1: r += br; g += bg; bl += bb; break;
    case 2: r = br - r; g = bg - g; bl = bb - bl; break;
    default: r = br + r / 4; g = bg + g / 4; bl = bb + bl / 4;
    }
    r = r < 0 ? 0 : r > 31 ? 31 : r; g = g < 0 ? 0 : g > 31 ? 31 : g; bl = bl < 0 ? 0 : bl > 31 ? 31 : bl;
    VR(x, y) = (uint16_t)(r | g << 5 | bl << 10 | (c & 0x8000));
}

/* texel at (u,v) for texpage tp and clut cl; returns 0 for the transparent colour */
static uint16_t texel(uint32_t tp, uint32_t cl, int u, int v) {
    int bx = (tp & 15) * 64, by = (tp >> 4 & 1) * 256, depth = tp >> 7 & 3, cx = (cl & 63) * 16, cy = cl >> 6 & 511;
    u &= 255; v &= 255;
    if (depth == 0) { uint16_t w = VR(bx + u / 4, by + v); return VR(cx + (w >> (u & 3) * 4 & 15), cy); }
    if (depth == 1) { uint16_t w = VR(bx + u / 2, by + v); return VR(cx + (w >> (u & 1) * 8 & 255), cy); }
    return VR(bx + u, by + v);
}

typedef struct { int x, y, r, g, b, u, v; } Vtx;

/* colour (24-bit cmd colour or gouraud) x texel -> 15-bit pixel */
static void shade(int x, int y, int r, int g, int b, int tex, int raw, uint32_t tp, uint32_t cl, int u, int v, int semi) {
    uint16_t c;
    if (tex) {
        uint16_t t = texel(tp, cl, u, v);
        if (!t) return;
        if (!raw) {
            int tr = (t & 31) * r / 128, tg = (t >> 5 & 31) * g / 128, tb = (t >> 10 & 31) * b / 128;
            c = (uint16_t)((tr > 31 ? 31 : tr) | (tg > 31 ? 31 : tg) << 5 | (tb > 31 ? 31 : tb) << 10 | (t & 0x8000));
        } else c = t;
        plot(x, y, c, semi && (t & 0x8000), tp >> 5 & 3);
    } else {
        c = (uint16_t)(r >> 3 | (g >> 3) << 5 | (b >> 3) << 10);
        plot(x, y, c, semi, tp >> 5 & 3);
    }
}

static int edge(const Vtx *a, const Vtx *b, int x, int y) { return (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x); }

static void tri(Vtx a, Vtx b, Vtx c, int gour, int tex, int raw, int semi, uint32_t tp, uint32_t cl) {
    int x0, x1, y0, y1, x, y, area = edge(&a, &b, c.x, c.y);
    if (!area) return;
    if (area < 0) { Vtx t = b; b = c; c = t; area = -area; }
    if (b.x - a.x > 1023 || c.x - a.x > 1023 || c.x - b.x > 1023 || a.x - b.x > 1023 || a.x - c.x > 1023 || b.x - c.x > 1023) return;
    if (b.y - a.y > 511 || c.y - a.y > 511 || c.y - b.y > 511 || a.y - b.y > 511 || a.y - c.y > 511 || b.y - c.y > 511) return;
    x0 = a.x < b.x ? a.x : b.x; x0 = x0 < c.x ? x0 : c.x; x1 = a.x > b.x ? a.x : b.x; x1 = x1 > c.x ? x1 : c.x;
    y0 = a.y < b.y ? a.y : b.y; y0 = y0 < c.y ? y0 : c.y; y1 = a.y > b.y ? a.y : b.y; y1 = y1 > c.y ? y1 : c.y;
    if (x0 < da_x0) x0 = da_x0; if (x1 > da_x1) x1 = da_x1; if (y0 < da_y0) y0 = da_y0; if (y1 > da_y1) y1 = da_y1;
    for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) {
        long long w0 = edge(&b, &c, x, y), w1 = edge(&c, &a, x, y), w2 = edge(&a, &b, x, y);
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        if ((w0 == 0 && x == x1) || (w1 == 0 && x == x1) || (w2 == 0 && x == x1)) continue; /* shortcut: crude right-edge exclusion */
        {
            int r = a.r, g = a.g, bl = a.b, u = 0, v = 0;
            if (gour) { r = (int)((w0 * a.r + w1 * b.r + w2 * c.r) / area); g = (int)((w0 * a.g + w1 * b.g + w2 * c.g) / area); bl = (int)((w0 * a.b + w1 * b.b + w2 * c.b) / area); }
            if (tex) { u = (int)((w0 * a.u + w1 * b.u + w2 * c.u) / area); v = (int)((w0 * a.v + w1 * b.v + w2 * c.v) / area); }
            shade(x, y, r, g, bl, tex, raw, tp, cl, u, v, semi);
        }
    }
}

static void line(Vtx a, Vtx b, int gour, int semi) {
    int dx = b.x - a.x, dy = b.y - a.y, n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy), i;
    if (n > 1023) return;
    for (i = 0; i <= n; i++) {
        int x = n ? a.x + dx * i / n : a.x, y = n ? a.y + dy * i / n : a.y;
        int r = gour && n ? a.r + (b.r - a.r) * i / n : a.r, g = gour && n ? a.g + (b.g - a.g) * i / n : a.g, bl = gour && n ? a.b + (b.b - a.b) * i / n : a.b;
        shade(x, y, r, g, bl, 0, 0, texpage, 0, 0, 0, semi);
    }
}

static void vxy(Vtx *v, uint32_t w) { v->x = ((int)(int16_t)(w << 5) >> 5) + off_x; v->y = ((int)(int16_t)((w >> 16) << 5) >> 5) + off_y; }
static void vcol(Vtx *v, uint32_t w) { v->r = w & 255; v->g = w >> 8 & 255; v->b = w >> 16 & 255; }

/* ---- commands ------------------------------------------------------------------------- */
static int cmd_len(uint32_t w) {
    uint32_t c = w >> 24;
    if (c >= 0x20 && c < 0x40) { int nv = (c & 8) ? 4 : 3; return 1 + nv * (1 + ((c & 4) ? 1 : 0)) + ((c & 0x10) ? nv - 1 : 0); }
    if (c >= 0x40 && c < 0x60) return (c & 8) ? 255 : ((c & 0x10) ? 4 : 3);
    if (c >= 0x60 && c < 0x80) return 2 + ((c & 4) ? 1 : 0) + (((c >> 3) & 3) == 0 ? 1 : 0);
    if (c >= 0x80 && c < 0xA0) return 4;
    if (c >= 0xA0 && c < 0xE0) return 3;
    if (c == 0x02) return 3;
    return 1;
}

static void run_poly(void) {
    uint32_t c = fifo[0] >> 24, tp = texpage, cl = 0;
    int quad = c & 8, gour = c & 0x10, tex = c & 4, semi = c & 2, raw = c & 1, nv = quad ? 4 : 3, i, k = 1;
    Vtx v[4];
    for (i = 0; i < nv; i++) {
        if (i == 0) vcol(&v[0], fifo[0]); else if (gour) vcol(&v[i], fifo[k++]); else vcol(&v[i], fifo[0]);
        vxy(&v[i], fifo[k++]);
        if (tex) {
            v[i].u = fifo[k] & 255; v[i].v = fifo[k] >> 8 & 255;
            if (i == 0) cl = fifo[k] >> 16;
            if (i == 1) { tp = fifo[k] >> 16; texpage = (texpage & ~0x9FFu) | (tp & 0x9FF); }
            k++;
        } else v[i].u = v[i].v = 0;
    }
    if (!tex && !gour) raw = 0;
    tri(v[0], v[1], v[2], gour, tex, raw, semi, tp, cl);
    if (quad) tri(v[1], v[2], v[3], gour, tex, raw, semi, tp, cl);
}

static void run_rect(void) {
    uint32_t c = fifo[0] >> 24, cl = 0;
    int tex = c & 4, semi = c & 2, raw = c & 1, sz = c >> 3 & 3, k = 2, w, h, x, y, u0 = 0, v0 = 0;
    Vtx p; vcol(&p, fifo[0]); vxy(&p, fifo[1]);
    if (tex) { u0 = fifo[2] & 255; v0 = fifo[2] >> 8 & 255; cl = fifo[2] >> 16; k = 3; }
    if (sz == 0) { w = fifo[k] & 0x3FF; h = fifo[k] >> 16 & 0x1FF; } else w = h = sz == 1 ? 1 : sz == 2 ? 8 : 16;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++)
        shade(p.x + x, p.y + y, p.r, p.g, p.b, tex, raw, texpage, cl, u0 + x, v0 + y, semi);
}

static void run_cmd(void) {
    uint32_t c = fifo[0] >> 24;
    g_gpu_prims++;
    if (c >= 0x20 && c < 0x40) run_poly();
    else if (c >= 0x40 && c < 0x60) {
        int gour = c & 0x10, semi = c & 2, i, k = 1; Vtx a, b;
        vcol(&a, fifo[0]); vxy(&a, fifo[k++]);
        while (k < nfifo) {
            b = a; if (gour) vcol(&b, fifo[k++]);
            if (k >= nfifo) break;
            vxy(&b, fifo[k++]); line(a, b, gour, semi); a = b;
        }
        (void)i;
    } else if (c >= 0x60 && c < 0x80) run_rect();
    else if (c == 0x02) {
        int x0 = fifo[1] & 0x3F0, y0 = fifo[1] >> 16 & 0x1FF, w = ((fifo[2] & 0x3FF) + 15) & ~15, h = fifo[2] >> 16 & 0x1FF, x, y;
        uint16_t col = (uint16_t)((fifo[0] & 255) >> 3 | ((fifo[0] >> 8 & 255) >> 3) << 5 | ((fifo[0] >> 16 & 255) >> 3) << 10);
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) VR(x0 + x, y0 + y) = col;
    } else if (c >= 0x80 && c < 0xA0) {
        int sx = fifo[1] & 1023, sy = fifo[1] >> 16 & 511, dx = fifo[2] & 1023, dy = fifo[2] >> 16 & 511;
        int w = ((fifo[3] - 1) & 1023) + 1, h = ((fifo[3] >> 16) - 1 & 511) + 1, x, y;
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) VR(dx + x, dy + y) = VR(sx + x, sy + y);
    } else if (c >= 0xA0 && c < 0xC0) {
        ld_x = fifo[1] & 1023; ld_y = fifo[1] >> 16 & 511;
        ld_w = ((fifo[2] & 0xFFFF) - 1 & 1023) + 1; ld_h = ((fifo[2] >> 16) - 1 & 511) + 1;
        ld_i = 0; ld_n = ld_w * ld_h; loading = 1;
    } else if (c >= 0xC0 && c < 0xE0) {
        st_x = fifo[1] & 1023; st_y = fifo[1] >> 16 & 511;
        st_w = ((fifo[2] & 0xFFFF) - 1 & 1023) + 1; st_h = ((fifo[2] >> 16) - 1 & 511) + 1;
        st_i = 0; st_n = st_w * st_h;
    } else if (c == 0xE1) texpage = fifo[0] & 0x3FFF;
    else if (c == 0xE3) { da_x0 = fifo[0] & 1023; da_y0 = fifo[0] >> 10 & 511; }
    else if (c == 0xE4) { da_x1 = fifo[0] & 1023; da_y1 = fifo[0] >> 10 & 511; }
    else if (c == 0xE5) { off_x = (int)(fifo[0] << 21) >> 21; off_y = (int)(fifo[0] << 10) >> 21; }
}

void gpu_gp0(uint32_t w) {
    if (loading) {
        int k;
        for (k = 0; k < 2 && ld_i < ld_n; k++, ld_i++) VR(ld_x + ld_i % ld_w, ld_y + ld_i / ld_w) = (uint16_t)(w >> 16 * k);
        if (ld_i >= ld_n) loading = 0;
        return;
    }
    if (!nfifo) { need = cmd_len(w); poly_line = need == 255; }
    if (nfifo < 256) fifo[nfifo++] = w;
    if (poly_line) { if (nfifo > 2 && (w & 0xF000F000u) == 0x50005000u) { nfifo--; run_cmd(); nfifo = 0; } return; }
    if (nfifo >= need) { run_cmd(); nfifo = 0; }
}

void gpu_gp1(uint32_t w) {
    switch (w >> 24) {
    case 0x00: nfifo = 0; loading = 0; st_n = 0; texpage = 0; disp_off = 1; dma_dir = 0; gp1_mode = 0; break;
    case 0x01: nfifo = 0; loading = 0; break;
    case 0x03: disp_off = w & 1; break;
    case 0x04: dma_dir = w & 3; break;
    case 0x05: disp_x = w & 0x3FE; disp_y = w >> 10 & 0x1FF; break;
    case 0x08: gp1_mode = w & 0xFF; break;
    }
}

uint32_t gpu_read(void) {
    uint32_t v = 0; int k;
    for (k = 0; k < 2 && st_i < st_n; k++, st_i++) v |= (uint32_t)VR(st_x + st_i % st_w, st_y + st_i / st_w) << 16 * k;
    return v;
}

uint32_t gpu_stat(int odd_field) {
    uint32_t m = gp1_mode, s = (texpage & 0x7FF) | 0x1C000000u;
    s |= (m >> 6 & 1) << 16 | (m & 3) << 17 | (m >> 2 & 1) << 19 | (m >> 3 & 1) << 20 | (m >> 4 & 1) << 21 | (m >> 5 & 1) << 22;
    s |= disp_off << 23 | dma_dir << 29;
    if (dma_dir == 1 || dma_dir == 2) s |= 1u << 25;
    if (dma_dir == 3) s |= 1u << 25;
    if (odd_field) s |= 1u << 31;
    return s;
}

/* ---- dumps (into the run's gitignored _out dir) ---------------------------------------- */
static void ppm(const char *path, int x0, int y0, int w, int h) {
    FILE *f = fopen(path, "wb"); int x, y;
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        uint16_t c = VR(x0 + x, y0 + y); uint8_t p[3];
        p[0] = (uint8_t)((c & 31) << 3); p[1] = (uint8_t)((c >> 5 & 31) << 3); p[2] = (uint8_t)((c >> 10 & 31) << 3);
        fwrite(p, 1, 3, f);
    }
    fclose(f);
}
void gpu_dump_vram(const char *path) { ppm(path, 0, 0, 1024, 512); }
void gpu_dump_display(const char *path) {
    static const int hw[8] = { 256, 320, 512, 640, 368, 368, 368, 368 };
    int w = (gp1_mode & 0x40) ? 368 : hw[gp1_mode & 3], h = ((gp1_mode & 0x24) == 0x24) ? 480 : 240;
    (void)hw[4];
    ppm(path, (int)disp_x, (int)disp_y, w, h);
}
