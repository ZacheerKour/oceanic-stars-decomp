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

/* F10 menu panel (640x480, square pixels): centred 4:3 box, whatever the window shape or scale_mode */
#define PSXRT_DRAW_MENU(px, ow, oh, ww, wh) do { \
    int mw_ = (wh) * 4 / 3, mh_ = (wh); \
    if (mw_ > (ww)) { mw_ = (ww); mh_ = (ww) * 3 / 4; } \
    gl_draw_osd_image((px), (ow), (oh), mw_, mh_, ((ww) - mw_) / 2, ((wh) - mh_) / 2, (ww), (wh)); } while (0)

/* main.cpp window creation: PSXRT_WINDOW_HEIGHT (settings.ini window_height, 360..4320) with PSXRT_WINDOW_WIDTH opens a w x h window
 * instead of the 4:3 one; a window larger than the display's usable area shrinks keeping its shape. The picture stays the centred
 * 4:3 rect and the side fill covers the rest. */
#define PSXRT_WINDOW_HEIGHT(gw, gh) do { \
    const char *eh_ = getenv("PSXRT_WINDOW_HEIGHT"), *ew_ = getenv("PSXRT_WINDOW_WIDTH"); \
    long long h_ = eh_ ? atoi(eh_) : 0, w_ = ew_ ? atoi(ew_) : 0; SDL_Rect b_; \
    if (h_ >= 360 && h_ <= 4320 && w_ >= 640) { \
        if (w_ > 16384) w_ = 16384; \
        if (SDL_GetDisplayUsableBounds(0, &b_) != 0 || b_.w <= 0 || b_.h <= 40) { b_.w = 3840; b_.h = 2160; } \
        b_.h -= 40; /* shortcut: fixed title-bar allowance (the window does not exist yet to ask its border size) */ \
        if (w_ > b_.w || h_ > b_.h) { \
            if ((long long)b_.w * h_ < (long long)b_.h * w_) { h_ = h_ * b_.w / w_; w_ = b_.w; } else { w_ = w_ * b_.h / h_; h_ = b_.h; } } \
        (gw) = (int)w_; (gh) = (int)h_; } } while (0)

/* after the fit: integer keeps k*240 rows (k >= 1) and the aspect; windows under 240 rows keep the fit */
#define PSXRT_LETTERBOX_INTEGER(num, den, dw, dh) \
    if (psxrt_scale_mode() == 1 && (dh) >= 240) { (dh) -= (dh) % 240; (dw) = (dh) * (num) / (den); }

/* main.cpp drain_host_events: a static frame skips its present (no swap), so a resize, maximise, fullscreen toggle or
 * expose would keep the stale picture; any window event forces the next presents. */
#define PSXRT_ON_HOST_EVENT(ev) \
    if ((ev).type >= SDL_EVENT_WINDOW_FIRST && (ev).type <= SDL_EVENT_WINDOW_LAST) gl_renderer_invalidate_present();

/* Graphics options menu (stubs/psxrt_menu_ui.inc) -> main.cpp statics. Expanded into main.cpp before local_mouse_live; the
 * values are the ones the OpenGL present reads on every frame, so a change shows on the next present. */
#define PSXRT_MAIN_ACCESSORS static int video_resolution_apply_live(int ir, int dynres, int ss_milli); extern "C" int psxrt_gfx_fullscreen_get(void) { return s_window_fullscreen.mode; } extern "C" int psxrt_gfx_fullscreen_set(int on) {     const int target = on ? (g_fullscreen ? g_fullscreen : 1) : 0;     if (psx_window_fullscreen_set(sdl_window, &s_window_fullscreen, target) != 0) return 0;     apply_present_cadence(); log_present_cadence(); gl_renderer_invalidate_present(); return 1; } extern "C" int psxrt_gfx_window_width(void) { int w = 0, h = 0; if (sdl_window) SDL_GetWindowSize(sdl_window, &w, &h); return w; } extern "C" int psxrt_gfx_window_resize(int w) {     if (!sdl_window || s_window_fullscreen.mode || w < 640 || w > 7680) return 0;     int h_ = w * 3 / 4; { const char *eh_ = getenv("PSXRT_WINDOW_HEIGHT"), *ew_ = getenv("PSXRT_WINDOW_WIDTH"); int h0_ = eh_ ? atoi(eh_) : 0, w0_ = ew_ ? atoi(ew_) : 0; if (h0_ >= 360 && w0_ >= 640) h_ = (int)((long long)w * h0_ / w0_); }     SDL_SetWindowSize(sdl_window, w, h_);     SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);     gl_renderer_invalidate_present(); return 1; } extern "C" int psxrt_gfx_filter_get(void) { return g_video_aa ? 1 : 0; } extern "C" void psxrt_gfx_filter_set(int linear) { g_video_aa = linear != 0; gl_renderer_invalidate_present(); } extern "C" void psxrt_gfx_redraw(void) { gl_renderer_invalidate_present(); } extern "C" int psxrt_gfx_scale_get(void) { return g_video_scale; } extern "C" int psxrt_gfx_scale_set(int n) { const int ot = g_title_scale, oi = g_video_internal_res; g_title_scale = n; const int r = video_resolution_apply_live(PSX_IR_UNSET, g_video_dynres, -1); if (!r) { g_title_scale = ot; g_video_internal_res = oi; } return r; }
