/* Headless evidence for native vs interpreted execution: with PSXRT_OVSTATS_SEC=<n> set, a background thread prints the
 * interpreter instruction count and (when AOT overlay C is linked) the static-overlay dispatch counters every n seconds.
 * Reads are racy (diagnostics only). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern uint64_t g_dirty_ram_insns_run;
extern uint64_t s_frame_count;
#ifdef PSX_HAS_OVERLAY_DISPATCH
void psx_overlay_static_get_stats(uint64_t *, uint64_t *, uint64_t *, uint64_t *);
#endif
__declspec(dllimport) void *__stdcall CreateThread(void *, size_t, unsigned long(__stdcall *)(void *), void *, unsigned long, unsigned long *);
__declspec(dllimport) void __stdcall Sleep(unsigned long);

static unsigned long __stdcall ovstats_thread(void *arg) {
    unsigned sec = (unsigned)(uintptr_t)arg;
    for (unsigned t = sec;; t += sec) {
        Sleep(sec * 1000u);
        uint64_t checks = 0, hits = 0, vmiss = 0, amiss = 0;
#ifdef PSX_HAS_OVERLAY_DISPATCH
        psx_overlay_static_get_stats(&checks, &hits, &vmiss, &amiss);
#endif
        fprintf(stderr, "[psxrt-ovstats] t=%us frames=%llu interp_insns=%llu static_hits=%llu checks=%llu variant_miss=%llu addr_miss=%llu\n",
                t, (unsigned long long)s_frame_count, (unsigned long long)g_dirty_ram_insns_run, (unsigned long long)hits,
                (unsigned long long)checks, (unsigned long long)vmiss, (unsigned long long)amiss);
    }
    return 0;
}

static void ovstats_start(void) {
    const char *e = getenv("PSXRT_OVSTATS_SEC");
    unsigned sec = e ? (unsigned)atoi(e) : 0u;
    if (sec) CreateThread(0, 0, ovstats_thread, (void *)(uintptr_t)sec, 0, 0);
}
#pragma section(".CRT$XCU", read)
#pragma comment(linker, "/include:psxrt_ovstats_init")
__declspec(allocate(".CRT$XCU")) void (*psxrt_ovstats_init)(void) = ovstats_start;
