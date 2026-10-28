/* Pause menu (F10 / Select+Start / Back+L3): drawing and actions. #included into a patched copy of psx_savestate_menu.c
 * (recomp/rtfull/CMakeLists.txt) so it can use that file's 8x8 font and rect helpers, and it replaces psx_runtime_menu.c,
 * whose non-launcher stubs left main.cpp's menu loop (pause guest and audio, keyboard + pad navigation) with nothing to
 * show. The state machine itself is psxrt_menu_input() in psxrt_io.c. */
#include <stdlib.h>
#include <stdio.h>
#include "psx_runtime_menu.h"
#include "psx_disc_tray.h"
#include "psxrt_io.h"
#include "host_osd.h"

typedef char psxrt_chk_inputs[(PM_IN_TOGGLE == PSX_MENU_TOGGLE && PM_IN_BACK == PSX_MENU_BACK && PM_IN_UP == PSX_MENU_UP &&
                               PM_IN_DOWN == PSX_MENU_DOWN && PM_IN_LEFT == PSX_MENU_LEFT && PM_IN_RIGHT == PSX_MENU_RIGHT &&
                               PM_IN_ACCEPT == PSX_MENU_ACCEPT) ? 1 : -1];

static PsxrtMenu pm_m;
static int pm_is_open, pm_dirty = 1;
static char pm_note[64];
static uint32_t pm_img[SSM_W * SSM_H];
int psx_video_get_scanlines(float *strength);   /* main.cpp */
void psx_video_set_scanlines(int on, float strength);

int psxrt_pm_open(void) { return pm_is_open; }

static void pm_render_gfx(void);
static void pm_render(void)
{
    if (pm_m.page) { pm_render_gfx(); return; }
    char buf[96];
    const int slot = psxrt_slot(), nd = psx_disc_tray_disc_count();
    int i;
    for (i = 0; i < SSM_W * SSM_H; i++) pm_img[i] = 0xFF0F1118u;
    fill_rect(pm_img, 0, 0, SSM_W, 56, 0xFF171B25u);
    draw_text(pm_img, 24, 20, "GAME MENU", 0xFFFFD24Du, 2);
    for (i = 0; i < PM_ROWS; i++) {
        const int y = 70 + i * 46, sel = (i == pm_m.sel);
        switch (i) {
        case PM_RESUME: snprintf(buf, sizeof buf, "RESUME"); break;
        case PM_SAVE: snprintf(buf, sizeof buf, "SAVE STATE  < SLOT %d >  %s", slot + 1, savestate_slot_exists(slot) ? "USED" : "EMPTY"); break;
        case PM_LOAD: snprintf(buf, sizeof buf, "LOAD STATE  < SLOT %d >  %s", slot + 1, savestate_slot_exists(slot) ? "USED" : "EMPTY"); break;
        case PM_SWAP:
            if (nd > 1) snprintf(buf, sizeof buf, "SWAP DISC  < DISC %d >  IN DRIVE %d", pm_m.disc_target, psx_disc_tray_mounted_disc());
            else snprintf(buf, sizeof buf, "SWAP DISC  (ONE DISC ONLY)");
            break;
        case PM_FULLSCREEN: snprintf(buf, sizeof buf, "TOGGLE FULLSCREEN"); break;
        case PM_GRAPHICS: snprintf(buf, sizeof buf, "GRAPHICS OPTIONS  >"); break;
        default: snprintf(buf, sizeof buf, pm_m.quit_armed ? "QUIT? PRESS CROSS/ENTER AGAIN" : "QUIT GAME"); break;
        }
        fill_rect(pm_img, 24, y, SSM_W - 48, 40, sel ? 0xFF2B2830u : 0xFF191D27u);
        stroke_rect(pm_img, 24, y, SSM_W - 48, 40, sel ? 0xFFFFD24Du : 0xFF303746u);
        draw_text(pm_img, 40, y + 12, buf, sel ? (i == PM_QUIT && pm_m.quit_armed ? 0xFFFF6B6Bu : 0xFFFFD24Du) : 0xFFE2E5EBu, 2);
    }
    if (pm_note[0]) draw_text(pm_img, 24, 410, pm_note, 0xFFFF6B6Bu, 1);
    fill_rect(pm_img, 0, 430, SSM_W, 50, 0xFF171B25u);
    draw_text(pm_img, 24, 440, "UP/DOWN MOVE  LEFT/RIGHT CHANGE  CROSS/ENTER OK", 0xFFB8BDC8u, 1);
    draw_text(pm_img, 24, 456, "CIRCLE/ESC BACK  F10 OR SELECT+START CLOSES", 0xFFB8BDC8u, 1);
    pm_dirty = 0;
}


/* ---- Graphics options page: live values from the runtime, changes applied at once and written back to settings.ini ---- */
static int pm_win_w;   /* last windowed width */
static void pm_flush(int force)   /* debounced settings.ini writes (psxrt_ini_queue): quiet 400 ms, or forced on close */
{
    const int r = psxrt_ini_flush(psxrt_now_ms(), force);
    if (r == 2) return;
    if (r == 0) snprintf(pm_note, sizeof pm_note, "COULD NOT SAVE settings.ini");
    else if (r == -2) snprintf(pm_note, sizeof pm_note, "COULD NOT SAVE: FILE ENCODING (SETTINGS.INI IS UTF-16)");
    pm_dirty = 1;
    fprintf(stderr, "[psxrt-gfx] settings.ini write %d\n", r);
}
static void pm_close(void) { pm_flush(1); pm_is_open = 0; }
static void gfx_refresh(void)
{
    PsxrtGfx *g = &pm_m.g;
    int w;
    if (!pm_win_w) { const char *e = getenv("PSXRT_WINDOW_WIDTH"); pm_win_w = e && atoi(e) >= 640 ? atoi(e) : 960; }
    g->fs = psxrt_gfx_fullscreen_get() ? 1 : 0;
    if (!g->fs && (w = psxrt_gfx_window_width()) > 0) pm_win_w = w;
    g->width = pm_win_w;
    g->mode = psxrt_scale_mode();
    g->linear = psxrt_gfx_filter_get();
    g->ires = psxrt_gfx_scale_get();
    g->scan = psx_video_get_scanlines(NULL) ? 1 : 0;
}
static void gfx_apply(void)
{
    PsxrtGfx *g = &pm_m.g;
    char k[32] = "", v[32] = "";
    int ok = 1;   /* only a change that took effect is written to settings.ini, and with the value that is actually applied */
    const int row = pm_m.gsel;
    switch (row) {
    case GR_WINMODE:
        if (!psxrt_gfx_fullscreen_set(g->fs)) { ok = 0; snprintf(pm_note, sizeof pm_note, "WINDOW MODE CHANGE FAILED"); }
        else if (!g->fs) psxrt_gfx_window_resize(pm_win_w);   /* back to windowed: the size chosen meanwhile */
        gfx_refresh();   /* real mode, not the requested one */
        break;
    case GR_WINSIZE:
        _putenv("PSXRT_WINDOW_HEIGHT=0");   /* picking a size = 4:3 window; the resize below reads this */
        pm_win_w = g->width;
        if (!g->fs) psxrt_gfx_window_resize(g->width);
        break;
    case GR_SCALE: psxrt_scale_mode_set(g->mode); psxrt_gfx_redraw(); break;
    case GR_FILTER: psxrt_gfx_filter_set(g->linear); break;
    case GR_IRES:   /* refused: the runtime keeps its scale and nothing is saved */
        if (!psxrt_gfx_scale_set(g->ires)) { ok = 0; snprintf(pm_note, sizeof pm_note, "INTERNAL RES NOT APPLIED: RENDERER REFUSED"); }
        g->ires = psxrt_gfx_scale_get();
        break;
    case GR_SCAN: psx_video_set_scanlines(g->scan, -1.0f); break;
    }
    if (ok && psxrt_gfx_kv(row, g, k, v, sizeof k)) psxrt_ini_queue(k, v, psxrt_now_ms());
    if (ok && row == GR_WINSIZE) { psxrt_ini_queue("window_height", "0", psxrt_now_ms()); }   /* this row picks a 4:3 window */
    fprintf(stderr, "[psxrt-gfx] row %d %s = %s, queued %d, fullscreen %d, window %d, scale %d\n", row, k, v, ok && k[0],
            psxrt_gfx_fullscreen_get(), psxrt_gfx_window_width(), psxrt_gfx_scale_get());
    if (row != GR_WINMODE) gfx_refresh();
}
static void pm_render_gfx(void)
{
    static const char *const mode_n[3] = { "FIT", "INTEGER", "STRETCH" };
    static const char *const size_n[5] = { "480P", "720P", "1080P", "1440P", "2160P" };
    static const char *const hint[GR_ROWS] = {
        "WINDOWED OR BORDERLESS FULLSCREEN (ALT+ENTER / F11 ALSO TOGGLE)", "WINDOW CLIENT SIZE, 4:3 (APPLIES WHILE WINDOWED)",
        "FIT = LARGEST 4:3  INTEGER = SHARP WHOLE MULTIPLES  STRETCH = FILL WINDOW", "LINEAR = SMOOTH  NEAREST = HARD PIXELS",
        "3D RENDER RESOLUTION, SHARPER BUT NEEDS A FASTER GPU", "CRT SCANLINE LOOK (F4 ALSO TOGGLES)", "" };
    static const char *const lab[GR_ROWS] = { "WINDOW MODE", "WINDOW SIZE", "SCALE MODE", "FILTER", "INTERNAL RES", "SCANLINES", "BACK" };
    const PsxrtGfx *g = &pm_m.g;
    char val[40];
    int i, p;
    for (i = 0; i < SSM_W * SSM_H; i++) pm_img[i] = 0xFF0F1118u;
    fill_rect(pm_img, 0, 0, SSM_W, 56, 0xFF171B25u);
    draw_text(pm_img, 24, 20, "GRAPHICS OPTIONS", 0xFFFFD24Du, 2);
    for (i = 0; i < GR_ROWS; i++) {
        const int y = 66 + i * 44, sel = (i == pm_m.gsel);
        val[0] = 0;
        switch (i) {
        case GR_WINMODE: snprintf(val, sizeof val, "< %s >", g->fs ? "BORDERLESS FULL" : "WINDOWED"); break;
        case GR_WINSIZE:
            for (p = 0; p < 5 && psxrt_widths[p] != g->width; p++) {}
            snprintf(val, sizeof val, "< %dX%d %s >", g->width, g->width * 3 / 4, p < 5 ? size_n[p] : "");
            break;
        case GR_SCALE: snprintf(val, sizeof val, "< %s >", mode_n[g->mode]); break;
        case GR_FILTER: snprintf(val, sizeof val, "< %s >", g->linear ? "LINEAR" : "NEAREST"); break;
        case GR_IRES: snprintf(val, sizeof val, "< %dX >", g->ires); break;
        case GR_SCAN: snprintf(val, sizeof val, "< %s >", g->scan ? "ON" : "OFF"); break;
        }
        fill_rect(pm_img, 24, y, SSM_W - 48, 40, sel ? 0xFF2B2830u : 0xFF191D27u);
        stroke_rect(pm_img, 24, y, SSM_W - 48, 40, sel ? 0xFFFFD24Du : 0xFF303746u);
        draw_text(pm_img, 40, y + 12, lab[i], sel ? 0xFFFFD24Du : 0xFFE2E5EBu, 2);
        if (val[0]) draw_text(pm_img, 250, y + 12, val, sel ? 0xFFFFFFFFu : 0xFFB8BDC8u, 2);
    }
    draw_text(pm_img, 24, 386, hint[pm_m.gsel], 0xFF7F8796u, 1);
    if (pm_m.gsel == GR_IRES) draw_text(pm_img, 24, 398, "APPLIED LIVE; IF THE RENDERER REFUSES IT, NOTHING IS SAVED", 0xFF7F8796u, 1);
    if (pm_note[0]) draw_text(pm_img, 24, 410, pm_note, 0xFFFF6B6Bu, 1);
    fill_rect(pm_img, 0, 430, SSM_W, 50, 0xFF171B25u);
    draw_text(pm_img, 24, 440, "UP/DOWN MOVE  LEFT/RIGHT OR CROSS/ENTER CHANGE", 0xFFB8BDC8u, 1);
    draw_text(pm_img, 24, 456, "CIRCLE/ESC BACK  CHANGES ARE SAVED TO SETTINGS.INI", 0xFFB8BDC8u, 1);
    pm_dirty = 0;
}

void psx_runtime_menu_init(void) { pm_is_open = 0; }
int psx_runtime_menu_open(void)
{
    if (!pm_is_open) {
        psxrt_menu_reset(&pm_m, psx_disc_tray_mounted_disc(), psx_disc_tray_disc_count());
        pm_note[0] = 0;
        pm_is_open = 1;
    }
    pm_dirty = 1;
    return 1;
}
void psx_runtime_menu_close(void) { pm_close(); }
int psx_runtime_menu_is_open(void) { return pm_is_open; }
void psx_runtime_menu_input(int input, int repeat)
{
    int a, r;
    char msg[48];
    if (!pm_is_open || (repeat && input != PSX_MENU_UP && input != PSX_MENU_DOWN)) return;   /* key repeat only moves the cursor: a held key must not spam changes or settings.ini writes */
    a = psxrt_menu_input(&pm_m, input, psx_disc_tray_disc_count());
    pm_note[0] = 0;
    pm_dirty = 1;
    switch (a) {
    case MA_CLOSE: pm_close(); break;
    case MA_SAVE: pm_close(); psxrt_hk_run(HK_SAVE); break;
    case MA_LOAD: pm_close(); psxrt_hk_run(HK_LOAD_NOW); break;
    case MA_SWAP:
        r = psx_disc_tray_cycle(pm_m.disc_target, PSX_DISC_TRAY_ORIGIN_MENU);
        if (r == PSX_DISC_TRAY_OK) {
            snprintf(msg, sizeof msg, "Disc %d inserted", pm_m.disc_target);
            host_osd_push(msg, 2500);
            pm_close();
        } else {
            snprintf(pm_note, sizeof pm_note, "DISC CHANGE FAILED (%d)", r);
        }
        break;
    case MA_FULLSCREEN: pm_close(); psxrt_push_fullscreen(); break;
    case MA_GRAPHICS: gfx_refresh(); break;
    case MA_GFX: gfx_apply(); break;
    case MA_QUIT: pm_flush(1); psxrt_push_quit(); break;   /* the menu loop's SDL_QUIT branch shuts the runtime down */
    }
}
int psx_runtime_menu_selected_disc(void) { return pm_m.disc_target; }
int psx_runtime_menu_select_disc(int disc) { (void)disc; return 0; }
int psx_runtime_menu_insert(void) { return 0; }
struct RecompRuntimeUi *psx_runtime_menu_model(void) { return NULL; }
void psx_runtime_menu_set_fallback_enabled(int enabled) { if (!enabled) pm_close(); }
int psx_runtime_menu_overlay_image(const uint32_t **pixels, int *w, int *h)
{
    if (!pm_is_open) return 0;
    pm_flush(0);
    if (pm_dirty) pm_render();
    *pixels = pm_img;
    *w = SSM_W;
    *h = SSM_H;
    return 1;
}
