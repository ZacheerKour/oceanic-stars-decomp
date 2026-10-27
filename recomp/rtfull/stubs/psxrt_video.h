/* Present-geometry and window hooks, force-included into the patched gpu_gl_renderer.c and main.cpp (CMakeLists.txt).
 * PSXRT_SCALE_MODE (play.ps1 writes it from settings.ini scale_mode):
 *   fit      largest 4:3 (or [video] aspect_ratio) rect, centred: symmetric pillar/letterbox (the default)
 *   integer  as fit, then shrunk to a whole multiple of 240 window pixels per PS1 field (sharp, more border)
 *   stretch  the whole window, aspect ignored
 * OpenGL renderer only: every OpenGL present, capture and OSD rect comes from letterbox_rect_aspect. */
#pragma once
#include <stdlib.h>
#include <string.h>

static int psxrt_scale_mode(void) {   /* 0 fit, 1 integer, 2 stretch; read once */
    static int m = -1;
    if (m < 0) {
        const char *e = getenv("PSXRT_SCALE_MODE");
        m = !e ? 0 : !strcmp(e, "integer") ? 1 : !strcmp(e, "stretch") ? 2 : 0;
    }
    return m;
}

/* before the fit: stretch makes the target aspect the window's own */
#define PSXRT_LETTERBOX_ADJUST(ww, wh, num, den) \
    if (psxrt_scale_mode() == 2 && (ww) > 0 && (wh) > 0) { (num) = (ww); (den) = (wh); }

/* after the fit: integer keeps k*240 rows (k >= 1) and the aspect; windows under 240 rows keep the fit */
#define PSXRT_LETTERBOX_INTEGER(num, den, dw, dh) \
    if (psxrt_scale_mode() == 1 && (dh) >= 240) { (dh) -= (dh) % 240; (dw) = (dh) * (num) / (den); }

/* main.cpp drain_host_events: a static frame skips its present (no swap), so a resize, maximise, fullscreen toggle or
 * expose would keep the stale picture; any window event forces the next presents. */
#define PSXRT_ON_HOST_EVENT(ev) \
    if ((ev).type >= SDL_EVENT_WINDOW_FIRST && (ev).type <= SDL_EVENT_WINDOW_LAST) gl_renderer_invalidate_present();
