/* Audio output health, WAV capture, device-change handling and the window modal-loop pump (hooked into main.cpp /
 * psx_sdl_audio.cpp by CMake patches; header psxrt_audio.h is force-included there).
 *
 * PSXRT_AUDIO_STATS=1: once per wall second, log "[psxrt-audio-stats]" from the SDL audio thread: SPU frames produced
 * per second (should be ~44100), frames the device pulled per second, DRC ring fill (queued ms), underrun events (a pull
 * that found the ring dry after one that did not), dry ms, overflow drops, and the longest stretch with no SPU output.
 * PSXRT_AUDIO_WAV=<file>: write every byte the device pulls (stream input rate, S16 stereo) to a WAV; the header is
 * rewritten each second, so a killed run still leaves a valid file.
 * Device events (added/removed/format changed), reopen attempts and their errors always go to the log ("[psxrt-audio]").
 * The playback device is SDL's default logical device, so SDL itself moves the stream when Windows changes the default
 * (KVM switch, headset plugged in). If our device is removed instead, or stops pulling audio for 1.5 s while not paused,
 * the stream is reopened on the current default. PSXRT_AUDIO_TEST_REOPEN=<n>: force such a reopen every n seconds.
 *
 * Window modal loop: dragging or resizing the window (or opening its system menu) makes Windows run its own message
 * loop inside DispatchMessage until the mouse is released, so the emulation thread, which pumps the messages, stopped
 * and so did the SPU (silence for the whole drag). The pump now runs on a second fiber of the same thread, and a thread
 * timer that Windows also dispatches inside the modal loop switches back to the emulation fiber, which keeps running
 * frames and returns to the modal loop at its next pump. PSXRT_MODAL_FIBER=0 turns this off (old behaviour). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "recomp_audio_drc.h"
#include "spu.h"
#include "psxrt_audio.h"
#undef SDL_PumpEvents   /* this file is the one place that calls the real ones */
#undef SDL_PollEvent
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int env_on(const char *name) { const char *e = getenv(name); return e && *e && *e != '0'; }

static FILE *s_wav;
static uint32_t s_wav_rate;
static uint64_t s_wav_bytes;
static volatile uint64_t s_last_pull_ms;   /* audio thread writes, main thread reads (watchdog) */

static void wav_header(void) {
    uint32_t data = s_wav_bytes > 0xFFFFFFF0u ? 0xFFFFFFF0u : (uint32_t)s_wav_bytes;
    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    uint32_t v = data + 36; memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    v = 16; memcpy(h + 16, &v, 4);
    uint16_t s = 1; memcpy(h + 20, &s, 2);          /* PCM */
    s = 2; memcpy(h + 22, &s, 2);                   /* stereo */
    memcpy(h + 24, &s_wav_rate, 4);
    v = s_wav_rate * 4; memcpy(h + 28, &v, 4);
    s = 4; memcpy(h + 32, &s, 2);
    s = 16; memcpy(h + 34, &s, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &data, 4);
    long pos = ftell(s_wav);
    fseek(s_wav, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, s_wav);
    fseek(s_wav, pos < 44 ? 44 : pos, SEEK_SET);
    fflush(s_wav);
}

/* Runs on the SDL audio thread after every DRC pull: `pcm` is exactly what the device stream receives. */
void psxrt_audio_pulled(const void *bridge, const void *pcm, int bytes) {
    static int stats = -1;
    static uint64_t t0, pulled, last_pushed, last_ur, last_ov, ur_events, dry_frames, stall_t0, stall_max, prev_ms;
    static int dry;
    const rab_bridge *b = (const rab_bridge *)bridge;
    const uint64_t now = SDL_GetTicks();
    if (stats < 0) {
        stats = env_on("PSXRT_AUDIO_STATS");
        const char *w = getenv("PSXRT_AUDIO_WAV");
        if (w && *w) {
            s_wav = fopen(w, "wb");
            s_wav_rate = (uint32_t)b->cfg.host_rate;
            if (s_wav) wav_header();
            else fprintf(stderr, "[psxrt-audio] cannot open PSXRT_AUDIO_WAV=%s\n", w);
        }
        t0 = prev_ms = now;
        last_pushed = b->stats.pushed_frames; last_ur = b->stats.underrun_events; last_ov = b->stats.overflow_drops;
    }
    s_last_pull_ms = now;
    if (s_wav) { fwrite(pcm, 1, (size_t)bytes, s_wav); s_wav_bytes += (uint64_t)bytes; }
    if (!stats && !s_wav) return;
    pulled += (uint64_t)(bytes / 4);
    /* dry = a pull in which the bridge hit underrun (its counter counts events, not frames) or was re-priming; dry_frames adds the whole pull, so dry ms is an upper bound */
    static int was_primed;
    const uint64_t ur = b->stats.underrun_events;
    const uint64_t d = ur != last_ur ? ur - last_ur : was_primed && !b->primed ? (uint64_t)(bytes / 4) : 0;
    if (d) { if (!dry) ur_events++; dry = 1; dry_frames += (uint64_t)(bytes / 4); last_ur = ur; }
    else dry = 0;
    was_primed |= b->primed;
    /* No SPU output since the last pull: the producer (emulation thread) is stalled. */
    if (b->stats.pushed_frames == last_pushed) { if (!stall_t0) stall_t0 = prev_ms; }
    else if (stall_t0) { if (now - stall_t0 > stall_max) stall_max = now - stall_t0; stall_t0 = 0; }
    prev_ms = now;
    if (now - t0 < 1000) return;
    if (stall_t0 && now - stall_t0 > stall_max) stall_max = now - stall_t0;
    const double sec = (double)(now - t0) / 1000.0;
    if (stats) {
        static uint64_t total_events;
        static uint32_t last_kon;
        SpuDebugInfo si;   /* read from the audio thread without a lock: counters for the log only */
        spu_debug_info(&si);
        int voices = 0;
        for (uint32_t m = si.active_mask; m; m &= m - 1) voices++;
        total_events += ur_events;
        fprintf(stderr, "[psxrt-audio-stats] t=%.1f spu=%.0f/s pulled=%.0f/s fill=%.0fms underruns=%llu (total %llu) dry<=%.0fms "
                "overflow=%llu corr=%+.4f stall_max=%llums keyon=%u voices=%d spuctrl=%04x main=%d/%d\n", (double)now / 1000.0,
                (double)(b->stats.pushed_frames - last_pushed) / sec, (double)pulled / sec, b->stats.last_fill_ms,
                (unsigned long long)ur_events, (unsigned long long)total_events, (double)dry_frames * 1000.0 / b->cfg.host_rate,
                (unsigned long long)(b->stats.overflow_drops - last_ov), b->stats.last_correction, (unsigned long long)stall_max,
                si.key_on_count - last_kon, voices, (unsigned)si.ctrl, si.main_l, si.main_r);
        last_kon = si.key_on_count;
        fflush(stderr);
    }
    if (s_wav) wav_header();
    t0 = now; pulled = 0; ur_events = 0; dry_frames = 0; stall_max = 0;
    last_pushed = b->stats.pushed_frames; last_ov = b->stats.overflow_drops;
}

/* ---- device changes (main thread) ---- */
static volatile int s_reopen_wanted;

static bool SDLCALL audio_event_watch(void *u, SDL_Event *e) {
    (void)u;
    if (e->type != SDL_EVENT_AUDIO_DEVICE_ADDED && e->type != SDL_EVENT_AUDIO_DEVICE_REMOVED &&
        e->type != SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED)
        return true;
    const SDL_AudioDeviceID ours = psxrt_audio_device();
    const char *what = e->type == SDL_EVENT_AUDIO_DEVICE_ADDED ? "added"
                     : e->type == SDL_EVENT_AUDIO_DEVICE_REMOVED ? "removed" : "format changed";
    const char *name = SDL_GetAudioDeviceName(e->adevice.which);
    fprintf(stderr, "[psxrt-audio] device %s: id %u%s %s%s\n", what, (unsigned)e->adevice.which,
            e->adevice.recording ? " (recording)" : "", name ? name : "", ours && e->adevice.which == ours ? " (ours)" : "");
    if (e->type == SDL_EVENT_AUDIO_DEVICE_REMOVED && ours && e->adevice.which == ours) s_reopen_wanted = 1;
    fflush(stderr);
    return true;
}

static void audio_log_device(const char *why) {
    const SDL_AudioDeviceID dev = psxrt_audio_device();
    const char *drv = SDL_GetCurrentAudioDriver(), *name = dev ? SDL_GetAudioDeviceName(dev) : NULL;
    SDL_AudioSpec spec;
    int frames = 0;
    if (dev && SDL_GetAudioDeviceFormat(dev, &spec, &frames))
        fprintf(stderr, "[psxrt-audio] %s: driver %s, \"%s\", device %d Hz %d ch, %d frames per device buffer\n", why,
                drv ? drv : "?", name ? name : "?", spec.freq, spec.channels, frames);
    else
        fprintf(stderr, "[psxrt-audio] %s: no playback device (%s)\n", why, SDL_GetError());
    fflush(stderr);
}

static void audio_service(void) {
    static int init;
    static uint64_t next_try, test_every, next_test;
    const uint64_t now = SDL_GetTicks();
    if (!init) {
        init = 1;
        SDL_AddEventWatch(audio_event_watch, NULL);
        const char *e = getenv("PSXRT_AUDIO_TEST_REOPEN");
        test_every = e ? (uint64_t)strtoul(e, NULL, 10) * 1000u : 0;
        next_test = now + test_every;
        if (psxrt_audio_device()) audio_log_device("audio opened");
    }
    if (!psxrt_audio_device()) return;   /* audio never opened (no device at start, or built without audio) */
    if (test_every && now >= next_test) {
        next_test = now + test_every;
        fprintf(stderr, "[psxrt-audio] PSXRT_AUDIO_TEST_REOPEN: forcing a device reopen\n");
        s_reopen_wanted = 1;
    }
    const int64_t idle = s_last_pull_ms ? (int64_t)(now - s_last_pull_ms) : 0;   /* the audio thread may be ahead of `now` */
    if (!s_reopen_wanted && idle > 1500 && !SDL_AudioDevicePaused(psxrt_audio_device())) {
        fprintf(stderr, "[psxrt-audio] ERROR: the playback device has not asked for audio for %lld ms; reopening it\n",
                (long long)idle);
        s_reopen_wanted = 1;
    }
    if (!s_reopen_wanted || now < next_try) return;
    if (psxrt_audio_reopen() == 0) {
        s_reopen_wanted = 0;
        s_last_pull_ms = now;
        audio_log_device("reopened");
    } else {
        fprintf(stderr, "[psxrt-audio] ERROR: reopening the default playback device failed: %s (retry in 2 s)\n", SDL_GetError());
        next_try = now + 2000;
        fflush(stderr);
    }
}

/* ---- event pump on its own fiber ---- */
static void *s_main_fiber, *s_pump_fiber;
static DWORD s_main_tid;
static int s_fiber = -1;           /* -1 not set up, 0 off, 1 on */
static volatile int s_modal;       /* the pump fiber is inside a Win32 modal loop */
static uint64_t s_pump_ns, s_modal_enter_ms;

static void CALLBACK pump_fiber(void *p) {
    (void)p;
    for (;;) {
        SDL_PumpEvents();
        if (s_modal) {
            s_modal = 0;
            fprintf(stderr, "[psxrt-audio] window modal loop ended after %llu ms; emulation kept running\n",
                    (unsigned long long)(SDL_GetTicks() - s_modal_enter_ms));
        }
        SwitchToFiber(s_main_fiber);
    }
}

/* Dispatched by every message loop of this thread: SDL's own pump (a due tick: ignored) or a modal move/size loop. */
static void CALLBACK modal_tick(HWND h, UINT m, UINT_PTR id, DWORD t) {
    (void)h; (void)m; (void)id; (void)t;
    if (GetCurrentFiber() != s_pump_fiber) return;
    if (!s_modal) {
        GUITHREADINFO gti = { sizeof gti };
        if (!GetGUIThreadInfo(GetCurrentThreadId(), &gti) || !(gti.flags & (GUI_INMOVESIZE | GUI_INMENUMODE))) return;
        s_modal = 1;
        s_modal_enter_ms = SDL_GetTicks();
        fprintf(stderr, "[psxrt-audio] window modal loop (move/size/menu): emulation continues on the main fiber\n");
    }
    SwitchToFiber(s_main_fiber);   /* resumes here at the emulation's next pump */
}

static void fiber_setup(void) {
    s_fiber = 0;
    const char *e = getenv("PSXRT_MODAL_FIBER");
    if (e && *e == '0') return;
    s_main_tid = GetCurrentThreadId();
    s_main_fiber = IsThreadAFiber() ? GetCurrentFiber() : ConvertThreadToFiber(NULL);
    if (s_main_fiber) s_pump_fiber = CreateFiber(1u << 20, pump_fiber, NULL);
    /* USER_TIMER_MINIMUM (10 ms): how long the emulation can wait for the thread inside a modal loop */
    if (!s_main_fiber || !s_pump_fiber || !SetTimer(NULL, 0, USER_TIMER_MINIMUM, modal_tick)) {
        fprintf(stderr, "[psxrt-audio] ERROR: modal-loop fiber setup failed (%lu); dragging the window will pause the game\n",
                GetLastError());
        return;
    }
    s_fiber = 1;
}

void psxrt_pump_events(void) {
    if (s_fiber < 0) fiber_setup();
    audio_service();
    if (s_fiber != 1 || GetCurrentThreadId() != s_main_tid || GetCurrentFiber() != s_main_fiber) { SDL_PumpEvents(); return; }
    /* Inside a modal loop a pump lasts until the next timer tick: pump at most once per ~frame, and since the tick is
     * then already due the modal loop hands the thread straight back. */
    if (s_modal && SDL_GetTicksNS() - s_pump_ns < 14000000) return;
    s_pump_ns = SDL_GetTicksNS();
    SwitchToFiber(s_pump_fiber);
}

bool psxrt_poll_event(SDL_Event *ev) {
    const SDL_EventAction act = ev ? SDL_GETEVENT : SDL_PEEKEVENT;
    if (SDL_PeepEvents(ev, 1, act, SDL_EVENT_FIRST, SDL_EVENT_LAST) > 0) return true;
    psxrt_pump_events();
    return SDL_PeepEvents(ev, 1, act, SDL_EVENT_FIRST, SDL_EVENT_LAST) > 0;
}
