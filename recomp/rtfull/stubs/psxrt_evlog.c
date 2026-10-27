/* Opt-in (PSXRT_EVLOG=1; =2 also echoes to stderr; unset = fully off, no F12) event log for "what just happened" reports (battle 'burn' replay, 2026-10-27). Once per guest frame (pad
 * hook, stubs/psxrt_io.c) it appends compact text lines to an in-memory ring: CD commands (C), coalesced sector reads (R),
 * SPU key-on/off (K), per-frame interpreted-instruction and AOT-overlay dispatch deltas (F, only when nonzero), changes of
 * the resident overlay at the battle/field load addresses (O) and any backward step of the guest frame counter (B: a
 * savestate load/rewind). The ring holds the newest EVLOG_LINES lines (a few minutes) and is written to
 * <save root>\logs\evlog_<date>_<time>_<tag>.txt on F12 and at exit, followed by the top interpreted PCs (P). No paths,
 * names or card data go in. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <direct.h>
#include "cdrom.h"
#include "spu.h"
#include "dirty_ram_interp.h"

extern uint64_t g_dirty_ram_insns_run;
extern uint64_t s_frame_count;
uint32_t psx_overlay_resident_crc_at(uint32_t phys, int *valid);
const char *savestate_root_dir(void);
const char *savestate_dir(void);
#ifdef PSX_HAS_OVERLAY_DISPATCH
void psx_overlay_static_get_stats(uint64_t *, uint64_t *, uint64_t *, uint64_t *);
#endif

#define EVLOG_LINES 32768u   /* power of two */
#define EVLOG_W 160
static char s_ring[EVLOG_LINES][EVLOG_W];
static uint64_t s_n;
static int s_echo, s_on = -1;
static volatile int s_dump_req;

static void L(const char *fmt, ...) {
    char *d = s_ring[s_n++ & (EVLOG_LINES - 1u)];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(d, EVLOG_W, fmt, ap);
    va_end(ap);
    if (s_echo) fprintf(stderr, "[evlog] %s\n", d);
}

/* Overlay load addresses worth naming (analysis/overlay_map.py): battle main 1984/0 and 1984/1, per-scene battle handler,
 * field area module, battle program B base. */
static const uint32_t k_ov[] = { 0x000715d0u, 0x00081728u, 0x0008c1f4u, 0x0007e000u, 0x0002f810u };
#define NOV (sizeof k_ov / sizeof k_ov[0])

/* Sector reads are coalesced into runs of consecutive LBAs across frames; a run is written when it breaks, before the next
 * CD command line, and before a dump. */
static int s_run0 = -1, s_prev, s_cnt;
static uint32_t s_rf;
static void run_flush(void) {
    if (s_run0 >= 0) L("R %u %d..%d %d", s_rf, s_run0, s_prev, s_cnt);
    s_run0 = -1;
}

static int cmp_insns(const void *a, const void *b) {
    const DirtyRamPcEntry *x = *(DirtyRamPcEntry *const *)a, *y = *(DirtyRamPcEntry *const *)b;
    return x->insns < y->insns ? 1 : x->insns > y->insns ? -1 : 0;
}

static void dump(const char *tag) {
    run_flush();
    const char *root = savestate_root_dir();
    char dir[512], path[600];
    snprintf(dir, sizeof dir, "%s\\logs", root && *root ? root : "saves");
    _mkdir(root && *root ? root : "saves");
    _mkdir(dir);
    time_t t = time(NULL);
    struct tm tmv;
    localtime_s(&tmv, &t);
    snprintf(path, sizeof path, "%s\\evlog_%04d%02d%02d_%02d%02d%02d_%s.txt", dir, tmv.tm_year + 1900, tmv.tm_mon + 1,
             tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec, tag);
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "[evlog] cannot write %s\n", path); return; }
    fprintf(f, "# psxrt evlog: frame=%llu lines=%llu interp_total=%llu\n", (unsigned long long)s_frame_count,
            (unsigned long long)s_n, (unsigned long long)g_dirty_ram_insns_run);
    fprintf(f, "# C cmd params irq resp | R first_lba..last_lba n | K on/off voice@spu_addr pitch | F interp/static_hits/vmiss/amiss "
               "| O addr crc valid | B frame went back | P pc insns hits entry_hits occ_crc occ_ok last_ext_ra\n");
    for (uint64_t i = s_n > EVLOG_LINES ? s_n - EVLOG_LINES : 0; i < s_n; i++) fprintf(f, "%s\n", s_ring[i & (EVLOG_LINES - 1u)]);
    static DirtyRamPcEntry *top[DIRTY_RAM_PC_TABLE_SIZE];
    unsigned n = 0;
    for (unsigned i = 0; i < DIRTY_RAM_PC_TABLE_SIZE; i++)
        if (g_dirty_ram_pc_table[i].pc && g_dirty_ram_pc_table[i].insns) top[n++] = &g_dirty_ram_pc_table[i];
    qsort(top, n, sizeof top[0], cmp_insns);
    for (unsigned i = 0; i < n && i < 200; i++)
        fprintf(f, "P %08X %llu %llu %llu %08X %u %08X\n", top[i]->pc, (unsigned long long)top[i]->insns,
                (unsigned long long)top[i]->hits, (unsigned long long)top[i]->entry_hits, top[i]->occ_crc, top[i]->occ_ok,
                top[i]->last_ext_ra);
    fclose(f);
    fprintf(stderr, "[evlog] wrote %s\n", path);
}

int psxrt_evlog_enabled(void) { return s_on > 0; }
void psxrt_evlog_request_dump(void) { s_dump_req = 1; }   /* F12 (any thread); written on the next guest frame */
static void at_exit(void) { dump("exit"); }

void psxrt_evlog_frame(unsigned script_frame) {
    static uint64_t last_cmd, last_sec, last_spu, last_ins, last_fc, st[4];
    static uint32_t ov_crc[NOV];
    static int ov_valid[NOV];
    static int init;
    const uint32_t fc = (uint32_t)s_frame_count;
    if (s_on == 0) return;
    if (!init) {
        init = 1;
        const char *e = getenv("PSXRT_EVLOG");
        s_on = e && *e && *e != '0';
        s_echo = s_on && *e == '2';
        if (s_on) atexit(at_exit);
        last_ins = g_dirty_ram_insns_run;
        last_fc = s_frame_count;
    }
    if (s_frame_count < last_fc) L("B %u from %llu script=%u", fc, (unsigned long long)last_fc, script_frame);
    last_fc = s_frame_count;

    const CDROMCommandHistoryEntry *ch;
    const uint64_t nc = cdrom_debug_get_command_history(&ch);
    if (nc - last_cmd > CDROM_COMMAND_HISTORY_CAP) last_cmd = nc - CDROM_COMMAND_HISTORY_CAP;
    for (; last_cmd < nc; last_cmd++) {
        const CDROMCommandHistoryEntry *e = &ch[last_cmd % CDROM_COMMAND_HISTORY_CAP];
        if (e->kind != 'C' || e->cmd == 0x01) continue;   /* GetStat polls are noise */
        char p[40] = "", r[40] = "";
        for (int i = 0; i < e->param_count && i < 8; i++) snprintf(p + strlen(p), sizeof p - strlen(p), "%02X", e->params[i]);
        for (int i = 0; i < e->response_count && i < 4; i++) snprintf(r + strlen(r), sizeof r - strlen(r), "%02X", e->response[i]);
        run_flush();
        L("C %u %02X %s irq=%u resp=%s", e->frame, e->cmd, p[0] ? p : "-", e->irq_flag, r[0] ? r : "-");
    }

    const CDROMSectorHistoryEntry *sh;
    const uint64_t ns = cdrom_debug_get_sector_history(&sh);
    if (ns - last_sec > CDROM_SECTOR_HISTORY_CAP) last_sec = ns - CDROM_SECTOR_HISTORY_CAP;
    for (; last_sec < ns; last_sec++) {
        const CDROMSectorHistoryEntry *e = &sh[last_sec % CDROM_SECTOR_HISTORY_CAP];
        if (s_run0 >= 0 && e->lba == s_prev + 1) { s_prev = e->lba; s_cnt++; continue; }
        run_flush();
        s_run0 = s_prev = e->lba; s_cnt = 1; s_rf = e->frame;
    }

    static SpuEvent ev[512];
    const uint64_t tot = spu_event_total();
    uint64_t want = tot - last_spu;
    if (want > 512) want = 512;
    const uint32_t got = spu_event_get(ev, (uint32_t)want);
    last_spu = tot;
    char kl[EVLOG_W] = "";
    for (uint32_t i = 0; i < got; i++) {
        if (ev[i].kind != SPU_EV_KEYON && ev[i].kind != SPU_EV_KEYOFF) continue;
        char one[32];
        snprintf(one, sizeof one, " %c%u@%05X/%04X", ev[i].kind == SPU_EV_KEYON ? '+' : '-', ev[i].voice, ev[i].addr, ev[i].pitch);
        if (strlen(kl) + strlen(one) >= EVLOG_W - 16) { L("K %u%s", fc, kl); kl[0] = 0; }
        strcat(kl, one);
    }
    if (kl[0]) L("K %u%s", fc, kl);

    uint64_t s[4] = { 0, 0, 0, 0 };
#ifdef PSX_HAS_OVERLAY_DISPATCH
    psx_overlay_static_get_stats(&s[0], &s[1], &s[2], &s[3]);
#endif
    const uint64_t di = g_dirty_ram_insns_run - last_ins;
    if (di || s[2] != st[2] || s[3] != st[3])
        L("F %u interp=%llu hits=%llu vmiss=%llu amiss=%llu", fc, (unsigned long long)di, (unsigned long long)(s[1] - st[1]),
          (unsigned long long)(s[2] - st[2]), (unsigned long long)(s[3] - st[3]));
    last_ins = g_dirty_ram_insns_run;
    memcpy(st, s, sizeof st);

    for (unsigned i = 0; i < NOV; i++) {
        int v = 0;
        const uint32_t c = psx_overlay_resident_crc_at(k_ov[i], &v);
        if (c != ov_crc[i] || v != ov_valid[i]) L("O %u %08X %08X %d", fc, 0x80000000u | k_ov[i], c, v);
        ov_crc[i] = c;
        ov_valid[i] = v;
    }
    if (s_dump_req) { s_dump_req = 0; dump("f12"); }
}
