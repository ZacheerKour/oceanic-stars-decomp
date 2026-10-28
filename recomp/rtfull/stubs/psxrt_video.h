/* Present-geometry and window hooks, force-included into the patched gpu_gl_renderer.c and main.cpp (CMakeLists.txt).
 * PSXRT_SCALE_MODE (play.ps1 writes it from settings.ini scale_mode):
 *   fit      largest 4:3 (or [video] aspect_ratio) rect, centred: symmetric pillar/letterbox (the default)
 *   integer  as fit, then shrunk to a whole multiple of 240 window pixels per PS1 field (sharp, more border)
 *   stretch  the whole window, aspect ignored
 * OpenGL renderer only: every OpenGL present, capture and OSD rect comes from letterbox_rect_aspect. */
#pragma once
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
int psxrt_scale_mode(void);            /* stubs/psxrt_io.c: one shared value (this header is compiled into two TUs), settable live */
void psxrt_scale_mode_set(int mode);
int psxrt_launch_fullscreen(void);     /* settings.ini window_mode = borderless (PSXRT_FULLSCREEN=1) */
#ifdef __cplusplus
}
#endif

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

/* Graphics options menu (stubs/psxrt_menu_ui.inc) -> main.cpp statics. Expanded into main.cpp before local_mouse_live; the
 * values are the ones the OpenGL present reads on every frame, so a change shows on the next present. */
#define PSXRT_MAIN_ACCESSORS static int video_resolution_apply_live(int ir, int dynres, int ss_milli); extern "C" int psxrt_gfx_fullscreen_get(void) { return s_window_fullscreen.mode; } extern "C" int psxrt_gfx_fullscreen_set(int on) {     const int target = on ? (g_fullscreen ? g_fullscreen : 1) : 0;     if (psx_window_fullscreen_set(sdl_window, &s_window_fullscreen, target) != 0) return 0;     apply_present_cadence(); log_present_cadence(); gl_renderer_invalidate_present(); return 1; } extern "C" int psxrt_gfx_window_width(void) { int w = 0, h = 0; if (sdl_window) SDL_GetWindowSize(sdl_window, &w, &h); return w; } extern "C" int psxrt_gfx_window_resize(int w) {     if (!sdl_window || s_window_fullscreen.mode || w < 640 || w > 7680) return 0;     SDL_SetWindowSize(sdl_window, w, w * 3 / 4);     SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);     gl_renderer_invalidate_present(); return 1; } extern "C" int psxrt_gfx_filter_get(void) { return g_video_aa ? 1 : 0; } extern "C" void psxrt_gfx_filter_set(int linear) { g_video_aa = linear != 0; gl_renderer_invalidate_present(); } extern "C" void psxrt_gfx_redraw(void) { gl_renderer_invalidate_present(); } extern "C" int psxrt_gfx_scale_get(void) { return g_video_scale; } extern "C" int psxrt_gfx_scale_set(int n) { g_title_scale = n; return video_resolution_apply_live(PSX_IR_UNSET, g_video_dynres, -1); }
