/* Scripted pad input and audio evidence for unattended runs (hooked into main.cpp / psx_sdl_audio.cpp by CMake patches).
 *
 * PSXRT_INPUT_SCRIPT=<file>: lines "<frame> <buttons...>" where frame counts pad samples (one per guest frame) and the
 * buttons (CROSS CIRCLE SQUARE TRIANGLE START SELECT UP DOWN LEFT RIGHT L1 L2 R1 R2 L3 R3, or a raw active-low hex word
 * 0xNNNN, or "-" for none) are held on port 1 from that frame until the next line. '#' starts a comment. Lines must be in
 * frame order. The scripted buttons are ANDed with live input, so the keyboard/gamepad keeps working.
 * PSXRT_INPUT_LOG=1: log "[psxrt-input] frame N live pad 0xNNNN <names>" whenever the live (keyboard/gamepad, before the
 * script) port-1 pad word changes: shows which PS1 bits a key or pad press produces.
 * PSXRT_VPAD_TEST=1: attach an SDL virtual gamepad at frame 120 (hot-plug), drive every button/stick/trigger on it and
 * log "[psxrt-vpad] PASS|FAIL <step>" against the expected PS1 bits, then detach it ("[psxrt-vpad] done fails=N").
 * Script verbs beyond pad words: "<frame> KEY F5|F6|F7|F9" runs the hotkey handler directly, "<frame> SDLKEY <name>" pushes a real
 * SDL key-down event (windowed runs only: the event loop does not run headless), "<frame> MENU open|up|down|left|right|
 * accept|back|close|shot_<tag>" drives the pause menu (open and close in the same frame: an open menu is modal).
 * Hotkeys (key -> action table below, tested by PSXRT_IO_SELFTEST): F5 save, F9 load, F6/F7 slot -/+ (slots 1-9); pad word
 * Select+R1 = save, Select+L1 = load (the three buttons are hidden from the game while the chord is held).
 * PSXRT_AUDIO_RMS=1: every ~5 s of device audio, log "[psxrt-audio] rms=<r> peak=<p> frames=<n>" (S16 stereo). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "psxrt_io.h"
#ifndef PSXRT_IO_SELFTEST
#include <SDL3/SDL.h>
#endif

void sio_set_pad_state_slot(int slot, uint16_t buttons);
uint16_t sio_get_pad_buttons_slot(int slot);

int savestate_request_save(int slot);
int savestate_request_load(int slot);
void psxrt_diag_dump_now(const char *tag, unsigned frame);   /* stubs/psxrt_diag.c */
void psxrt_prof_start(void);                                  /* stubs/psxrt_prof.c */

/* one-shot actions: "<frame> DUMP <tag>" (vram_<tag>.ppm), "<frame> SAVESTATE <slot>", "<frame> LOADSTATE <slot>";
 * they keep the pad word of the line before. LOADSTATE restores the guest only: the script frame counter runs on. */
enum { ACT_PAD, ACT_DUMP, ACT_SAVE, ACT_LOAD, ACT_KEY, ACT_SDLKEY, ACT_MENU };
#define MAX_STEPS 8192
static struct { uint32_t frame; uint16_t word; uint8_t act; char arg[24]; } s_steps[MAX_STEPS];
static int s_nsteps = -1, s_cur = -1;
static uint32_t s_frame;


/* ---- hotkeys, pad chord, pause-menu model: pure logic, no runtime calls (tested by PSXRT_IO_SELFTEST) ---- */
#define K_F5 0x4000003E   /* SDL3 keycodes: SDL_SCANCODE_TO_KEYCODE(58 + n - 1); checked against SDL below */
#define K_F6 0x4000003F
#define K_F7 0x40000040
#define K_F8 0x40000041
#define K_F9 0x40000042
#define K_F10 0x40000043
#define K_F12 0x40000045
#define MOD_BLOCK 0x0FC0   /* SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI: Alt/Ctrl+F5 etc. are not ours */
static int s_slot;   /* 0..HK_SLOTS-1, in memory only: a new start begins at slot 1 */

int psxrt_hk_action(int key, int mod, int repeat) {
    if (repeat || (mod & MOD_BLOCK)) return HK_NONE;
    switch (key) {
    case K_F5: return HK_SAVE;
    case K_F9: return HK_LOAD;
    case K_F6: return HK_SLOT_PREV;
    case K_F7: return HK_SLOT_NEXT;
    }
    return HK_NONE;
}
int psxrt_key_by_name(const char *n) {
    static const struct { const char *n; int k; } t[] = { {"F5", K_F5}, {"F6", K_F6}, {"F7", K_F7}, {"F8", K_F8}, {"F9", K_F9}, {"F10", K_F10}, {"F12", K_F12} };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) if (!_stricmp(n, t[i].n)) return t[i].k;
    return 0;
}
int psxrt_slot(void) { return s_slot; }
static void slot_step(int d) { s_slot = (s_slot + d + HK_SLOTS) % HK_SLOTS; }
void psxrt_slot_set(int slot) { s_slot = slot < 0 ? 0 : slot >= HK_SLOTS ? HK_SLOTS - 1 : slot; }

/* Pad chord on the final port-1 word (live keyboard/pad ANDed with the script): Select+R1 = save, Select+L1 = load, fired on the
 * press edge. While it is held Select, L1 and R1 are cleared from *out so the game does not also switch characters. */
int psxrt_chord_step(int *prev, uint16_t word, uint16_t *out) {
    const int sel = !(word & 1u), l1 = !(word & (1u << 10)), r1 = !(word & (1u << 11));
    const int now = sel && l1 != r1 ? (r1 ? HK_SAVE : HK_LOAD) : HK_NONE;
    *out = now ? (uint16_t)(word | 1u | (1u << 10) | (1u << 11)) : word;
    const int fire = now != *prev ? now : HK_NONE;
    *prev = now;
    return fire;
}

void psxrt_menu_reset(PsxrtMenu *m, int mounted, int ndiscs) {
    m->sel = 0;
    m->quit_armed = 0;
    m->disc_target = ndiscs > 1 ? (mounted == 1 ? 2 : 1) : 1;
}
int psxrt_menu_input(PsxrtMenu *m, int in, int nd) {
    const int row = m->sel;
    switch (in) {
    case PM_IN_TOGGLE: return MA_CLOSE;
    case PM_IN_BACK:   /* Esc / Circle: first backs out of a pending quit, then closes */
        if (m->quit_armed) { m->quit_armed = 0; return MA_NONE; }
        return MA_CLOSE;
    case PM_IN_UP: case PM_IN_DOWN:
        m->sel = (row + PM_ROWS + (in == PM_IN_DOWN ? 1 : -1)) % PM_ROWS;
        m->quit_armed = 0;
        return MA_NONE;
    case PM_IN_LEFT: case PM_IN_RIGHT: {
        const int d = in == PM_IN_RIGHT ? 1 : -1;
        if (row == PM_SAVE || row == PM_LOAD) slot_step(d);
        else if (row == PM_SWAP && nd > 1) m->disc_target = (m->disc_target - 1 + d + nd) % nd + 1;
        return MA_NONE;
    }
    case PM_IN_ACCEPT:
        switch (row) {
        case PM_RESUME: return MA_CLOSE;
        case PM_SAVE: return MA_SAVE;
        case PM_LOAD: return MA_LOAD;
        case PM_SWAP: return nd > 1 ? MA_SWAP : MA_NONE;
        case PM_FULLSCREEN: return MA_FULLSCREEN;
        default:   /* PM_QUIT: needs a second accept on the same row */
            if (!m->quit_armed) { m->quit_armed = 1; return MA_NONE; }
            m->quit_armed = 0;
            return MA_QUIT;
        }
    }
    return MA_NONE;
}

static const char *const k_names[16] = { "SELECT", "L3", "R3", "START", "UP", "RIGHT", "DOWN", "LEFT",
                                         "L2", "R2", "L1", "R1", "TRIANGLE", "CIRCLE", "CROSS", "SQUARE" };

static int parse_word(char *rest, uint16_t *out) {
    uint16_t w = 0xFFFFu;
    for (char *t = strtok(rest, " \t\r\n,+"); t; t = strtok(NULL, " \t\r\n,+")) {
        if (!strcmp(t, "-")) continue;
        if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) { w &= (uint16_t)strtoul(t, NULL, 16); continue; }
        int b = 0;
        while (b < 16 && _stricmp(t, k_names[b])) b++;
        if (b == 16) return 0;
        w &= (uint16_t)~(1u << b);
    }
    *out = w;
    return 1;
}

static void load_script(void) {
    s_nsteps = 0;
    setvbuf(stderr, NULL, _IONBF, 0);   /* runs are usually ended by a kill: do not lose the evidence lines */
    const char *path = getenv("PSXRT_INPUT_SCRIPT");
    if (!path || !*path) return;
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "[psxrt-input] cannot open %s\n", path); return; }
    char line[256];
    int ln = 0;
    while (fgets(line, sizeof line, f) && s_nsteps < MAX_STEPS) {
        ln++;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *end;
        unsigned long fr = strtoul(line, &end, 10);
        if (end == line) continue;   /* blank or comment-only line */
        uint16_t w = s_nsteps ? s_steps[s_nsteps - 1].word : 0xFFFFu;
        char verb[16] = "", arg[24] = "";
        int act = ACT_PAD;
        if (sscanf(end, " %15s %23s", verb, arg) >= 1) {
            if (!_stricmp(verb, "DUMP")) act = ACT_DUMP;
            else if (!_stricmp(verb, "SAVESTATE")) act = ACT_SAVE;
            else if (!_stricmp(verb, "LOADSTATE")) act = ACT_LOAD;
            else if (!_stricmp(verb, "KEY")) act = ACT_KEY;
            else if (!_stricmp(verb, "SDLKEY")) act = ACT_SDLKEY;
            else if (!_stricmp(verb, "MENU")) act = ACT_MENU;
        }
        if ((act == ACT_PAD && !parse_word(end, &w)) || (s_nsteps && fr < s_steps[s_nsteps - 1].frame)) {
            fprintf(stderr, "[psxrt-input] %s:%d: bad line, ignored\n", path, ln);
            continue;
        }
        s_steps[s_nsteps].frame = (uint32_t)fr;
        s_steps[s_nsteps].word = w;
        s_steps[s_nsteps].act = (uint8_t)act;
        memcpy(s_steps[s_nsteps].arg, arg, sizeof arg);
        s_nsteps++;
    }
    fclose(f);
    fprintf(stderr, "[psxrt-input] %d steps from %s\n", s_nsteps, path);
}


static void log_pad_names(char *out, size_t n, uint16_t w) {
    out[0] = 0;
    for (int b = 0; b < 16; b++)
        if (!(w & (1u << b))) { strncat(out, out[0] ? "+" : "", n - strlen(out) - 1); strncat(out, k_names[b], n - strlen(out) - 1); }
    if (!out[0]) strcpy(out, "-");
}

#ifndef PSXRT_IO_SELFTEST
/* Virtual gamepad self-test: SDL_GamepadButton/axis -> input.ini map -> pad word, through the real hot-plug path. */
enum { PAD_SEL = 1u << 0, PAD_L3 = 1u << 1, PAD_R3 = 1u << 2, PAD_START = 1u << 3, PAD_UP = 1u << 4, PAD_RIGHT = 1u << 5,
       PAD_DOWN = 1u << 6, PAD_LEFT = 1u << 7, PAD_L2 = 1u << 8, PAD_R2 = 1u << 9, PAD_L1 = 1u << 10, PAD_R1 = 1u << 11,
       PAD_TRI = 1u << 12, PAD_CIR = 1u << 13, PAD_CRO = 1u << 14, PAD_SQU = 1u << 15 };
typedef struct { const char *name; int button; int axis; int value; uint16_t down; int button2; } VStep;
static const VStep k_vsteps[] = {
    { "idle", -1, -1, 0, 0, -1 },
    { "A(south)->Cross", SDL_GAMEPAD_BUTTON_SOUTH, -1, 0, PAD_CRO, -1 },
    { "B(east)->Circle", SDL_GAMEPAD_BUTTON_EAST, -1, 0, PAD_CIR, -1 },
    { "X(west)->Square", SDL_GAMEPAD_BUTTON_WEST, -1, 0, PAD_SQU, -1 },
    { "Y(north)->Triangle", SDL_GAMEPAD_BUTTON_NORTH, -1, 0, PAD_TRI, -1 },
    { "LB->L1", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, -1, 0, PAD_L1, -1 },
    { "RB->R1", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, -1, 0, PAD_R1, -1 },
    { "LT->L2", -1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767, PAD_L2, -1 },
    { "RT->R2", -1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767, PAD_R2, -1 },
    { "Start->Start", SDL_GAMEPAD_BUTTON_START, -1, 0, PAD_START, -1 },
    { "Back->Select", SDL_GAMEPAD_BUTTON_BACK, -1, 0, PAD_SEL, -1 },
    { "LS click->L3", SDL_GAMEPAD_BUTTON_LEFT_STICK, -1, 0, PAD_L3, -1 },
    { "RS click->R3", SDL_GAMEPAD_BUTTON_RIGHT_STICK, -1, 0, PAD_R3, -1 },
    { "dpad up", SDL_GAMEPAD_BUTTON_DPAD_UP, -1, 0, PAD_UP, -1 },
    { "dpad down", SDL_GAMEPAD_BUTTON_DPAD_DOWN, -1, 0, PAD_DOWN, -1 },
    { "dpad left", SDL_GAMEPAD_BUTTON_DPAD_LEFT, -1, 0, PAD_LEFT, -1 },
    { "dpad right", SDL_GAMEPAD_BUTTON_DPAD_RIGHT, -1, 0, PAD_RIGHT, -1 },
    { "left stick up", -1, SDL_GAMEPAD_AXIS_LEFTY, -32767, PAD_UP, -1 },
    { "left stick down", -1, SDL_GAMEPAD_AXIS_LEFTY, 32767, PAD_DOWN, -1 },
    { "left stick left", -1, SDL_GAMEPAD_AXIS_LEFTX, -32767, PAD_LEFT, -1 },
    { "left stick right", -1, SDL_GAMEPAD_AXIS_LEFTX, 32767, PAD_RIGHT, -1 },
    { "left stick small drift (no input)", -1, SDL_GAMEPAD_AXIS_LEFTX, 4000, 0, -1 },
    { "left stick 60% right", -1, SDL_GAMEPAD_AXIS_LEFTX, 20000, PAD_RIGHT, -1 },
    { "right stick full (no input)", -1, SDL_GAMEPAD_AXIS_RIGHTX, 32767, 0, -1 },
    { "left stick left + A", -1, SDL_GAMEPAD_AXIS_LEFTX, -32767, PAD_LEFT | PAD_CRO, SDL_GAMEPAD_BUTTON_SOUTH },
};
#define VSTEP_FRAMES 12
#define VPAD_START 120

static void vpad_tick(uint32_t fr, uint16_t live) {
    static SDL_Joystick *joy;
    static int steps_done, fails, finished;
    static int pass_only = -1;
    if (pass_only < 0) { const char *e = getenv("PSXRT_VPAD_TEST"); pass_only = e && *e && *e != '0'; }
    if (!pass_only || finished || fr < VPAD_START) return;
    const int n = (int)(sizeof k_vsteps / sizeof k_vsteps[0]);
    uint32_t t = fr - VPAD_START;
    if (t == 0) {
        SDL_VirtualJoystickDesc d;
        SDL_INIT_INTERFACE(&d);
        d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        d.naxes = SDL_GAMEPAD_AXIS_COUNT;
        d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        d.name = "psxrt virtual gamepad";
        SDL_JoystickID id = SDL_AttachVirtualJoystick(&d);
        joy = id ? SDL_OpenJoystick(id) : NULL;
        fprintf(stderr, "[psxrt-vpad] attach id=%u joy=%p\n", (unsigned)id, (void *)joy);
        if (!joy) { finished = 1; return; }
        return;
    }
    uint32_t idx = (t - 1) / VSTEP_FRAMES, ph = (t - 1) % VSTEP_FRAMES;
    if (idx >= (uint32_t)n) {
        SDL_CloseJoystick(joy);
        fprintf(stderr, "[psxrt-vpad] done steps=%d fails=%d\n", steps_done, fails);
        finished = 1;
        return;
    }
    const VStep *s = &k_vsteps[idx];
    if (ph == 0) {
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) SDL_SetJoystickVirtualButton(joy, b, false);
        for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; a++) SDL_SetJoystickVirtualAxis(joy, a, (a == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || a == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) ? -32768 : 0);
        if (s->button >= 0) SDL_SetJoystickVirtualButton(joy, s->button, true);
        if (s->button2 >= 0) SDL_SetJoystickVirtualButton(joy, s->button2, true);
        if (s->axis >= 0) SDL_SetJoystickVirtualAxis(joy, s->axis, (Sint16)s->value);
    } else if (ph == VSTEP_FRAMES - 2) {   /* sample late in the step: hot-plug + event pump have settled */
        uint16_t want = (uint16_t)~s->down;
        char nm[96];
        log_pad_names(nm, sizeof nm, live);
        steps_done++;
        if (live != want) fails++;
        fprintf(stderr, "[psxrt-vpad] %s %s: live 0x%04X (%s) want 0x%04X\n", live == want ? "PASS" : "FAIL", s->name, live, nm, want);
    }
}
#else
static void vpad_tick(uint32_t fr, uint16_t live) { (void)fr; (void)live; }
#endif

#ifndef PSXRT_IO_SELFTEST
void host_osd_push(const char *msg, int duration_ms);
int psxrt_evlog_enabled(void);
void psxrt_evlog_request_dump(void);   /* stubs/psxrt_evlog.c */
int savestate_slot_exists(int slot);
int psx_runtime_menu_open(void);
void psx_runtime_menu_close(void);
void psx_runtime_menu_input(int input, int repeat);
int psx_runtime_menu_overlay_image(const uint32_t **pixels, int *w, int *h);
/* the key constants above must be SDL's */
typedef char psxrt_chk_keys[(K_F5 == SDLK_F5 && K_F6 == SDLK_F6 && K_F7 == SDLK_F7 && K_F8 == SDLK_F8 && K_F9 == SDLK_F9 &&
                             K_F10 == SDLK_F10 && (MOD_BLOCK & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) == (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) ? 1 : -1];

void psxrt_hk_run(int act) {
    char m[64];
    const int slot = s_slot;
    switch (act) {
    case HK_SAVE:   /* completion toast ("Saved slot N" / "Save failed") comes from the runtime's psx_frontend_on_savestate_notify */
        if (!savestate_request_save(slot)) { snprintf(m, sizeof m, "Slot %d: save not possible now", slot + 1); host_osd_push(m, 1800); }
        break;
    case HK_LOAD:
        if (!savestate_slot_exists(slot)) { snprintf(m, sizeof m, "Slot %d is empty", slot + 1); host_osd_push(m, 1800); }
        else if (!savestate_request_load(slot)) { snprintf(m, sizeof m, "Slot %d: load not possible now", slot + 1); host_osd_push(m, 1800); }
        break;
    case HK_SLOT_PREV: case HK_SLOT_NEXT:
        slot_step(act == HK_SLOT_NEXT ? 1 : -1);
        snprintf(m, sizeof m, "State slot %d (%s)", s_slot + 1, savestate_slot_exists(s_slot) ? "used" : "empty");
        host_osd_push(m, 1500);
        break;
    }
    fprintf(stderr, "[psxrt-hk] action %d slot %d\n", act, s_slot + 1);
}
int psxrt_hotkey_key(int key, int mod, int repeat) {   /* called from main.cpp's SDL key-down branch; 1 = consumed */
    if (key == K_F12 && psxrt_evlog_enabled() && !repeat && !(mod & MOD_BLOCK)) {   /* write the event log (stubs/psxrt_evlog.c) */
        psxrt_evlog_request_dump();
        host_osd_push("Event log saved (saves/logs)", 1500);
        return 1;
    }
    const int act = psxrt_hk_action(key, mod, repeat);
    if (act) psxrt_hk_run(act);
    return act != HK_NONE;
}
void psxrt_push_fullscreen(void) {   /* the menu's Fullscreen row: Alt+Enter reaches the same handler as the real keys */
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.down = true;
    e.key.key = SDLK_RETURN;
    e.key.scancode = SDL_SCANCODE_RETURN;
    e.key.mod = SDL_KMOD_LALT;
    SDL_PushEvent(&e);
}
void psxrt_push_quit(void) { SDL_Event e; SDL_zero(e); e.type = SDL_EVENT_QUIT; SDL_PushEvent(&e); }

static void push_sdl_key(const char *name) {
    const int k = psxrt_key_by_name(name);
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.down = true;
    e.key.key = (SDL_Keycode)k;
    e.key.scancode = SDL_GetScancodeFromKey((SDL_Keycode)k, NULL);
    fprintf(stderr, "[psxrt-input] SDLKEY %s (0x%X) push -> %d\n", name, k, k ? (int)SDL_PushEvent(&e) : -1);
}
static void write_menu_ppm(const char *tag) {
    const uint32_t *px;
    int w, h;
    char name[64];
    if (!psx_runtime_menu_overlay_image(&px, &w, &h)) { fprintf(stderr, "[psxrt-menu] shot %s: menu not open\n", tag); return; }
    snprintf(name, sizeof name, "menu_%s.ppm", tag);
    FILE *f = fopen(name, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) { uint8_t c[3] = { (uint8_t)(px[i] >> 16), (uint8_t)(px[i] >> 8), (uint8_t)px[i] }; fwrite(c, 1, 3, f); }
    fclose(f);
    fprintf(stderr, "[psxrt-menu] wrote %s\n", name);
}
static void menu_script(const char *c) {
    static const struct { const char *n; int in; } t[] = { {"up", PM_IN_UP}, {"down", PM_IN_DOWN}, {"left", PM_IN_LEFT}, {"right", PM_IN_RIGHT},
                                                           {"accept", PM_IN_ACCEPT}, {"back", PM_IN_BACK} };
    if (!_stricmp(c, "open")) { fprintf(stderr, "[psxrt-menu] open -> %d\n", psx_runtime_menu_open()); return; }
    if (!_stricmp(c, "close")) { psx_runtime_menu_close(); fprintf(stderr, "[psxrt-menu] close\n"); return; }
    if (!_strnicmp(c, "shot_", 5)) { write_menu_ppm(c + 5); return; }
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!_stricmp(c, t[i].n)) { psx_runtime_menu_input(t[i].in, 0); fprintf(stderr, "[psxrt-menu] input %s\n", c); return; }
    fprintf(stderr, "[psxrt-menu] unknown command %s\n", c);
}
static void chord_apply(void) {
    static int prev;
    const uint16_t w = sio_get_pad_buttons_slot(0);
    uint16_t o;
    const int fire = psxrt_chord_step(&prev, w, &o);
    if (o != w) sio_set_pad_state_slot(0, o);
    if (fire) psxrt_hk_run(fire);
}
#else
static void chord_apply(void) {}
static void menu_script(const char *c) { (void)c; }
static void push_sdl_key(const char *c) { (void)c; }
int psxrt_hotkey_key(int k, int m, int r) { (void)k; (void)m; (void)r; return 0; }
#endif

void psxrt_diag_frame(unsigned frame);   /* stubs/psxrt_diag.c */
void psxrt_evlog_frame(unsigned frame);  /* stubs/psxrt_evlog.c */

static void script_apply(void) {
    uint32_t fr = s_frame++;
    psxrt_diag_frame(fr);
    psxrt_evlog_frame(fr);
    if (s_nsteps < 0) { psxrt_prof_start(); load_script(); }
    {
        static int logon = -1;
        static uint16_t last = 0xFFFFu;
        uint16_t live = sio_get_pad_buttons_slot(0);
        if (logon < 0) { const char *e = getenv("PSXRT_INPUT_LOG"); logon = e && *e && *e != '0'; }
        if (logon && live != last) {
            char nm[96];
            log_pad_names(nm, sizeof nm, live);
            fprintf(stderr, "[psxrt-input] frame %u live pad 0x%04X %s\n", fr, live, nm);
        }
        last = live;
        vpad_tick(fr, live);
    }
    if (!s_nsteps) return;
    int i = s_cur;
    while (i + 1 < s_nsteps && s_steps[i + 1].frame <= fr) {
        i++;
        const char *a = s_steps[i].arg;
        switch (s_steps[i].act) {
        case ACT_DUMP: psxrt_diag_dump_now(a, fr); break;
        case ACT_SAVE: fprintf(stderr, "[psxrt-input] frame %u savestate %s -> %d\n", fr, a, savestate_request_save(atoi(a))); break;
        case ACT_LOAD: fprintf(stderr, "[psxrt-input] frame %u loadstate %s -> %d\n", fr, a, savestate_request_load(atoi(a))); break;
        case ACT_KEY: fprintf(stderr, "[psxrt-input] frame %u key %s -> %d\n", fr, a, psxrt_hotkey_key(psxrt_key_by_name(a), 0, 0)); break;
        case ACT_SDLKEY: push_sdl_key(a); break;
        case ACT_MENU: menu_script(a); break;
        }
    }
    if (i != s_cur) {
        s_cur = i;
        if (s_steps[i].act == ACT_PAD) fprintf(stderr, "[psxrt-input] frame %u pad 0x%04X\n", fr, s_steps[i].word);
    }
    if (i >= 0) sio_set_pad_state_slot(0, sio_get_pad_buttons_slot(0) & s_steps[i].word);
}

void psxrt_input_script_apply(void) {
    script_apply();
    chord_apply();
}

void psxrt_audio_tap(const void *buf, int bytes) {
    static int on = -1;
    static double sum;
    static int peak;
    static uint64_t n, total;
    if (on < 0) { const char *e = getenv("PSXRT_AUDIO_RMS"); on = e && *e && *e != '0'; }
    if (!on) return;
    const int16_t *s = (const int16_t *)buf;
    for (int i = 0; i < bytes / 2; i++) {
        int v = s[i];
        sum += (double)v * v;
        if (abs(v) > peak) peak = abs(v);
    }
    n += (uint64_t)(bytes / 2);
    if (n >= 44100u * 2u * 5u) {
        total += n / 2;
        fprintf(stderr, "[psxrt-audio] rms=%.1f peak=%d frames=%llu total=%llu\n", sqrt(sum / (double)n), peak,
                (unsigned long long)(n / 2), (unsigned long long)total);
        sum = 0; peak = 0; n = 0;
    }
}

#ifdef PSXRT_IO_SELFTEST
static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
int main(void) {   /* cl /DPSXRT_IO_SELFTEST psxrt_io.c && psxrt_io.exe */
    char a[] = " CROSS START ", b[] = " 0xBFFF", c[] = " -", d[] = " BOGUS";
    uint16_t w;
    CHECK(parse_word(a, &w) && w == (uint16_t)~((1u << 14) | (1u << 3)));
    CHECK(parse_word(b, &w) && w == 0xBFFF && parse_word(c, &w) && w == 0xFFFF && !parse_word(d, &w));
    /* key -> action */
    CHECK(psxrt_hk_action(K_F5, 0, 0) == HK_SAVE && psxrt_hk_action(K_F9, 0, 0) == HK_LOAD);
    CHECK(psxrt_hk_action(K_F6, 0, 0) == HK_SLOT_PREV && psxrt_hk_action(K_F7, 0, 0) == HK_SLOT_NEXT);
    CHECK(psxrt_hk_action(K_F5, 0x0001, 0) == HK_SAVE);                 /* shift is fine */
    CHECK(psxrt_hk_action(K_F5, 0x0040, 0) == HK_NONE);                 /* Ctrl+F5 */
    CHECK(psxrt_hk_action(K_F5, 0x0100, 0) == HK_NONE);                 /* Alt+F5 */
    CHECK(psxrt_hk_action(K_F5, 0, 1) == HK_NONE);                      /* key repeat must not re-save */
    CHECK(psxrt_hk_action(K_F8, 0, 0) == HK_NONE && psxrt_hk_action(K_F10, 0, 0) == HK_NONE);   /* F8 browser, F10 menu: not ours */
    CHECK(psxrt_hk_action(0x20, 0, 0) == HK_NONE);                      /* space */
    CHECK(psxrt_key_by_name("f9") == K_F9 && psxrt_key_by_name("F1") == 0);
    /* slots 1-9 wrap */
    CHECK(psxrt_slot() == 0);
    slot_step(-1); CHECK(psxrt_slot() == 8);
    slot_step(1); CHECK(psxrt_slot() == 0);
    psxrt_slot_set(99); CHECK(psxrt_slot() == 8); psxrt_slot_set(-3); CHECK(psxrt_slot() == 0);
    /* pad chord: edge-triggered, hides Select/L1/R1 while held */
    {
        const uint16_t none = 0xFFFF, sel = 0xFFFE, r1 = (uint16_t)~(1u << 11), l1 = (uint16_t)~(1u << 10);
        int prev = 0; uint16_t o;
        CHECK(psxrt_chord_step(&prev, none, &o) == HK_NONE && o == none);
        CHECK(psxrt_chord_step(&prev, sel, &o) == HK_NONE && o == sel);            /* Select alone reaches the game */
        CHECK(psxrt_chord_step(&prev, sel & r1, &o) == HK_SAVE && o == 0xFFFF);
        CHECK(psxrt_chord_step(&prev, sel & r1, &o) == HK_NONE && o == 0xFFFF);    /* held: no refire, still hidden */
        CHECK(psxrt_chord_step(&prev, none, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&prev, sel & l1, &o) == HK_LOAD);
        CHECK(psxrt_chord_step(&prev, sel & l1 & r1, &o) == HK_NONE && o == (uint16_t)(sel & l1 & r1));   /* both bumpers: no chord */
        CHECK(psxrt_chord_step(&prev, r1, &o) == HK_NONE && o == r1);              /* R1 alone (character switch) untouched */
        CHECK(psxrt_chord_step(&prev, (uint16_t)(sel & r1 & ~(1u << 14)), &o) == HK_SAVE && (o & (1u << 14)) == 0);   /* other buttons pass through */
    }
    /* pause menu: rows Resume, Save, Load, Swap, Fullscreen, Quit */
    {
        PsxrtMenu m;
        psxrt_menu_reset(&m, 1, 2);
        CHECK(m.sel == 0 && m.disc_target == 2);
        CHECK(psxrt_menu_input(&m, PM_IN_ACCEPT, 2) == MA_CLOSE);                  /* Resume */
        CHECK(psxrt_menu_input(&m, PM_IN_UP, 2) == MA_NONE && m.sel == PM_QUIT);   /* wraps to Quit */
        CHECK(psxrt_menu_input(&m, PM_IN_DOWN, 2) == MA_NONE && m.sel == PM_RESUME);
        CHECK(psxrt_menu_input(&m, PM_IN_BACK, 2) == MA_CLOSE && psxrt_menu_input(&m, PM_IN_TOGGLE, 2) == MA_CLOSE);
        psxrt_menu_input(&m, PM_IN_DOWN, 2);
        CHECK(m.sel == PM_SAVE && psxrt_menu_input(&m, PM_IN_RIGHT, 2) == MA_NONE && psxrt_slot() == 1);
        CHECK(psxrt_menu_input(&m, PM_IN_ACCEPT, 2) == MA_SAVE);
        psxrt_menu_input(&m, PM_IN_DOWN, 2);
        CHECK(m.sel == PM_LOAD && psxrt_menu_input(&m, PM_IN_LEFT, 2) == MA_NONE && psxrt_slot() == 0);
        CHECK(psxrt_menu_input(&m, PM_IN_ACCEPT, 2) == MA_LOAD);
        psxrt_menu_input(&m, PM_IN_DOWN, 2);
        CHECK(m.sel == PM_SWAP && psxrt_menu_input(&m, PM_IN_RIGHT, 2) == MA_NONE && m.disc_target == 1);
        CHECK(psxrt_menu_input(&m, PM_IN_RIGHT, 2) == MA_NONE && m.disc_target == 2 && psxrt_menu_input(&m, PM_IN_ACCEPT, 2) == MA_SWAP);
        CHECK(psxrt_menu_input(&m, PM_IN_ACCEPT, 1) == MA_NONE);                   /* one-disc game: swap does nothing */
        psxrt_menu_input(&m, PM_IN_DOWN, 2);
        CHECK(m.sel == PM_FULLSCREEN && psxrt_menu_input(&m, PM_IN_ACCEPT, 2) == MA_FULLSCREEN);
        psxrt_menu_input(&m, PM_IN_DOWN, 2);
        CHECK(m.sel == PM_QUIT && psxrt_menu_input(&m, PM_IN_ACCEPT, 2) == MA_NONE && m.quit_armed);   /* first accept only arms */
        CHECK(psxrt_menu_input(&m, PM_IN_BACK, 2) == MA_NONE && !m.quit_armed);    /* Circle backs out of the confirm, menu stays */
        psxrt_menu_input(&m, PM_IN_ACCEPT, 2);
        psxrt_menu_input(&m, PM_IN_UP, 2); psxrt_menu_input(&m, PM_IN_DOWN, 2);    /* moving away disarms */
        CHECK(!m.quit_armed && psxrt_menu_input(&m, PM_IN_ACCEPT, 2) == MA_NONE);
        CHECK(psxrt_menu_input(&m, PM_IN_ACCEPT, 2) == MA_QUIT && !m.quit_armed);  /* second accept quits */
        psxrt_menu_reset(&m, 2, 2); CHECK(m.disc_target == 1);
        psxrt_menu_reset(&m, 1, 1); CHECK(m.disc_target == 1);
    }
    puts(fails ? "selftest FAILED" : "selftest OK");
    return fails != 0;
}
uint16_t sio_get_pad_buttons_slot(int s) { (void)s; return 0xFFFF; }
void sio_set_pad_state_slot(int s, uint16_t b) { (void)s; (void)b; }
void psxrt_diag_frame(unsigned f) { (void)f; }
void psxrt_evlog_frame(unsigned f) { (void)f; }
void psxrt_diag_dump_now(const char *t, unsigned f) { (void)t; (void)f; }
void psxrt_prof_start(void) {}
int savestate_request_save(int s) { return s; }
int savestate_request_load(int s) { return s; }
#endif
