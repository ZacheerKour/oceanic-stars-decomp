/* Force-included into the patched main.cpp and psx_sdl_audio.cpp (CMake patches cannot contain ';'), and included by
 * psxrt_io.c and psxrt_menu_ui.inc. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void psxrt_input_script_apply(void);
void psxrt_audio_tap(const void *buf, int bytes);
uint32_t psxrt_occ_crc(uint32_t phys, int *valid);   /* stubs/psxrt_evlog.c */

/* Hotkeys (F5 save, F9 load, F6/F7 slot -/+) and the pad chord; see psxrt_io.c. */
enum { HK_NONE, HK_SAVE, HK_LOAD, HK_SLOT_PREV, HK_SLOT_NEXT };
#define HK_SLOTS 9
int psxrt_hotkey_key(int key, int mod, int repeat);   /* 1 = consumed */
void psxrt_hk_run(int act);
int psxrt_hk_action(int key, int mod, int repeat);
int psxrt_key_by_name(const char *name);
int psxrt_slot(void);
void psxrt_slot_set(int slot);
int psxrt_chord_step(int *prev, uint16_t word, uint16_t *out);
#define PSXRT_CONTINUE continue;   /* in a macro because the CMake patch strings cannot contain a semicolon */

/* Pause menu model (drawing and actions: psxrt_menu_ui.inc). Input codes equal the runtime's PsxRuntimeMenuInput. */
enum { PM_IN_TOGGLE, PM_IN_BACK, PM_IN_UP, PM_IN_DOWN, PM_IN_LEFT, PM_IN_RIGHT, PM_IN_ACCEPT };
enum { PM_RESUME, PM_SAVE, PM_LOAD, PM_SWAP, PM_FULLSCREEN, PM_GRAPHICS, PM_QUIT, PM_ROWS };
enum { MA_NONE, MA_CLOSE, MA_SAVE, MA_LOAD, MA_SWAP, MA_FULLSCREEN, MA_QUIT, MA_GRAPHICS, MA_GFX };
/* Graphics page: the current display values (window_mode, window_width, scale_mode, filter, render_scale, scanlines). */
enum { GR_WINMODE, GR_WINSIZE, GR_SCALE, GR_FILTER, GR_IRES, GR_SCAN, GR_BACK, GR_ROWS };
typedef struct { int fs, width, mode, linear, ires, scan; } PsxrtGfx;   /* fs 0/1, mode 0 fit 1 integer 2 stretch, linear 0/1, ires 1..8 */
typedef struct { int sel, quit_armed, disc_target, page, gsel; PsxrtGfx g; } PsxrtMenu;   /* page 0 main, 1 graphics */
void psxrt_menu_reset(PsxrtMenu *m, int mounted_disc, int ndiscs);
int psxrt_menu_input(PsxrtMenu *m, int input, int ndiscs);   /* MA_GFX: row m->gsel of m->g changed (apply, then psxrt_gfx_kv + psxrt_ini_persist) */
int psxrt_gfx_step(PsxrtGfx *g, int row, int dir);
int psxrt_gfx_kv(int row, const PsxrtGfx *g, char *key, char *val, int n);   /* settings.ini key/value of a row, 0 for Back */
extern const int psxrt_widths[5];
/* settings.ini rewrite: only the one value changes, comments/other keys/line ends stay; persist is atomic (temp + replace). */
int psxrt_ini_valid(const char *key, const char *val);
char *psxrt_ini_set(const char *text, const char *key, const char *val);   /* malloc'd, NULL if invalid */
int psxrt_ini_persist(const char *key, const char *val);   /* PSXRT_SETTINGS_INI: 1 saved, 0 failed, -1 no file configured */
int psxrt_launch_fullscreen(void);
int psxrt_scale_mode(void);
void psxrt_scale_mode_set(int mode);
/* main.cpp accessors (psxrt_video.h PSXRT_MAIN_ACCESSORS) */
int psxrt_gfx_fullscreen_get(void);
int psxrt_gfx_fullscreen_set(int on);
int psxrt_gfx_window_width(void);
int psxrt_gfx_window_resize(int w);
int psxrt_gfx_filter_get(void);
void psxrt_gfx_filter_set(int linear);
void psxrt_gfx_redraw(void);
int psxrt_gfx_scale_get(void);
int psxrt_gfx_scale_set(int n);
void psxrt_push_fullscreen(void);
void psxrt_push_quit(void);
#ifdef __cplusplus
}
#endif
