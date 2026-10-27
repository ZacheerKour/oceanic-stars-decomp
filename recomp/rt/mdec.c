/* MDEC (motion decoder): run-length + IDCT + YUV->RGB for FMV, fed by DMA0, read by DMA1.
 * Our own code, from the public psx-spx description. shortcut: float IDCT instead of the
 * hardware's scale-table fixed point, and monochrome (4/8-bit) output is not decoded
 * (zeros); fine for viewing frames, revisit if a game uses mono output. */
#include <math.h>
#include <string.h>
#include "host.h"

static const uint8_t zz[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63 };

static uint8_t qt_y[64], qt_c[64];
static uint32_t in[0x10000]; static uint32_t nin, need, cmd;   /* current command + its parameter words */
static uint32_t out[0x20000]; static uint32_t nout, outpos;     /* decoded words waiting for DMA1 */
static uint32_t ctrl;                                           /* MDEC1 bits 30/29: DMA enables */
static double cosk[8][8]; static int cos_ready;

static int s10(uint32_t v) { return (int)(v << 22) >> 22; }

/* decode one 8x8 block from halfword stream h[*p..n); returns 0 at end of data */
static int block(const uint16_t *h, uint32_t n, uint32_t *p, const uint8_t *qt, double *dst) {
    int blk[64], k, q; uint16_t w; double tmp[64]; int x, y, u;
    memset(blk, 0, sizeof blk);
    do { if (*p >= n) return 0; w = h[(*p)++]; } while (w == 0xFE00);
    q = w >> 10 & 63;
    k = 0;
    {
        int val = s10(w) * qt[0];
        for (;;) {
            if (q == 0) val = s10(w) * 2;
            val = val < -0x400 ? -0x400 : val > 0x3FF ? 0x3FF : val;
            if (q) blk[zz[k]] = val; else blk[k] = val;
            if (*p >= n) break;
            w = h[(*p)++];
            k += (w >> 10 & 63) + 1;
            if (k > 63) break;
            val = (s10(w) * qt[k] * q + 4) / 8;
        }
    }
    if (!cos_ready) { for (x = 0; x < 8; x++) for (u = 0; u < 8; u++) cosk[x][u] = (u ? 1.0 : sqrt(0.5)) * cos((2 * x + 1) * u * 3.14159265358979 / 16) / 2; cos_ready = 1; }
    for (y = 0; y < 8; y++) for (u = 0; u < 8; u++) { double s = 0; int v; for (v = 0; v < 8; v++) s += cosk[y][v] * blk[v * 8 + u]; tmp[y * 8 + u] = s; }
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) { double s = 0; for (u = 0; u < 8; u++) s += cosk[x][u] * tmp[y * 8 + u]; dst[y * 8 + x] = s; }
    return 1;
}

static int clamp8(double v) { int i = (int)floor(v + 0.5); return i < 0 ? 0 : i > 255 ? 255 : i; }

static void decode(void) {
    const uint16_t *h = (const uint16_t *)in;
    uint32_t n = (cmd & 0xFFFF) * 2, p = 0, depth = cmd >> 27 & 3, b15 = (cmd >> 25 & 1) << 15;
    double cr[64], cb[64], yb[4][64];
    nout = outpos = 0;
    if (depth < 2) { /* mono: not decoded (see header) */ return; }
    for (;;) {
        int i, x, y; uint8_t px[16 * 16 * 3];
        if (!block(h, n, &p, qt_c, cr) || !block(h, n, &p, qt_c, cb)) break;
        for (i = 0; i < 4; i++) if (!block(h, n, &p, qt_y, yb[i])) break;
        if (i < 4) break;
        for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
            double Y = yb[(y >> 3) * 2 + (x >> 3)][(y & 7) * 8 + (x & 7)] + 128, R = cr[(y >> 1) * 8 + (x >> 1)], B = cb[(y >> 1) * 8 + (x >> 1)];
            uint8_t *o = px + (y * 16 + x) * 3;
            o[0] = (uint8_t)clamp8(Y + 1.402 * R); o[1] = (uint8_t)clamp8(Y - 0.3437 * B - 0.7143 * R); o[2] = (uint8_t)clamp8(Y + 1.772 * B);
        }
        if (depth == 2) {   /* 24-bit: 768 bytes = 192 words */
            if (nout + 192 > sizeof out / 4) break;
            memcpy(out + nout, px, 768); nout += 192;
        } else {            /* 15-bit: 256 halfwords = 128 words */
            uint16_t hw[256];
            if (nout + 128 > sizeof out / 4) break;
            for (i = 0; i < 256; i++) hw[i] = (uint16_t)(px[i * 3] >> 3 | (px[i * 3 + 1] >> 3) << 5 | (px[i * 3 + 2] >> 3) << 10 | b15);
            memcpy(out + nout, hw, 512); nout += 128;
        }
    }
}

static void finish(void) {
    uint32_t c = cmd >> 29, i;
    if (c == 1) decode();
    else if (c == 2) { for (i = 0; i < 16; i++) memcpy(qt_y + i * 4, &in[i], 4); if (cmd & 1) for (i = 0; i < 16; i++) memcpy(qt_c + i * 4, &in[16 + i], 4); }
    /* c == 3: scale table, unused by the float IDCT */
    need = 0; nin = 0;
}

void mdec_write0(uint32_t v) {
    if (!need) {
        uint32_t c = v >> 29;
        cmd = v; nin = 0;
        need = c == 1 ? (v & 0xFFFF) : c == 2 ? ((v & 1) ? 32 : 16) : c == 3 ? 32 : 0;
        if (!need) finish();
        return;
    }
    if (nin < sizeof in / 4) in[nin] = v;
    nin++;
    if (nin >= need) finish();
}

void mdec_write1(uint32_t v) {
    if (v & 0x80000000u) { need = nin = 0; nout = outpos = 0; }
    ctrl = v & 0x60000000u;
}

uint32_t mdec_read0(void) { return outpos < nout ? out[outpos++] : 0; }

uint32_t mdec_status(void) {
    uint32_t s = 0;
    if (outpos >= nout) s |= 0x80000000u;
    if (need) s |= 0x20000000u | ((need - nin - 1) & 0xFFFF); else s |= 0xFFFF;
    if (ctrl & 0x40000000u) s |= 0x10000000u;
    if ((ctrl & 0x20000000u) && outpos < nout) s |= 0x08000000u;
    s |= (cmd >> 27 & 3) << 25 | (cmd >> 26 & 1) << 24 | (cmd >> 25 & 1) << 23;
    return s;
}

uint32_t mdec_out_avail(void) { return nout - outpos; }
