/* Minimal PS1 CD-ROM controller: the command subset libcd and the BIOS use,
 * data reads from a raw 2352-byte-sector .bin opened read-only. No audio (the
 * game has no XA/CDDA). Timing is approximate. */
#include <stdio.h>
#include <string.h>
#include "host.h"

#define ACK_DELAY   50000u
#define DONE_DELAY 100000u
#define SECTOR_1X  451584u   /* 33.8688 MHz / 75 */

typedef struct { uint8_t type, n, b[8]; uint32_t delay; } Resp;

static FILE *disc;
static long long disc_sectors;
static uint8_t idx, ie = 0x1F /* reset value, as DuckStation CDROM::Reset */, iflag, params[16], nparams, stat = 0x02, mode;
static uint8_t resp[16], nresp, rpos;
static Resp q[8]; static int nq;
static uint64_t now, due;
static uint32_t loc, read_lba; static int reading;
static uint64_t next_sector;
static uint8_t sector[2352], fifo[2352]; static uint32_t fifo_n, fifo_pos;
static uint32_t n_cmd[32], n_sectors, n_unknown;
static uint32_t *g_istat;
int g_cd_trace; /* >0: print this many register accesses */
static uint32_t locs[12], nlocs; static uint8_t modes[12];

void host_cd_open(const char *bin_path) {
    disc = fopen(bin_path, "rb");
    if (!disc) { fprintf(stderr, "cannot open disc %s\n", bin_path); return; }
    _fseeki64(disc, 0, SEEK_END); disc_sectors = _ftelli64(disc) / 2352;
}

static void push(uint8_t type, uint32_t delay, int n, const uint8_t *b) {
    if (nq == 8) return;
    q[nq].type = type; q[nq].delay = delay; q[nq].n = (uint8_t)n; memcpy(q[nq].b, b, n); nq++;
    if (nq == 1) due = now + delay;
}

static void deliver(void) {
    if (!nq || iflag || now < due) return;
    iflag = q[0].type; memcpy(resp, q[0].b, q[0].n); nresp = q[0].n; rpos = 0;
    memmove(q, q + 1, sizeof(Resp) * --nq);
    if (nq) due = now + q[0].delay;
    if ((iflag & ie) && g_istat) *g_istat |= 4;
}

static uint8_t bcd(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 15)); }
static uint8_t tobcd(uint32_t v) { return (uint8_t)((v / 10) << 4 | (v % 10)); }

static void command(uint8_t c) {
    uint8_t s[8]; int n = 1;
    n_cmd[c & 31]++;
    s[0] = stat;
    switch (c) {
    case 0x01: case 0x0B: case 0x0C: case 0x0D: push(3, ACK_DELAY, 1, s); break;
    case 0x02: loc = (bcd(params[0]) * 60u + bcd(params[1])) * 75u + bcd(params[2]) - 150u; if (nlocs < 12) { modes[nlocs] = mode; locs[nlocs++] = loc; } push(3, ACK_DELAY, 1, s); break;
    case 0x0E: mode = params[0]; push(3, ACK_DELAY, 1, s); break;
    case 0x06: case 0x1B:
        push(3, ACK_DELAY, 1, s); reading = 1; read_lba = loc; stat = 0x22;
        next_sector = now + ACK_DELAY + ((mode & 0x80) ? SECTOR_1X / 2 : SECTOR_1X); break;
    case 0x08: case 0x09:
        push(3, ACK_DELAY, 1, s); reading = 0; stat = 0x02; s[0] = stat; push(2, DONE_DELAY, 1, s); break;
    case 0x0A: push(3, ACK_DELAY, 1, s); reading = 0; mode = 0; stat = 0x02; s[0] = stat; push(2, DONE_DELAY, 1, s); break;
    case 0x15: case 0x16: push(3, ACK_DELAY, 1, s); push(2, DONE_DELAY, 1, s); break;
    case 0x0F: s[1] = mode; s[2] = 0; s[3] = 0; s[4] = 0; push(3, ACK_DELAY, 5, s); break;
    case 0x10: case 0x11: memset(s, 0, 8); push(3, ACK_DELAY, 8, s); break;
    case 0x13: s[1] = 1; s[2] = 1; push(3, ACK_DELAY, 3, s); break;
    case 0x14: {
        uint32_t end = (uint32_t)disc_sectors + 150;
        if (params[0] == 0) { s[1] = tobcd(end / 4500); s[2] = tobcd(end / 75 % 60); } else { s[1] = 0; s[2] = 2; }
        push(3, ACK_DELAY, 3, s); break;
    }
    case 0x19: { static const uint8_t v[4] = { 0x94, 0x09, 0x19, 0xC0 }; push(3, ACK_DELAY, 4, v); break; }
    case 0x1A: { static const uint8_t id[8] = { 0x02, 0x00, 0x20, 0x00, 'S', 'C', 'E', 'A' };
                 push(3, ACK_DELAY, 1, s); push(2, DONE_DELAY, 8, id); break; }
    case 0x1E: push(3, ACK_DELAY, 1, s); push(2, DONE_DELAY, 1, s); break;
    default: n_unknown++; s[0] = stat | 1; s[1] = 0x40; n = 2; push(5, ACK_DELAY, n, s);
    }
    nparams = 0;
}

void host_cd_tick(uint64_t cycles, uint32_t *i_stat) {
    now = cycles; g_istat = i_stat;
    if (reading && now >= next_sector && !nq && !iflag) {
        uint8_t s = stat;
        memset(sector, 0, sizeof sector);
        if (disc && _fseeki64(disc, (long long)read_lba * 2352, SEEK_SET) == 0) fread(sector, 1, 2352, disc);
        read_lba++; n_sectors++;
        next_sector = now + ((mode & 0x80) ? SECTOR_1X / 2 : SECTOR_1X);
        push(1, 0, 1, &s);
    }
    deliver();
}

static uint32_t cd_read(uint32_t reg);
uint32_t host_cd_read(uint32_t reg) {
    uint32_t v = cd_read(reg);
    if (g_cd_trace > 0) { g_cd_trace--; printf("[cd] r%u.%u -> %02X (if %X ie %X nq %d @%llu)\n", reg, idx, v, iflag, ie, nq, (unsigned long long)now); }
    return v;
}
static uint32_t cd_read(uint32_t reg) {
    switch (reg) {
    case 0: return idx | (nparams == 0 ? 8 : 0) | (nparams < 16 ? 0x10 : 0) | (rpos < nresp ? 0x20 : 0) | (fifo_pos < fifo_n ? 0x40 : 0);
    case 1: return rpos < nresp ? resp[rpos++] : 0;
    case 2: return fifo_pos < fifo_n ? fifo[fifo_pos++] : 0;
    default: return (idx & 1) ? (iflag | 0xE0u) : (ie | 0xE0u);
    }
}

void host_cd_write(uint32_t reg, uint8_t v, uint32_t *i_stat) {
    g_istat = i_stat;
    if (g_cd_trace > 0) { g_cd_trace--; printf("[cd] w%u.%u <- %02X (if %X ie %X nq %d @%llu)\n", reg, idx, v, iflag, ie, nq, (unsigned long long)now); }
    if (reg == 0) { idx = v & 3; return; }
    if (reg == 1 && idx == 0) { command(v); return; }
    if (reg == 2 && idx == 0) { if (nparams < 16) params[nparams++] = v; return; }
    if (reg == 2 && idx == 1) { ie = v & 0x1F; if ((iflag & ie) && g_istat) *g_istat |= 4; return; }
    if (reg == 3 && idx == 0) {
        if (v & 0x80) {   /* BFRD: sector buffer -> data FIFO; a repeat while the FIFO is loaded is a no-op (libcd sets it again between its header and data DMAs) */
            if (fifo_pos < fifo_n) return;
            uint32_t off = (mode & 0x20) ? 12 : 24, len = (mode & 0x20) ? 2340 : 2048;
            memcpy(fifo, sector + off, len); fifo_n = len; fifo_pos = 0;
        } else fifo_n = fifo_pos = 0;
        return;
    }
    if (reg == 3 && idx == 1) { iflag &= ~(v & 0x1F); if (v & 0x40) nparams = 0; deliver(); }
}

void host_cd_dma(uint8_t *ram, uint32_t addr, uint32_t words) {
    uint32_t i;
    if (g_cd_trace > 0) { g_cd_trace--; printf("[cd] dma3 %06X words %u fifo %u/%u\n", addr, words, fifo_pos, fifo_n); }
    for (i = 0; i < words * 4; i++) {
        ram[(addr + i) & 0x1FFFFF] = fifo_pos < fifo_n ? fifo[fifo_pos] : 0;
        if (fifo_pos < fifo_n) fifo_pos++;
    }
}

void host_cd_summary(void) {
    int i;
    printf("[cd] disc %s, sectors read %u, unknown cmds %u, commands:", disc ? "open" : "none", n_sectors, n_unknown);
    for (i = 0; i < 32; i++) if (n_cmd[i]) printf(" %02X=%u", i, n_cmd[i]);
    printf(" last lba %u\n[cd] first setloc lba(mode):", read_lba);
    for (i = 0; i < (int)nlocs; i++) printf(" %u(%02X)", locs[i], modes[i]);
    printf("\n");
}
