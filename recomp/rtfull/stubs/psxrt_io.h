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
enum { PM_RESUME, PM_SAVE, PM_LOAD, PM_SWAP, PM_FULLSCREEN, PM_QUIT, PM_ROWS };
enum { MA_NONE, MA_CLOSE, MA_SAVE, MA_LOAD, MA_SWAP, MA_FULLSCREEN, MA_QUIT };
typedef struct { int sel, quit_armed, disc_target; } PsxrtMenu;
void psxrt_menu_reset(PsxrtMenu *m, int mounted_disc, int ndiscs);
int psxrt_menu_input(PsxrtMenu *m, int input, int ndiscs);
void psxrt_push_fullscreen(void);
void psxrt_push_quit(void);
#ifdef __cplusplus
}
#endif
