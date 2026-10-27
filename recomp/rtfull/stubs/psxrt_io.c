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
 * PSXRT_AUDIO_RMS=1: every ~5 s of device audio, log "[psxrt-audio] rms=<r> peak=<p> frames=<n>" (S16 stereo). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
enum { ACT_PAD, ACT_DUMP, ACT_SAVE, ACT_LOAD };
#define MAX_STEPS 8192
static struct { uint32_t frame; uint16_t word; uint8_t act; char arg[24]; } s_steps[MAX_STEPS];
static int s_nsteps = -1, s_cur = -1;
static uint32_t s_frame;

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

void psxrt_diag_frame(unsigned frame);   /* stubs/psxrt_diag.c */

void psxrt_input_script_apply(void) {
    uint32_t fr = s_frame++;
    psxrt_diag_frame(fr);
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
        }
    }
    if (i != s_cur) {
        s_cur = i;
        if (s_steps[i].act == ACT_PAD) fprintf(stderr, "[psxrt-input] frame %u pad 0x%04X\n", fr, s_steps[i].word);
    }
    if (i >= 0) sio_set_pad_state_slot(0, sio_get_pad_buttons_slot(0) & s_steps[i].word);
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
int main(void) {   /* cl /DPSXRT_IO_SELFTEST psxrt_io.c && psxrt_io.exe */
    char a[] = " CROSS START ", b[] = " 0xBFFF", c[] = " -", d[] = " BOGUS";
    uint16_t w;
    int ok = parse_word(a, &w) && w == (uint16_t)~((1u << 14) | (1u << 3));
    ok = ok && parse_word(b, &w) && w == 0xBFFF && parse_word(c, &w) && w == 0xFFFF && !parse_word(d, &w);
    puts(ok ? "selftest OK" : "selftest FAILED");
    return !ok;
}
uint16_t sio_get_pad_buttons_slot(int s) { (void)s; return 0xFFFF; }
void sio_set_pad_state_slot(int s, uint16_t b) { (void)s; (void)b; }
void psxrt_diag_frame(unsigned f) { (void)f; }
void psxrt_diag_dump_now(const char *t, unsigned f) { (void)t; (void)f; }
void psxrt_prof_start(void) {}
int savestate_request_save(int s) { return s; }
int savestate_request_load(int s) { return s; }
#endif
