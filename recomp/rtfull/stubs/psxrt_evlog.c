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
#include "host_span_ring.h"
#include "audio_trace.h"

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
    /* Audio pipeline gaps in the newest 32768 audio events: A = audio-trace totals (pump underruns = host queue empty, lowest queue
     * bytes), U/X/Y = host underrun / zero-run in decoded XA PCM / pump backpressure skip (frame, lba or queued bytes, run info). */
    static AudioTraceEvent aev[32768];
    AudioTraceStats as;
    audio_trace_get_stats(&as);
    fprintf(f, "A pump_calls=%llu underruns=%llu skips=%llu queue_lo=%u queue_hi=%u events=%llu\n", (unsigned long long)as.pump_calls,
            (unsigned long long)as.underruns, (unsigned long long)as.pump_skips, as.queue_lowater, as.queue_hiwater,
            (unsigned long long)as.events_total);
    const uint32_t na = audio_trace_events_get(aev, 32768);
    unsigned printed = 0;
    for (uint32_t i = 0; i < na && printed < 300; i++) {
        const char c = aev[i].kind == AUDIO_EV_UNDERRUN ? 'U' : aev[i].kind == AUDIO_EV_XA_ZERO ? 'X' : aev[i].kind == AUDIO_EV_PUMP_SKIP ? 'Y' : 0;
        if (c) { fprintf(f, "%c %u a=%u b=%08X\n", c, aev[i].frame, aev[i].a, aev[i].b); printed++; }
    }
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
uint32_t psx_overlay_resident_crc_at(uint32_t phys, int *valid);
uint32_t psxrt_occ_crc(uint32_t phys, int *valid) {   /* the runtime scans 16384 cache entries per call, see CMakeLists.txt */
    static int occ = -1;
    if (occ < 0) { const char *e = getenv("PSXRT_OCC"); occ = e && *e && *e != '0'; }   /* debug-server per_pc occ_crc/occ_ok */
    if (occ) return psx_overlay_resident_crc_at(phys, valid);
    *valid = 0;
    return 0;
}
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
    /* Wall time per guest frame (host_span_ring.h stamps): T = wall ms of each 30-frame block (16.7 ms/frame = 500 per 30), W = one
     * frame that took > 25 ms (wall us, interp insns, page faults, disk KiB), S = emu-thread spans >= 1 ms (disc read, loader...). */
    static uint64_t last_st, last_sp, blk_t;
    static uint32_t blk_f;
    const uint64_t stt = host_frame_stamp_total();
    if (stt - last_st > HOST_FRAME_STAMP_CAP) last_st = stt - HOST_FRAME_STAMP_CAP;
    for (; last_st < stt; last_st++) {
        uint32_t f, ii, sc, pf, io;
        uint64_t t;
        if (!host_frame_stamp_get(last_st, &f, &t, &ii, &sc, &pf, &io)) continue;
        static uint64_t pt; static uint32_t ppf, pio;
        if (pt && t - pt > 25000u) L("W %u %llu interp=%u pf=%u io_kb=%u", f, (unsigned long long)(t - pt), ii, pf - ppf, io - pio);
        pt = t; ppf = pf; pio = io;
        if (!blk_t) { blk_t = t; blk_f = f; }
        if (f - blk_f >= 30u) { L("T %u %llu", f, (unsigned long long)((t - blk_t) / 1000u)); blk_t = t; blk_f = f; }
    }
    const uint64_t spt = host_span_total();
    if (spt - last_sp > HOST_SPAN_RING_CAP) last_sp = spt - HOST_SPAN_RING_CAP;
    for (; last_sp < spt; last_sp++) {
        HostSpan sp;
        if (host_span_get(last_sp, &sp) && sp.dur_us >= 1000u)
            L("S %u %s %u us arg=%u emu=%u", sp.frame, host_span_kind_name(sp.kind), sp.dur_us, sp.arg, sp.emu);
    }
    if (s_dump_req) { s_dump_req = 0; dump("f12"); }
}
