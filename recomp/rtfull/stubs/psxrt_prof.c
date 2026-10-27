/* Sampling profiler for the emulation thread (no PDB needed): with PSXRT_PROF=1 a background thread suspends the thread
 * that runs the pad hook about every 1 ms, reads its RIP and appends the RVA (uint32, relative to the exe image) to
 * prof.bin in the working directory. recomp/rtfull/prof_report.py maps the RVAs to functions with the linker map
 * (psxrecomp-game.map, /MAP). Diagnostics only: the samples are not synchronised with anything. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static HANDLE s_target;

static DWORD WINAPI prof_thread(void *arg) {
    (void)arg;
    FILE *f = fopen("prof.bin", "wb");
    if (!f) return 0;
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uint32_t buf[1024];
    unsigned n = 0;
    for (;;) {
        Sleep(1);
        if (SuspendThread(s_target) == (DWORD)-1) break;
        CONTEXT c;
        c.ContextFlags = CONTEXT_CONTROL;
        BOOL ok = GetThreadContext(s_target, &c);
        ResumeThread(s_target);
        if (!ok) continue;
        uintptr_t rip = (uintptr_t)c.Rip;
        buf[n++] = rip >= base && rip - base < 0xFFFFFFFFu ? (uint32_t)(rip - base) : 0xFFFFFFFFu;  /* outside: DLL/kernel */
        if (n == 1024) { fwrite(buf, 4, n, f); fflush(f); n = 0; }
    }
    fclose(f);
    return 0;
}

void psxrt_prof_start(void) {   /* called once from the pad hook, i.e. on the emulation thread */
    const char *e = getenv("PSXRT_PROF");
    if (!e || *e != '1') return;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_target,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, 0)) return;
    CreateThread(NULL, 0, prof_thread, NULL, 0, NULL);
    fprintf(stderr, "[psxrt-prof] sampling the emulation thread into prof.bin\n");
}
