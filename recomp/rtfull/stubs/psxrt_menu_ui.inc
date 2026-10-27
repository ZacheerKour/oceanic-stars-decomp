/* Pause menu (F10 / Select+Start / Back+L3): drawing and actions. #included into a patched copy of psx_savestate_menu.c
 * (recomp/rtfull/CMakeLists.txt) so it can use that file's 8x8 font and rect helpers, and it replaces psx_runtime_menu.c,
 * whose non-launcher stubs left main.cpp's menu loop (pause guest and audio, keyboard + pad navigation) with nothing to
 * show. The state machine itself is psxrt_menu_input() in psxrt_io.c. */
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

int psxrt_pm_open(void) { return pm_is_open; }

static void pm_render(void)
{
    char buf[96];
    const int slot = psxrt_slot(), nd = psx_disc_tray_disc_count();
    int i;
    for (i = 0; i < SSM_W * SSM_H; i++) pm_img[i] = 0xFF0F1118u;
    fill_rect(pm_img, 0, 0, SSM_W, 56, 0xFF171B25u);
    draw_text(pm_img, 24, 20, "GAME MENU", 0xFFFFD24Du, 2);
    for (i = 0; i < PM_ROWS; i++) {
        const int y = 84 + i * 52, sel = (i == pm_m.sel);
        switch (i) {
        case PM_RESUME: snprintf(buf, sizeof buf, "RESUME"); break;
        case PM_SAVE: snprintf(buf, sizeof buf, "SAVE STATE  < SLOT %d >  %s", slot + 1, savestate_slot_exists(slot) ? "USED" : "EMPTY"); break;
        case PM_LOAD: snprintf(buf, sizeof buf, "LOAD STATE  < SLOT %d >  %s", slot + 1, savestate_slot_exists(slot) ? "USED" : "EMPTY"); break;
        case PM_SWAP:
            if (nd > 1) snprintf(buf, sizeof buf, "SWAP DISC  < DISC %d >  IN DRIVE %d", pm_m.disc_target, psx_disc_tray_mounted_disc());
            else snprintf(buf, sizeof buf, "SWAP DISC  (ONE DISC ONLY)");
            break;
        case PM_FULLSCREEN: snprintf(buf, sizeof buf, "TOGGLE FULLSCREEN"); break;
        default: snprintf(buf, sizeof buf, pm_m.quit_armed ? "QUIT? PRESS CROSS/ENTER AGAIN" : "QUIT GAME"); break;
        }
        fill_rect(pm_img, 24, y, SSM_W - 48, 44, sel ? 0xFF2B2830u : 0xFF191D27u);
        stroke_rect(pm_img, 24, y, SSM_W - 48, 44, sel ? 0xFFFFD24Du : 0xFF303746u);
        draw_text(pm_img, 40, y + 14, buf, sel ? (i == PM_QUIT && pm_m.quit_armed ? 0xFFFF6B6Bu : 0xFFFFD24Du) : 0xFFE2E5EBu, 2);
    }
    if (pm_note[0]) draw_text(pm_img, 24, 410, pm_note, 0xFFFF6B6Bu, 1);
    fill_rect(pm_img, 0, 430, SSM_W, 50, 0xFF171B25u);
    draw_text(pm_img, 24, 440, "UP/DOWN MOVE  LEFT/RIGHT CHANGE  CROSS/ENTER OK", 0xFFB8BDC8u, 1);
    draw_text(pm_img, 24, 456, "CIRCLE/ESC BACK  F10 OR SELECT+START CLOSES", 0xFFB8BDC8u, 1);
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
void psx_runtime_menu_close(void) { pm_is_open = 0; }
int psx_runtime_menu_is_open(void) { return pm_is_open; }
void psx_runtime_menu_input(int input, int repeat)
{
    int a, r;
    char msg[48];
    if (!pm_is_open || (repeat && (input == PSX_MENU_ACCEPT || input == PSX_MENU_BACK || input == PSX_MENU_TOGGLE))) return;
    a = psxrt_menu_input(&pm_m, input, psx_disc_tray_disc_count());
    pm_note[0] = 0;
    pm_dirty = 1;
    switch (a) {
    case MA_CLOSE: pm_is_open = 0; break;
    case MA_SAVE: pm_is_open = 0; psxrt_hk_run(HK_SAVE); break;
    case MA_LOAD: pm_is_open = 0; psxrt_hk_run(HK_LOAD); break;
    case MA_SWAP:
        r = psx_disc_tray_cycle(pm_m.disc_target, PSX_DISC_TRAY_ORIGIN_MENU);
        if (r == PSX_DISC_TRAY_OK) {
            snprintf(msg, sizeof msg, "Disc %d inserted", pm_m.disc_target);
            host_osd_push(msg, 2500);
            pm_is_open = 0;
        } else {
            snprintf(pm_note, sizeof pm_note, "DISC CHANGE FAILED (%d)", r);
        }
        break;
    case MA_FULLSCREEN: pm_is_open = 0; psxrt_push_fullscreen(); break;
    case MA_QUIT: psxrt_push_quit(); break;   /* the menu loop's SDL_QUIT branch shuts the runtime down */
    }
}
int psx_runtime_menu_selected_disc(void) { return pm_m.disc_target; }
int psx_runtime_menu_select_disc(int disc) { (void)disc; return 0; }
int psx_runtime_menu_insert(void) { return 0; }
struct RecompRuntimeUi *psx_runtime_menu_model(void) { return NULL; }
void psx_runtime_menu_set_fallback_enabled(int enabled) { if (!enabled) pm_is_open = 0; }
int psx_runtime_menu_overlay_image(const uint32_t **pixels, int *w, int *h)
{
    if (!pm_is_open) return 0;
    if (pm_dirty) pm_render();
    *pixels = pm_img;
    *w = SSM_W;
    *h = SSM_H;
    return 1;
}
