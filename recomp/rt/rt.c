/* Minimal host runtime for psxrecomp-generated code (boot exe SCUS_944.21).
 *
 * Our own code: no network, no process spawning, no dynamic loading, no file
 * writes. Reads two files given on the command line (BIOS image, PS-X EXE).
 * Provides the 15 link symbols the generated boot exe needs, a PS1 memory map,
 * a small MIPS R3000A interpreter for code that is not compiled (BIOS ROM,
 * kernel RAM, overlays), hardware stubs, and a CPS driver loop.
 * Video/audio/input are headless stubs (see host.h); RECOMP_NOTES.md has the audit.
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "psx_runtime.h"
#include "host.h"

/* ---- link symbols the generated code needs ------------------------------- */
int g_psx_cps_mode;
uint32_t g_debug_last_store_pc;
void debug_server_cyc_observe(uint32_t pc) { (void)pc; }
void debug_server_log_call_entry(uint32_t pc) { (void)pc; }
void psx_restore_state_escape(void) { }
int psx_vsync_query_hle_try(CPUState *c, uint32_t a) { (void)c; (void)a; return 0; }
/* shortcut: code is never modified at run time here (no overlay over the boot exe); add a dirty-page check if it is */
int dirty_ram_text_native_ok_ranges(const uint32_t *p, uint32_t n) { (void)p; (void)n; return 1; }
int dirty_ram_text_native_ok_ranges_from(const uint32_t *p, uint32_t n, uint32_t pc) { (void)p; (void)n; (void)pc; return 1; }
extern int psx_dispatch_game_compiled(CPUState *cpu, uint32_t addr);

/* ---- state ----------------------------------------------------------------- */
static uint8_t ram[0x200000], scratch[0x400], bios[0x80000], io[0x2000];
static CPUState cpu_s, *C = &cpu_s;
static jmp_buf g_top;
static int exe_loaded;
static const char *exe_path;
static uint64_t cycles, next_vblank = 564480, n_vblank, n_irq, n_dispatch, n_interp, n_checks;
static uint64_t n_gp0, n_gp1, n_dma[7], n_syscall_hle, n_exc[32];
static uint32_t cachectl;
static uint64_t timer_base[3], timer_fire[3]; /* timer_fire 0 = no IRQ armed */
static uint64_t n_timer_irq[3];
static uint32_t bios_calls[3][256];
static uint32_t ring[32]; static int ring_i;
static uint32_t page_hits[0x800]; /* interpreted pcs per 1 KB of RAM/ROM window */
static uint64_t max_vblanks = 600; static double max_seconds = 20; static clock_t t0;
static int verbose;
static uint32_t trace_pc[8]; static int n_trace_pc, trace_left = 40; /* --trace-pc: print regs when the interpreter reaches it */
static const char *dump_dir;  /* --dump: RAM image + VRAM PPMs written at stop (gitignored _out dir) */
static uint64_t frame_every;  /* --frames N: display-area PPM every N vblanks into <dump>/frames */
extern int g_cd_trace;
static uint32_t unk_io[64][2]; static int n_unk_io;

#define I_STAT (*(uint32_t *)(io + 0x070))
#define I_MASK (*(uint32_t *)(io + 0x074))
#define SR C->cop0[12]
#define CAUSE C->cop0[13]
#define EPC C->cop0[14]

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void summary(const char *why);

static void note_unk_io(uint32_t p, int w) {
    int i;
    for (i = 0; i < n_unk_io; i++) if (unk_io[i][0] == p && unk_io[i][1] == (uint32_t)w) return;
    if (n_unk_io < 64) { unk_io[n_unk_io][0] = p; unk_io[n_unk_io][1] = w; n_unk_io++; }
}

/* ---- DMA --------------------------------------------------------------------- */
static void dma_run(int ch) {
    uint32_t *r = (uint32_t *)(io + 0x080 + ch * 16); /* MADR, BCR, CHCR */
    uint32_t addr = r[0] & 0x1FFFFC, chcr = r[2], i, n;
    uint32_t *dicr = (uint32_t *)(io + 0x0F4);
    n_dma[ch]++;
    if (ch == 2 && ((chcr >> 9) & 3) == 2) {          /* GPU linked list: feed each packet to GP0 */
        for (i = 0; i < 0x100000; i++) {
            uint32_t h = rd32(ram + addr), k;
            for (k = 1; k <= h >> 24; k++) gpu_gp0(rd32(ram + ((addr + 4 * k) & 0x1FFFFC)));
            n_gp0 += h >> 24;
            if ((h & 0x800000) || (h & 0xFFFFFF) == 0xFFFFFF) break;
            addr = h & 0x1FFFFC;
        }
    } else if (ch == 2) {                             /* block: chcr bit0 1 = RAM->GPU, 0 = GPU->RAM */
        n = (r[1] & 0xFFFF) * (((chcr >> 9) & 3) ? (r[1] >> 16) : 1);
        if (!n) n = 0x10000;
        for (i = 0; i < n; i++, addr = (addr + 4) & 0x1FFFFC) {
            if (chcr & 1) gpu_gp0(rd32(ram + addr)); else { uint32_t v = gpu_read(); memcpy(ram + addr, &v, 4); }
        }
        n_gp0 += n;
    } else if (ch == 6) {                             /* OTC: reverse ordering table */
        n = r[1] & 0xFFFF; if (!n) n = 0x10000;
        for (i = 0; i < n; i++) {
            uint32_t v = (i == n - 1) ? 0x00FFFFFF : ((addr - 4) & 0x1FFFFF);
            memcpy(ram + addr, &v, 4); addr = (addr - 4) & 0x1FFFFC;
        }
    } else if (ch == 3) {
        host_cd_dma(ram, addr, (r[1] & 0xFFFF) * ((r[1] >> 16) ? (r[1] >> 16) : 1));
    } /* ch 0/1 MDEC, 4 SPU, 5 PIO: completed without data (headless) */
    r[2] &= ~((1u << 24) | (1u << 28));
    if (*dicr & (1u << (16 + ch))) *dicr |= 1u << (24 + ch);
    if ((*dicr & (1u << 23)) && ((*dicr >> 24) & (*dicr >> 16) & 0x7F)) {
        if (!(*dicr & 0x80000000u)) I_STAT |= 8;
        *dicr |= 0x80000000u;
    }
}

/* ---- I/O ------------------------------------------------------------------------ */
static uint32_t timer_div(int n) {
    uint32_t mode = *(uint32_t *)(io + 0x104 + n * 16);
    if (n == 1 && (mode & 0x100)) return 2146;        /* hblank */
    if (n == 2 && (mode & 0x200)) return 8;
    return 1;
}

/* Arm the next IRQ of root counter n from its mode (bit4 at target, bit5 at 0xFFFF, bit3 reset at target). */
static void timer_arm(int n) {
    uint32_t mode = *(uint32_t *)(io + 0x104 + n * 16), tgt = *(uint32_t *)(io + 0x108 + n * 16) & 0xFFFF;
    uint64_t per = (uint64_t)(((mode & 0x10) ? tgt : 0xFFFF) + 1u) * timer_div(n), done;
    timer_fire[n] = 0;
    if (!(mode & 0x30) || !per) return;
    done = (cycles - timer_base[n]) / per;
    timer_fire[n] = timer_base[n] + (done + 1) * per;
}

static uint32_t io_read(uint32_t p, int w) {
    uint32_t off = p - 0x1F801000, v = 0;
    if (off >= 0x800 && off < 0x804) return host_cd_read(off - 0x800);
    if (off >= 0x100 && off < 0x130 && (off & 15) == 0) {
        int n = off >> 4 & 3;
        uint64_t c = (cycles - timer_base[n]) / timer_div(n);
        uint32_t mode = *(uint32_t *)(io + 0x104 + n * 16), tgt = *(uint32_t *)(io + 0x108 + n * 16) & 0xFFFF;
        return (uint32_t)((mode & 8) ? c % (tgt + 1u) : c) & 0xFFFF;
    }
    switch (off) {
    case 0x040: return 0xFF;                          /* SIO RX: no controller */
    case 0x044: return 0x7;                           /* TX ready, RX not empty */
    case 0x810: return gpu_read();
    case 0x814: return gpu_stat((int)(n_vblank & 1));
    case 0xDAE: return *(uint16_t *)(io + 0xDAA) & 0x3F; /* SPUSTAT mirrors SPUCNT mode */
    }
    if (!((off >= 0x070 && off < 0x078) || (off >= 0x080 && off < 0x130) || off >= 0xC00 || off < 0x024))
        note_unk_io(p, 0);
    memcpy(&v, io + off, w);
    return v;
}

static void io_write(uint32_t p, uint32_t v, int w) {
    uint32_t off = p - 0x1F801000;
    if (off >= 0x800 && off < 0x804) { host_cd_write(off - 0x800, (uint8_t)v, &I_STAT); return; }
    if (off == 0x070) { I_STAT &= v; return; }
    if (off == 0x810) { n_gp0++; gpu_gp0(v); return; }
    if (off == 0x814) { n_gp1++; gpu_gp1(v); return; }
    if (off == 0x0F4) {
        uint32_t *d = (uint32_t *)(io + 0x0F4);
        *d = (v & 0x00FFFFFF) | (*d & 0x7F000000 & ~(v & 0x7F000000));
        if ((*d & 0x8000) || ((*d & (1u << 23)) && ((*d >> 24) & (*d >> 16) & 0x7F))) *d |= 0x80000000u;
        else *d &= 0x7FFFFFFF;
        return;
    }
    if (off >= 0x100 && off < 0x130) {
        int n = off >> 4 & 3;
        memcpy(io + off, &v, w);
        if ((off & 15) == 4) timer_base[n] = cycles;
        timer_arm(n);
        return;
    }
    memcpy(io + off, &v, w);
    if (off >= 0x080 && off < 0x0F0 && (off & 15) == 8 && w == 4 && (v & (1u << 24))) dma_run(off >> 4 & 7);
}

/* ---- memory map ------------------------------------------------------------------ */
static uint8_t *mem_ptr(uint32_t a, uint32_t w) {
    uint32_t p = a & 0x1FFFFFFF;
    if (p < 0x800000) return ram + (p & 0x1FFFFF & ~(w - 1));
    if (p >= 0x1F800000 && p < 0x1F800400) return scratch + (p & 0x3FF & ~(w - 1));
    if (p >= 0x1FC00000 && p < 0x1FC80000) return bios + (p & 0x7FFFF & ~(w - 1));
    return 0;
}
static int is_io(uint32_t a) { uint32_t p = a & 0x1FFFFFFF; return a < 0xFFFE0000u && p >= 0x1F801000 && p < 0x1F803000; }

uint32_t psx_read_word(uint32_t a) {
    uint8_t *m = mem_ptr(a, 4);
    if (a >= 0xFFFE0000u) return cachectl;
    if (m) return rd32(m);
    if (is_io(a)) return io_read(a & 0x1FFFFFFC, 4);
    return 0xFFFFFFFFu;                                /* expansion / open bus */
}
uint16_t psx_read_half(uint32_t a) {
    uint8_t *m = mem_ptr(a, 2); uint16_t v;
    if (m) { memcpy(&v, m, 2); return v; }
    if (is_io(a)) return (uint16_t)io_read(a & 0x1FFFFFFE, 2);
    return 0xFFFF;
}
static uint8_t read_byte(uint32_t a) {
    uint8_t *m = mem_ptr(a, 1);
    if (m) return *m;
    if (is_io(a)) return (uint8_t)io_read(a & 0x1FFFFFFF, 1);
    return 0xFF;
}
uint8_t psx_cyc_load_byte(CPUState *c, uint32_t a, uint32_t rt, uint32_t mask) { (void)c; (void)rt; (void)mask; return read_byte(a); }

static uint32_t interp_pc; static int n_vec_writes;
static uint32_t watch_addr; static int watch_left = 60; /* --watch: print stores to this word (physical) */
static void write_n(uint32_t a, uint32_t v, uint32_t w) {
    uint8_t *m;
    if (exe_loaded && (a & 0x1FFFFFFF) >= 0x80 && (a & 0x1FFFFFFF) < 0x90 && !(SR & 0x10000) && n_vec_writes++ < 8)
        printf("[rt] exception-vector write %08X=%08X by interp pc %08X / compiled store pc %08X\n", a, v, interp_pc, g_debug_last_store_pc);
    if (watch_addr && (a & 0x1FFFFFFC) == watch_addr && watch_left > 0 && !(SR & 0x10000)) { watch_left--;
        printf("[watch] %08X=%08X (w%u) by interp pc %08X / compiled store pc %08X ra %08X vbl %llu\n", a, v, w, interp_pc, g_debug_last_store_pc, C->gpr[31], (unsigned long long)n_vblank); }
    if (SR & 0x10000) return;                          /* cache isolated: stores hit the cache only */
    if (a >= 0xFFFE0000u) { cachectl = v; return; }
    m = mem_ptr(a, w);
    if (m && (m < bios || m >= bios + sizeof bios)) { memcpy(m, &v, w); return; }
    if (is_io(a)) io_write(a & 0x1FFFFFFF & ~(w - 1), v, (int)w);
}
static void write_word(uint32_t a, uint32_t v) { write_n(a, v, 4); }
static void write_half(uint32_t a, uint16_t v) { write_n(a, v, 2); }
static void write_byte(uint32_t a, uint8_t v) { write_n(a, v, 1); }

/* ---- exceptions / interrupts ------------------------------------------------------ */
static void exception(uint32_t code, uint32_t epc, int bd) {
    n_exc[code & 31]++;
    CAUSE = (CAUSE & ~0x8000007Cu) | (code << 2) | (bd ? 0x80000000u : 0);
    EPC = epc;
    SR = (SR & ~0x3Fu) | ((SR << 2) & 0x3Fu);
    C->pc = (SR & 0x400000) ? 0xBFC00180u : 0x80000080u;
}

static void tick(uint32_t n) {
    cycles += n;
    host_cd_tick(cycles, &I_STAT);
    {   int k;  /* shortcut: assumes repeat mode (bit6) and reset-at-target when IRQ-at-target; one-shot timers keep firing */
        for (k = 0; k < 3; k++) if (timer_fire[k] && cycles >= timer_fire[k]) {
            I_STAT |= 0x10u << k; n_timer_irq[k]++;
            timer_base[k] = (*(uint32_t *)(io + 0x104 + k * 16) & 8) ? timer_fire[k] : timer_base[k];
            timer_arm(k);
        }
    }
    if (cycles >= next_vblank) {
        next_vblank += 564480; n_vblank++; I_STAT |= 1;
        host_frame(n_vblank);
        if (dump_dir && frame_every && n_vblank % frame_every == 0) { char p[512]; snprintf(p, sizeof p, "%s/frames/f%05llu.ppm", dump_dir, (unsigned long long)n_vblank); gpu_dump_display(p); }
        if (n_vblank >= max_vblanks) summary("vblank limit");
        if ((n_vblank & 15) == 0 && (double)(clock() - t0) / CLOCKS_PER_SEC > max_seconds) summary("time limit");
    }
}

static int irq_ready(void) {
    if (I_STAT & I_MASK) CAUSE |= 0x400; else CAUSE &= ~0x400u;
    return (SR & 1) && (SR & CAUSE & 0x400);
}

void psx_check_interrupts_at(CPUState *c, uint32_t resume) {
    (void)c;
    n_checks++;
    /* shortcut: flat ~15 cycles per compiled block (not cycle-exact; use psxrecomp block cycles if timing matters).
     * At 8, libetc v_wait's 2-block loop (0x8001FFF0) spun its 32768-iteration timeout in < 1 frame: 'VSync: timeout'. */
    tick(15);
    if ((n_checks & 0xFFFFF) == 0 && (double)(clock() - t0) / CLOCKS_PER_SEC > max_seconds) summary("time limit");
    if (irq_ready()) { n_irq++; exception(0, resume, 0); longjmp(g_top, 1); }
}
void psx_check_interrupts_dispatch_entry(CPUState *c, uint32_t resume) { psx_check_interrupts_at(c, resume); }

/* Compiled code has no syscall PC, so the kernel's syscall service is done here.
 * shortcut: HLE of the 4 syscall functions; route to the BIOS handler if a game needs ChangeThreadSubFunction */
int psx_syscall(CPUState *c, uint32_t code) {
    (void)code;
    n_syscall_hle++;
    switch (c->gpr[4]) {
    case 0: break;
    case 1: c->gpr[2] = (SR & 0x401) == 0x401; SR &= ~0x401u; break;   /* EnterCriticalSection */
    case 2: SR |= 0x401; break;                                       /* ExitCriticalSection */
    default: fprintf(stderr, "syscall a0=%u from compiled code: not handled\n", c->gpr[4]); summary("syscall");
    }
    return 0;
}

/* ---- interpreter ------------------------------------------------------------------- */
enum { NEXT, TAKEN, NOT_TAKEN, EXC };
#define R(x) C->gpr[x]
static int exec(uint32_t in, uint32_t pc, uint32_t *tgt) {
    uint32_t op = in >> 26, rs = in >> 21 & 31, rt = in >> 16 & 31, rd = in >> 11 & 31, sh = in >> 6 & 31;
    uint32_t imm = (uint32_t)(int32_t)(int16_t)in, uimm = in & 0xFFFF, ea = R(rs) + imm, t;
    int r = NEXT;
    switch (op) {
    case 0:
        switch (in & 63) {
        case 0x00: t = R(rt) << sh; if (rd) R(rd) = t; break;
        case 0x02: t = R(rt) >> sh; if (rd) R(rd) = t; break;
        case 0x03: t = (uint32_t)((int32_t)R(rt) >> sh); if (rd) R(rd) = t; break;
        case 0x04: t = R(rt) << (R(rs) & 31); if (rd) R(rd) = t; break;
        case 0x06: t = R(rt) >> (R(rs) & 31); if (rd) R(rd) = t; break;
        case 0x07: t = (uint32_t)((int32_t)R(rt) >> (R(rs) & 31)); if (rd) R(rd) = t; break;
        case 0x08: *tgt = R(rs); return TAKEN;
        case 0x09: *tgt = R(rs); if (rd) R(rd) = pc + 8; return TAKEN;
        case 0x0C: exception(8, pc, 0); return EXC;
        case 0x0D: exception(9, pc, 0); return EXC;
        case 0x10: if (rd) R(rd) = C->hi; break;
        case 0x11: C->hi = R(rs); break;
        case 0x12: if (rd) R(rd) = C->lo; break;
        case 0x13: C->lo = R(rs); break;
        case 0x18: { int64_t m = (int64_t)(int32_t)R(rs) * (int32_t)R(rt); C->lo = (uint32_t)m; C->hi = (uint32_t)(m >> 32); break; }
        case 0x19: { uint64_t m = (uint64_t)R(rs) * R(rt); C->lo = (uint32_t)m; C->hi = (uint32_t)(m >> 32); break; }
        case 0x1A: {
            int32_t n = (int32_t)R(rs), d = (int32_t)R(rt);
            if (!d) { C->hi = (uint32_t)n; C->lo = n >= 0 ? 0xFFFFFFFFu : 1; }
            else if (n == INT32_MIN && d == -1) { C->hi = 0; C->lo = 0x80000000u; }
            else { C->lo = (uint32_t)(n / d); C->hi = (uint32_t)(n % d); }
            break;
        }
        case 0x1B:
            if (!R(rt)) { C->hi = R(rs); C->lo = 0xFFFFFFFFu; }
            else { C->lo = R(rs) / R(rt); C->hi = R(rs) % R(rt); }
            break;
        /* shortcut: add/addi/sub never raise the overflow exception */
        case 0x20: case 0x21: t = R(rs) + R(rt); if (rd) R(rd) = t; break;
        case 0x22: case 0x23: t = R(rs) - R(rt); if (rd) R(rd) = t; break;
        case 0x24: t = R(rs) & R(rt); if (rd) R(rd) = t; break;
        case 0x25: t = R(rs) | R(rt); if (rd) R(rd) = t; break;
        case 0x26: t = R(rs) ^ R(rt); if (rd) R(rd) = t; break;
        case 0x27: t = ~(R(rs) | R(rt)); if (rd) R(rd) = t; break;
        case 0x2A: t = (int32_t)R(rs) < (int32_t)R(rt); if (rd) R(rd) = t; break;
        case 0x2B: t = R(rs) < R(rt); if (rd) R(rd) = t; break;
        default: exception(10, pc, 0); return EXC;
        }
        break;
    case 1: {
        int take = (rt & 1) ? (int32_t)R(rs) >= 0 : (int32_t)R(rs) < 0;
        if ((rt & 0x1E) == 0x10) R(31) = pc + 8;
        *tgt = pc + 4 + (imm << 2); return take ? TAKEN : NOT_TAKEN;
    }
    case 2: *tgt = (pc & 0xF0000000u) | ((in & 0x3FFFFFF) << 2); return TAKEN;
    case 3: R(31) = pc + 8; *tgt = (pc & 0xF0000000u) | ((in & 0x3FFFFFF) << 2); return TAKEN;
    case 4: *tgt = pc + 4 + (imm << 2); return R(rs) == R(rt) ? TAKEN : NOT_TAKEN;
    case 5: *tgt = pc + 4 + (imm << 2); return R(rs) != R(rt) ? TAKEN : NOT_TAKEN;
    case 6: *tgt = pc + 4 + (imm << 2); return (int32_t)R(rs) <= 0 ? TAKEN : NOT_TAKEN;
    case 7: *tgt = pc + 4 + (imm << 2); return (int32_t)R(rs) > 0 ? TAKEN : NOT_TAKEN;
    case 8: case 9: if (rt) R(rt) = R(rs) + imm; break;
    case 10: if (rt) R(rt) = (int32_t)R(rs) < (int32_t)imm; break;
    case 11: if (rt) R(rt) = R(rs) < imm; break;
    case 12: if (rt) R(rt) = R(rs) & uimm; break;
    case 13: if (rt) R(rt) = R(rs) | uimm; break;
    case 14: if (rt) R(rt) = R(rs) ^ uimm; break;
    case 15: if (rt) R(rt) = uimm << 16; break;
    case 16:
        if (rs == 0) { if (rt) R(rt) = C->cop0[rd]; }
        else if (rs == 4) { C->cop0[rd] = R(rt); }
        else if (rs == 16 && (in & 63) == 16) SR = (SR & ~0xFu) | ((SR >> 2) & 0xFu);
        else { exception(10, pc, 0); return EXC; }
        break;
    /* shortcut: no load-delay slot emulation; loads land immediately */
    case 0x20: t = (uint32_t)(int32_t)(int8_t)read_byte(ea); if (rt) R(rt) = t; break;
    case 0x21: t = (uint32_t)(int32_t)(int16_t)psx_read_half(ea); if (rt) R(rt) = t; break;
    case 0x22: { uint32_t w = psx_read_word(ea & ~3u), s = (3 - (ea & 3)) * 8;
                 t = s ? ((R(rt) & ((1u << s) - 1)) | (w << s)) : w; if (rt) R(rt) = t; break; }
    case 0x23: t = psx_read_word(ea); if (rt) R(rt) = t; break;
    case 0x24: t = read_byte(ea); if (rt) R(rt) = t; break;
    case 0x25: t = psx_read_half(ea); if (rt) R(rt) = t; break;
    case 0x26: { uint32_t w = psx_read_word(ea & ~3u), s = (ea & 3) * 8;
                 t = s ? ((R(rt) & ~(0xFFFFFFFFu >> s)) | (w >> s)) : w; if (rt) R(rt) = t; break; }
    case 0x28: write_byte(ea, (uint8_t)R(rt)); break;
    case 0x29: write_half(ea, (uint16_t)R(rt)); break;
    case 0x2A: { uint32_t a = ea & ~3u, w = psx_read_word(a), s = (3 - (ea & 3)) * 8;
                 w = s ? ((w & ~(0xFFFFFFFFu >> s)) | (R(rt) >> s)) : R(rt); write_word(a, w); break; }
    case 0x2B: write_word(ea, R(rt)); break;
    case 0x2E: { uint32_t a = ea & ~3u, w = psx_read_word(a), s = (ea & 3) * 8;
                 w = s ? ((w & ((1u << s) - 1)) | (R(rt) << s)) : R(rt); write_word(a, w); break; }
    default:   /* COP2 (GTE) and the rest: not needed by the BIOS; report */
        fprintf(stderr, "interp: unhandled opcode %08X at %08X\n", in, pc); summary("unhandled opcode");
    }
    return r;
}

static void interp_block(void) {
    int n;
    for (n = 0; n < 1024; n++) {
        uint32_t pc = interp_pc = C->pc, in = psx_read_word(pc), tgt = 0, t2;
        int r;
        n_interp++; tick(2);
        if ((pc & 0x1FFFFFFF) < 0x200000) page_hits[(pc & 0x1FFFFF) >> 10]++;
        else if ((pc & 0x1FFFFFFF) >= 0x1FC00000) page_hits[0x7FF]++;
        if (n_trace_pc && trace_left > 0) { int k; for (k = 0; k < n_trace_pc; k++) if (trace_pc[k] == pc) { trace_left--;
            printf("[trace] pc %08X v0 %08X a0 %08X a1 %08X a2 %08X a3 %08X s0 %08X s1 %08X s2 %08X ra %08X vbl %llu\n", pc, R(2), R(4), R(5), R(6), R(7), R(16), R(17), R(18), R(31), (unsigned long long)n_vblank); } }
        r = exec(in, pc, &tgt);
        if (r == NEXT) { C->pc = pc + 4; continue; }
        if (r == EXC) return;
        n_interp++; tick(2);
        if (exec(psx_read_word(pc + 4), pc + 4, &t2) == EXC) { EPC = pc; CAUSE |= 0x80000000u; return; }
        C->pc = r == TAKEN ? tgt : pc + 8;
        return;
    }
}

/* ---- boot --------------------------------------------------------------------------- */
static long read_file(const char *path, uint8_t *dst, long max) {
    FILE *f = fopen(path, "rb"); long n;
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
    n = (long)fread(dst, 1, (size_t)max, f); fclose(f);
    return n;
}

static void load_exe(void) {
    static uint8_t buf[0x200000];
    long n = read_file(exe_path, buf, sizeof buf);
    uint32_t pc0 = rd32(buf + 0x10), gp = rd32(buf + 0x14), ta = rd32(buf + 0x18), ts = rd32(buf + 0x1C);
    uint32_t sp = rd32(buf + 0x30) + rd32(buf + 0x34);
    if (n < 0x800 || memcmp(buf, "PS-X EXE", 8) || ts > (uint32_t)(n - 0x800) || (ta & 0x1FFFFF) + ts > sizeof ram) {
        fprintf(stderr, "bad PS-X EXE %s\n", exe_path); exit(1);
    }
    memcpy(ram + (ta & 0x1FFFFF), buf + 0x800, ts);
    C->pc = pc0; R(28) = gp; if (sp) { R(29) = sp; R(30) = sp; }
    exe_loaded = 1;
    if (verbose >= 2) g_cd_trace = 4000;
    printf("[rt] exe loaded at BIOS shell hook: %08X+%X entry %08X sp %08X (cycle %llu, vblank %llu)\n",
           ta, ts, pc0, sp, (unsigned long long)cycles, (unsigned long long)n_vblank);
}

static const char *bname[3] = { "A0", "B0", "C0" };
static void summary(const char *why) {
    int i, j;
    printf("\n[rt] stop: %s\n", why);
    printf("[rt] pc %08X ra %08X sp %08X v0 %08X a0 %08X SR %08X CAUSE %08X EPC %08X I_STAT %X I_MASK %X\n",
           C->pc, R(31), R(29), R(2), R(4), SR, CAUSE, EPC, I_STAT, I_MASK);
    printf("[rt] RAM[80..8C] %08X %08X %08X %08X k0 %08X k1 %08X\n", rd32(ram + 0x80), rd32(ram + 0x84), rd32(ram + 0x88), rd32(ram + 0x8C), R(26), R(27));
    printf("[rt] cycles %llu vblanks %llu irqs %llu compiled-dispatches %llu interp-insns %llu syscall-hle %llu\n",
           (unsigned long long)cycles, (unsigned long long)n_vblank, (unsigned long long)n_irq,
           (unsigned long long)n_dispatch, (unsigned long long)n_interp, (unsigned long long)n_syscall_hle);
    printf("[rt] exceptions:"); for (i = 0; i < 32; i++) if (n_exc[i]) printf(" code%d=%llu", i, (unsigned long long)n_exc[i]);
    printf(" timer-irqs %llu/%llu/%llu", (unsigned long long)n_timer_irq[0], (unsigned long long)n_timer_irq[1], (unsigned long long)n_timer_irq[2]);
    printf("\n[rt] gp0 words %llu gp1 %llu gpu cmds %llu dma:", (unsigned long long)n_gp0, (unsigned long long)n_gp1, (unsigned long long)g_gpu_prims);
    for (i = 0; i < 7; i++) printf(" ch%d=%llu", i, (unsigned long long)n_dma[i]);
    printf("\n[rt] bios calls after exe load (table.fn=count):");
    for (i = 0; i < 3; i++) for (j = 0; j < 256; j++) if (bios_calls[i][j]) printf(" %s.%02X=%u", bname[i], j, bios_calls[i][j]);
    printf("\n[rt] last driver pcs:"); for (i = 0; i < 32; i++) printf(" %08X", ring[(ring_i + i) & 31]);
    printf("\n[rt] hottest interpreted 1KB pages:");
    for (j = 0; j < 6; j++) { int b = 0; for (i = 1; i < 0x800; i++) if (page_hits[i] > page_hits[b]) b = i;
        if (!page_hits[b]) break; printf(" %s%05X=%u", b == 0x7FF ? "ROM" : "RAM+", b == 0x7FF ? 0 : b << 10, page_hits[b]); page_hits[b] = 0; }
    printf("\n[rt] unmodelled I/O (r=0 w=1 timer-irq=2):"); for (i = 0; i < n_unk_io; i++) printf(" %08X/%u", unk_io[i][0], unk_io[i][1]);
    printf("\n");
    host_cd_summary();
    if (dump_dir) { char p[512]; FILE *f; snprintf(p, sizeof p, "%s/ram.bin", dump_dir);
        if ((f = fopen(p, "wb"))) { fwrite(ram, 1, sizeof ram, f); fclose(f); }
        snprintf(p, sizeof p, "%s/vram.ppm", dump_dir); gpu_dump_vram(p);
        snprintf(p, sizeof p, "%s/display.ppm", dump_dir); gpu_dump_display(p); }
    fflush(stdout);
    exit(0);
}

int main(int argc, char **argv) {
    const char *bios_path = 0;
    int i;
    for (i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--bios")) bios_path = argv[i + 1];
        else if (!strcmp(argv[i], "--exe")) exe_path = argv[i + 1];
        else if (!strcmp(argv[i], "--cue-bin")) host_cd_open(argv[i + 1]);
        else if (!strcmp(argv[i], "--vblanks")) max_vblanks = strtoull(argv[i + 1], 0, 10);
        else if (!strcmp(argv[i], "--seconds")) max_seconds = atof(argv[i + 1]);
        else if (!strcmp(argv[i], "--verbose")) verbose = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--dump")) dump_dir = argv[i + 1];
        else if (!strcmp(argv[i], "--frames")) frame_every = strtoull(argv[i + 1], 0, 10);
        else if (!strcmp(argv[i], "--watch")) watch_addr = strtoul(argv[i + 1], 0, 16) & 0x1FFFFFFC;
        else if (!strcmp(argv[i], "--trace-pc") && n_trace_pc < 8) trace_pc[n_trace_pc++] = strtoul(argv[i + 1], 0, 16);
    }
    if (!bios_path || !exe_path) {
        fprintf(stderr, "usage: rt --bios <scph.bin> --exe <PS-X EXE> [--cue-bin <disc.bin>] [--vblanks N] [--seconds S]\n");
        return 2;
    }
    if (read_file(bios_path, bios, sizeof bios) != (long)sizeof bios) { fprintf(stderr, "BIOS must be 512 KB\n"); return 1; }
    C->read_word = psx_read_word; C->write_word = write_word;
    C->read_half = psx_read_half; C->write_half = write_half;
    C->read_byte = read_byte; C->write_byte = write_byte;
    C->pc = 0xBFC00000u; SR = 0x00400000u;
    t0 = clock();
    setjmp(g_top);
    for (;;) {
        uint32_t pc = C->pc, p = pc & 0x1FFFFFFF;
        ring[ring_i++ & 31] = pc;
        if (!exe_loaded && pc == 0x80030000u) load_exe();
        if (exe_loaded && (p == 0xA0 || p == 0xB0 || p == 0xC0)) {
            uint32_t fn = R(9) & 0xFF;
            bios_calls[(p >> 4) - 0xA][fn]++;
            if ((p == 0xA0 && fn == 0x3C) || (p == 0xB0 && fn == 0x3D)) host_tty(R(4));
            if (p == 0xB0 && fn == 0x35 && R(4) == 1) { uint32_t k; for (k = 0; k < R(6) && k < 256; k++) host_tty(read_byte(R(5) + k)); }
            if (verbose && bios_calls[(p >> 4) - 0xA][fn] <= 3) printf("[rt] bios %s.%02X a0=%08X ra=%08X\n", bname[(p >> 4) - 0xA], fn, R(4), R(31));
        }
        if (irq_ready()) { n_irq++; exception(0, pc, 0); continue; }
        if (exe_loaded && psx_dispatch_game_compiled(C, pc)) { n_dispatch++; continue; }
        interp_block();
    }
}
