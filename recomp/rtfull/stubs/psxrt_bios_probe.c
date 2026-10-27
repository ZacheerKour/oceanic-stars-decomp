/* BIOS evidence probe (headless): with PSXRT_BIOS_PROBE=1 a background thread logs, once a second and only on change,
 * the kernel device-table words [0x150]/[0x154], the 'bu' DCB's +0x34 (firstfile) and the game's saved handler
 * (program A 0x8004EFE4); and arms the interpreter's call-resolution ring (dirty_ram_interp.c g_callret_*) on the
 * jalr at A 0x8004B344 so every call through the saved DCB+0x34 handler is printed with its target and returned v0.
 * Only works while program A runs interpreted (no overlay C for it linked). Reads are racy (diagnostics only). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int psx_peek_word(uint32_t addr, uint32_t *out);   /* memory.c: RAM/scratchpad/ROM only, no side effects */
extern uint32_t g_callret_lo, g_callret_hi;
extern uint64_t g_callret_seq;
typedef struct {   /* mirror of dirty_ram_interp.c CallRetEnt (f42d2c0), field for field */
    uint64_t cycle; uint32_t frame;
    uint32_t pc, target, sp_b, ra_b, s0_b, s3_b;
    uint32_t path;
    uint32_t pc_a, ra_a, sp_a, s0_a, s3_a, v0_a;
    uint32_t bail_a, rfe_a, esc_a, in_exc_a;
    uint32_t dstatic, dblocks;
    uint32_t dexc;
    uint32_t last_func_a;
} CallRetEnt;
extern CallRetEnt g_callret_ring[64];
__declspec(dllimport) void *__stdcall CreateThread(void *, size_t, unsigned long(__stdcall *)(void *), void *, unsigned long, unsigned long *);
__declspec(dllimport) void __stdcall Sleep(unsigned long);

static uint32_t rd32(uint32_t a) {
    uint32_t v = 0;
    psx_peek_word(a, &v);
    return v;
}
static uint32_t rb(uint32_t a) { return (rd32(a & ~3u) >> ((a & 3u) * 8u)) & 0xFFu; }

static unsigned long __stdcall probe_thread(void *arg) {
    (void)arg;
    uint32_t last[4] = {1, 1, 1, 1};
    uint64_t seen = 0;
    for (unsigned t = 0;; t++) {
        Sleep(1000);
        uint32_t base = rd32(0x150), size = rd32(0x154), bu = 0, ff = 0;
        for (uint32_t off = 0; base && off < size && off < 0x50u * 32u; off += 0x50) {   /* find the 'bu' record */
            uint32_t name = rd32(base + off);
            if (name && rb(name) == 'b' && rb(name + 1) == 'u' && rb(name + 2) == 0) { bu = base + off; ff = rd32(bu + 0x34); break; }   /* "bu\0" */
        }
        uint32_t now[4] = {base, size, ff, rd32(0x8004EFE4)};
        if (memcmp(now, last, sizeof now)) {
            fprintf(stderr, "[psxrt-bios] t=%us [0x150]=%08X [0x154]=%08X (%u records) bu_dcb=%08X bu+0x34=%08X A[0x8004EFE4]=%08X\n",
                    t, base, size, size / 0x50u, bu, ff, now[3]);
            memcpy(last, now, sizeof now);
        }
        uint64_t seq = g_callret_seq;
        if (seq - seen > 64) seen = seq - 64;
        for (; seen < seq; seen++) {
            const CallRetEnt *e = &g_callret_ring[seen & 63u];
            fprintf(stderr, "[psxrt-bios] call #%llu frame %u: jalr at %08X -> %08X (sp %08X) returned v0=%08X pc_after=%08X path=%u\n",
                    (unsigned long long)seen, e->frame, e->pc, e->target, e->sp_b, e->v0_a, e->pc_a, e->path);
        }
    }
    return 0;
}

static void probe_start(void) {
    const char *e = getenv("PSXRT_BIOS_PROBE");
    if (!e || e[0] != '1') return;
    g_callret_lo = 0x8004B344u;   /* program A: jalr through the saved DCB+0x34 handler */
    g_callret_hi = 0x8004B348u;
    CreateThread(0, 0, probe_thread, 0, 0, 0);
}
#pragma section(".CRT$XCU", read)
#pragma comment(linker, "/include:psxrt_bios_probe_init")
__declspec(allocate(".CRT$XCU")) void (*psxrt_bios_probe_init)(void) = probe_start;
