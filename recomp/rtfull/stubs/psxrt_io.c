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
 * Select+R1 = save, Select+L1 = load held 450 ms (the three buttons are hidden from the game while the chord is held). A load from
 * F9 or the chord asks a second press within 3 s (PSXRT_CONFIRM_QUICKLOAD=0, settings.ini confirm_quickload = 0, turns it off).
 * PSXRT_AUDIO_RMS=1: every ~5 s of device audio, log "[psxrt-audio] rms=<r> peak=<p> frames=<n>" (S16 stereo). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "psxrt_io.h"
#include <windows.h>
#include <io.h>
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
static int s_ld_slot = -1;   /* slot of a quick load waiting for its confirming second press */
static uint32_t s_ld_t;
static void slot_step(int d) { s_slot = (s_slot + d + HK_SLOTS) % HK_SLOTS; s_ld_slot = -1; }
void psxrt_slot_set(int slot) { s_slot = slot < 0 ? 0 : slot >= HK_SLOTS ? HK_SLOTS - 1 : slot; s_ld_slot = -1; }
uint32_t psxrt_now_ms(void) { return (uint32_t)GetTickCount64(); }
/* 1 = go ahead with the quick load now; 0 = armed, the player must ask again within 3 s for the same slot */
int psxrt_load_gate(int slot, uint32_t now, int confirm) {
    if (!confirm) return 1;
    if (s_ld_slot == slot && now - s_ld_t <= 3000u) { s_ld_slot = -1; return 1; }
    s_ld_slot = slot; s_ld_t = now;
    return 0;
}

/* Pad chord on the final port-1 word (live keyboard/pad ANDed with the script). Select+R1 = save (fires on the press edge), Select+L1 =
 * load (fires once after CHORD_LOAD_MS held). Only a clean chord counts: exactly those two buttons held, reached from fewer buttons.
 * Any extra button, all three of Select/L1/R1, or releasing one of the three while another stays down marks the chord dirty until
 * Select, L1 and R1 are all up, so releasing a bigger chord (or Shift+Shift typed in passing) cannot turn into a save/load.
 * While the chord is armed Select, L1 and R1 are cleared from *out so the game does not also switch characters. */
#define CH_SEL 1u
#define CH_L1 (1u << 10)
#define CH_R1 (1u << 11)
#define CH_ALL (CH_SEL | CH_L1 | CH_R1)
#define CHORD_LOAD_MS 450u
int psxrt_chord_step(PsxrtChord *c, uint16_t word, uint32_t now, uint16_t *out) {
    const uint16_t held = (uint16_t)~word, m = held & CH_ALL;
    int cand = HK_NONE, fire = HK_NONE;
    if (!m) c->dirty = 0;
    else if ((held & ~CH_ALL) || (c->pm & ~m) || m == CH_ALL) c->dirty = 1;
    if (!c->dirty) cand = m == (CH_SEL | CH_R1) ? HK_SAVE : m == (CH_SEL | CH_L1) ? HK_LOAD : HK_NONE;
    if (cand != c->cand) { c->cand = cand; c->t0 = now; c->fired = 0; }
    if (cand && !c->fired && (cand == HK_SAVE || now - c->t0 >= CHORD_LOAD_MS)) { c->fired = 1; fire = cand; }
    c->pm = m;
    *out = cand ? (uint16_t)(word | CH_ALL) : word;
    return fire;
}

/* ---- Graphics page model ---- */
const int psxrt_widths[5] = { 640, 960, 1440, 1920, 2880 };   /* 4:3 windows 480p 720p 1080p 1440p 2160p */
static const char *const k_modes[3] = { "fit", "integer", "stretch" };

int psxrt_gfx_step(PsxrtGfx *g, int row, int d) {   /* d = +1 / -1; 1 when a value changed */
    int i;
    switch (row) {
    case GR_WINMODE: g->fs = !g->fs; return 1;
    case GR_WINSIZE: {
        int w = g->width;
        if (d > 0) { for (i = 0; i < 5 && psxrt_widths[i] <= w; i++) {} w = psxrt_widths[i % 5]; }
        else { for (i = 4; i >= 0 && psxrt_widths[i] >= w; i--) {} w = psxrt_widths[i < 0 ? 4 : i]; }
        g->width = w;
        return 1;
    }
    case GR_SCALE: g->mode = (g->mode + d + 3) % 3; return 1;
    case GR_FILTER: g->linear = !g->linear; return 1;
    case GR_IRES: g->ires = (g->ires - 1 + d + 8) % 8 + 1; return 1;
    case GR_SCAN: g->scan = !g->scan; return 1;
    }
    return 0;
}
int psxrt_gfx_kv(int row, const PsxrtGfx *g, char *key, char *val, int n) {
    const char *k, *v;
    char b[16];
    switch (row) {
    case GR_WINMODE: k = "window_mode"; v = g->fs ? "borderless" : "windowed"; break;
    case GR_WINSIZE: k = "window_width"; snprintf(b, sizeof b, "%d", g->width); v = b; break;
    case GR_SCALE: k = "scale_mode"; v = k_modes[g->mode]; break;
    case GR_FILTER: k = "filter"; v = g->linear ? "linear" : "nearest"; break;
    case GR_IRES: k = "render_scale"; snprintf(b, sizeof b, "%d", g->ires); v = b; break;
    case GR_SCAN: k = "scanlines"; v = g->scan ? "on" : "off"; break;
    default: return 0;
    }
    snprintf(key, (size_t)n, "%s", k);
    snprintf(val, (size_t)n, "%s", v);
    return 1;
}

/* ---- settings.ini rewrite (same line grammar as recomp/launcher/play.c: key = value, key case-insensitive, last one wins) ---- */
static int all_digits(const char *v, int maxlen) { int n = (int)strlen(v); if (!n || n > maxlen) return 0; for (; *v; v++) if (*v < '0' || *v > '9') return 0; return 1; }
int psxrt_ini_valid(const char *k, const char *v) {   /* the ranges play.c accepts, so a persisted value never trips its fallback */
    if (!_stricmp(k, "window_width")) return all_digits(v, 4) && v[0] != '0' && atoi(v) >= 640 && atoi(v) <= 7680;
    if (!_stricmp(k, "window_height")) return !strcmp(v, "0") || (all_digits(v, 4) && v[0] != '0' && atoi(v) >= 360 && atoi(v) <= 4320);   /* 0 = 4:3 from window_width */
    if (!_stricmp(k, "side_fill")) return !strcmp(v, "off") || !strcmp(v, "blur") || !strcmp(v, "art");
    if (!_stricmp(k, "render_scale")) return all_digits(v, 1) && v[0] >= '1' && v[0] <= '8';
    if (!_stricmp(k, "scale_mode")) return !strcmp(v, "fit") || !strcmp(v, "integer") || !strcmp(v, "stretch");
    if (!_stricmp(k, "filter")) return !strcmp(v, "linear") || !strcmp(v, "nearest");
    if (!_stricmp(k, "scanlines")) return !strcmp(v, "on") || !strcmp(v, "off");
    if (!_stricmp(k, "window_mode")) return !strcmp(v, "windowed") || !strcmp(v, "borderless");
    if (!_stricmp(k, "hint_shown")) return !strcmp(v, "1");
    return 0;
}
char *psxrt_ini_set(const char *t, const char *key, const char *val) {
    if (!psxrt_ini_valid(key, val)) return NULL;
    const size_t kl = strlen(key), tl = strlen(t);
    const char *hit = NULL, *hit_end = NULL, *hit_eol = NULL;   /* last matching line: text through its '=' is kept, its value replaced */
    const char *p = t;
    while (*p) {
        const char *e = p; while (*e && *e != '\n' && *e != '\r') e++;
        const char *c = p; while (c < e && (*c == ' ' || *c == '\t')) c++;
        if ((size_t)(e - c) > kl && !_strnicmp(c, key, kl)) {
            const char *q = c + kl; while (q < e && (*q == ' ' || *q == '\t')) q++;
            if (q < e && *q == '=') { hit = p; hit_end = q + 1; hit_eol = e; }
        }
        p = e; if (*p == '\r' && p[1] == '\n') p++; if (*p) p++;
    }
    char *o = (char *)malloc(tl + kl + strlen(val) + 16);
    if (!o) return NULL;
    if (hit) {
        size_t n = (size_t)(hit_end - t);
        memcpy(o, t, n);
        n += (size_t)sprintf(o + n, " %s", val);
        strcpy(o + n, hit_eol);
    } else {
        const char *eol = strstr(t, "\r\n") ? "\r\n" : "\n";
        memcpy(o, t, tl);
        sprintf(o + tl, "%s%s = %s%s", tl && t[tl - 1] != '\n' ? eol : "", key, val, eol);
    }
    return o;
}
int psxrt_ini_persist(const char *key, const char *val) {
    const wchar_t *path = _wgetenv(L"PSXRT_SETTINGS_INI");   /* wide: a game folder with non-ANSI characters must still open */
    if (!path || !*path) return -1;
    FILE *f = _wfopen(path, L"rb");
    if (!f) return 0;
    char *buf = (char *)malloc(1 << 20);
    size_t n = buf ? fread(buf, 1, (1 << 20) - 1, f) : 0;
    const int big = buf && !feof(f);
    fclose(f);
    if (!buf || big || !n) { free(buf); return 0; }
    const unsigned char *u = (const unsigned char *)buf;   /* UTF-16 (BOM or NULs): this ANSI/UTF-8 rewrite would truncate it, so refuse */
    if ((n >= 2 && ((u[0] == 0xFF && u[1] == 0xFE) || (u[0] == 0xFE && u[1] == 0xFF))) || memchr(buf, 0, n)) { free(buf); return -2; }
    buf[n] = 0;
    char *out = psxrt_ini_set(buf, key, val);
    free(buf);
    if (!out) return 0;
    wchar_t *tmp = (wchar_t *)malloc((wcslen(path) + 5) * sizeof(wchar_t));
    if (!tmp) { free(out); return 0; }
    wcscpy(tmp, path);
    wcscat(tmp, L".tmp");
    const size_t on = strlen(out);
    int ok = 0;
    f = _wfopen(tmp, L"wb");
    if (f) {
        ok = fwrite(out, 1, on, f) == on && fflush(f) == 0 && FlushFileBuffers((HANDLE)_get_osfhandle(_fileno(f))) != 0;
        ok = fclose(f) == 0 && ok;
    }
    free(out);
    if (ok) ok = MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) _wremove(tmp);   /* only our own temp file */
    free(tmp);
    return ok;
}

/* Debounced writes for the menu: the latest value per key is held and written 400 ms after the last change (or at once when forced:
 * menu close/quit), so a burst of changes is one rewrite per key instead of one per press. */
static struct { char k[32], v[32]; } s_pq[8];
static int s_pn;
static uint32_t s_pq_t;
void psxrt_ini_queue(const char *key, const char *val, uint32_t now) {
    int i;
    for (i = 0; i < s_pn && strcmp(s_pq[i].k, key); i++) {}
    if (i == 8) { psxrt_ini_flush(now, 1); i = 0; }
    snprintf(s_pq[i].k, sizeof s_pq[i].k, "%s", key);
    snprintf(s_pq[i].v, sizeof s_pq[i].v, "%s", val);
    if (i == s_pn) s_pn++;
    s_pq_t = now;
}
int psxrt_ini_flush(uint32_t now, int force) {   /* 2 = nothing written yet, else the last non-1 persist result (1 when all saved) */
    int r = 1;
    if (!s_pn || (!force && now - s_pq_t < 400u)) return 2;
    for (int i = 0; i < s_pn; i++) { const int p = psxrt_ini_persist(s_pq[i].k, s_pq[i].v); if (p != 1) r = p; }
    s_pn = 0;
    return r;
}

static int s_scale_mode = -1, s_fs_launch = -1;
int psxrt_scale_mode(void) {   /* 0 fit, 1 integer, 2 stretch: from PSXRT_SCALE_MODE until the menu changes it */
    if (s_scale_mode < 0) {
        const char *e = getenv("PSXRT_SCALE_MODE");
        s_scale_mode = !e ? 0 : !strcmp(e, "integer") ? 1 : !strcmp(e, "stretch") ? 2 : 0;
    }
    return s_scale_mode;
}
void psxrt_scale_mode_set(int m) { s_scale_mode = m < 0 || m > 2 ? 0 : m; }
int psxrt_launch_fullscreen(void) {   /* PSXRT_FULLSCREEN=1 (settings.ini window_mode = borderless); never for a hidden test window */
    if (s_fs_launch < 0) {
        const char *e = getenv("PSXRT_FULLSCREEN"), *h = getenv("PSX_HIDDEN_WINDOW");
        s_fs_launch = e && !strcmp(e, "1") && !(h && *h && *h != '0');
    }
    return s_fs_launch;
}

void psxrt_menu_reset(PsxrtMenu *m, int mounted, int ndiscs) {
    m->sel = 0;
    m->quit_armed = 0;
    m->page = 0;
    m->gsel = 0;
    m->disc_target = ndiscs > 1 ? (mounted == 1 ? 2 : 1) : 1;
}
static int gfx_menu_input(PsxrtMenu *m, int in) {
    const int row = m->gsel;
    switch (in) {
    case PM_IN_TOGGLE: return MA_CLOSE;
    case PM_IN_BACK: m->page = 0; return MA_NONE;
    case PM_IN_UP: case PM_IN_DOWN: m->gsel = (row + GR_ROWS + (in == PM_IN_DOWN ? 1 : -1)) % GR_ROWS; return MA_NONE;
    case PM_IN_LEFT: case PM_IN_RIGHT: case PM_IN_ACCEPT:
        if (row == GR_BACK) { if (in == PM_IN_ACCEPT) m->page = 0; return MA_NONE; }
        return psxrt_gfx_step(&m->g, row, in == PM_IN_LEFT ? -1 : 1) ? MA_GFX : MA_NONE;
    }
    return MA_NONE;
}
int psxrt_menu_input(PsxrtMenu *m, int in, int nd) {
    const int row = m->sel;
    if (m->page == 1) return gfx_menu_input(m, in);
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
        case PM_GRAPHICS: m->page = 1; m->gsel = 0; return MA_GRAPHICS;
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

static int confirm_quickload(void) { const char *e = getenv("PSXRT_CONFIRM_QUICKLOAD"); return !(e && *e == '0'); }   /* settings.ini confirm_quickload */
void psxrt_hk_run(int act) {
    char m[64];
    const int slot = s_slot;
    switch (act) {
    case HK_SAVE:   /* completion toast ("Saved slot N" / "Save failed") comes from the runtime's psx_frontend_on_savestate_notify */
        if (!savestate_request_save(slot)) { snprintf(m, sizeof m, "Slot %d: save not possible now", slot + 1); host_osd_push(m, 1800); }
        break;
    case HK_LOAD: case HK_LOAD_NOW:   /* _NOW: the pause menu's own Load row, already a deliberate choice */
        if (!savestate_slot_exists(slot)) { snprintf(m, sizeof m, "Slot %d is empty", slot + 1); host_osd_push(m, 1800); }
        else if (act == HK_LOAD && !psxrt_load_gate(slot, psxrt_now_ms(), confirm_quickload())) {
            snprintf(m, sizeof m, "Slot %d: press load again within 3 s to confirm", slot + 1); host_osd_push(m, 2500);
        }
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
    static PsxrtChord ch;
    const uint16_t w = sio_get_pad_buttons_slot(0);
    uint16_t o;
    const int fire = psxrt_chord_step(&ch, w, psxrt_now_ms(), &o);
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
#ifndef PSXRT_IO_SELFTEST
    if (fr == 600) {   /* first start only: play.c sets PSXRT_HINT until settings.ini has hint_shown = 1 */
        const char *h = getenv("PSXRT_HINT");
        if (h && *h == '1') { host_osd_push("F10 or Select+Start = menu (Graphics options inside)", 6000); psxrt_ini_persist("hint_shown", "1"); }
    }
#endif
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
    /* pad chord: clean transitions only; save on the press edge, load after 450 ms held; hides Select/L1/R1 while armed */
    {
        const uint16_t none = 0xFFFF, sel = 0xFFFE, r1 = (uint16_t)~(1u << 11), l1 = (uint16_t)~(1u << 10), cr = (uint16_t)~(1u << 14);
        PsxrtChord c; uint16_t o; uint32_t t = 1000;
        memset(&c, 0, sizeof c);
        CHECK(psxrt_chord_step(&c, none, t, &o) == HK_NONE && o == none);
        CHECK(psxrt_chord_step(&c, sel, t += 16, &o) == HK_NONE && o == sel);          /* Select alone reaches the game */
        CHECK(psxrt_chord_step(&c, sel & r1, t += 16, &o) == HK_SAVE && o == 0xFFFF);
        CHECK(psxrt_chord_step(&c, sel & r1, t += 16, &o) == HK_NONE && o == 0xFFFF);  /* held: no refire, still hidden */
        CHECK(psxrt_chord_step(&c, none, t += 16, &o) == HK_NONE && o == none);
        CHECK(psxrt_chord_step(&c, r1, t += 16, &o) == HK_NONE && o == r1);            /* R1 alone (character switch) untouched */
        CHECK(psxrt_chord_step(&c, none, t += 16, &o) == HK_NONE);
        /* load: needs the hold, fires once */
        CHECK(psxrt_chord_step(&c, sel & l1, t += 16, &o) == HK_NONE && o == 0xFFFF);
        CHECK(psxrt_chord_step(&c, sel & l1, t + 400, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, sel & l1, t + 450, &o) == HK_LOAD && o == 0xFFFF);
        CHECK(psxrt_chord_step(&c, sel & l1, t + 2000, &o) == HK_NONE);                /* once only */
        CHECK(psxrt_chord_step(&c, none, t + 2016, &o) == HK_NONE);
        /* a short tap (Shift+Shift typed in passing) never loads */
        t += 3000;
        CHECK(psxrt_chord_step(&c, sel & l1, t, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, sel & l1, t + 300, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, none, t + 316, &o) == HK_NONE && o == none);
        CHECK(psxrt_chord_step(&c, none, t + 2000, &o) == HK_NONE);
        /* rolling: all three held, then R1 released -> Select+L1 left, which must not load (even held for seconds) */
        t += 5000;
        CHECK(psxrt_chord_step(&c, sel & l1 & r1, t, &o) == HK_NONE && o == (uint16_t)(sel & l1 & r1));   /* both bumpers: no chord, nothing hidden */
        CHECK(psxrt_chord_step(&c, sel & l1, t + 16, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, sel & l1, t + 3000, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, none, t + 3016, &o) == HK_NONE);
        /* ... nor a save from Select+R1 left after releasing L1 */
        t += 9000;
        CHECK(psxrt_chord_step(&c, sel & l1 & r1, t, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, sel & r1, t + 16, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, none, t + 32, &o) == HK_NONE);
        /* one-frame swap Select+R1 -> Select+L1 is not a clean press */
        t += 9000;
        CHECK(psxrt_chord_step(&c, sel & r1, t, &o) == HK_SAVE);
        CHECK(psxrt_chord_step(&c, sel & l1, t + 16, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, sel & l1, t + 3000, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, none, t + 3016, &o) == HK_NONE);
        /* an extra button blocks the chord and passes through untouched; releasing it is not a clean press; a clean retry works */
        t += 9000;
        CHECK(psxrt_chord_step(&c, (uint16_t)(sel & r1 & cr), t, &o) == HK_NONE && o == (uint16_t)(sel & r1 & cr));
        CHECK(psxrt_chord_step(&c, sel & r1, t + 16, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, none, t + 32, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, sel & r1, t + 48, &o) == HK_SAVE);
        CHECK(psxrt_chord_step(&c, (uint16_t)(sel & r1 & cr), t + 64, &o) == HK_NONE && o == (uint16_t)(sel & r1 & cr));   /* cross added while held: chord drops */
        CHECK(psxrt_chord_step(&c, none, t + 80, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, (uint16_t)(sel & l1 & cr), t + 4000, &o) == HK_NONE);
        CHECK(psxrt_chord_step(&c, (uint16_t)(sel & l1 & cr), t + 9000, &o) == HK_NONE);
    }
    /* quick-load confirmation: first request arms, a second within 3 s for the same slot goes, slot change or timeout re-arms */
    {
        psxrt_slot_set(0);
        CHECK(psxrt_load_gate(0, 10000, 0) == 1);                                      /* confirmation switched off */
        CHECK(psxrt_load_gate(0, 10000, 1) == 0 && psxrt_load_gate(0, 12900, 1) == 1);
        CHECK(psxrt_load_gate(0, 12950, 1) == 0 && psxrt_load_gate(0, 16000, 1) == 0); /* too late: armed again */
        CHECK(psxrt_load_gate(1, 16100, 1) == 0 && psxrt_load_gate(0, 16200, 1) == 0); /* other slot does not confirm */
        psxrt_slot_set(0);
        CHECK(psxrt_load_gate(0, 17000, 1) == 0);
        slot_step(1); slot_step(-1);                                                   /* a slot step cancels the pending load */
        CHECK(psxrt_load_gate(0, 17100, 1) == 0);
        CHECK(psxrt_load_gate(0, 0xFFFFFF00u, 1) == 0 && psxrt_load_gate(0, 0x100u, 1) == 1);   /* tick counter wrap */
        psxrt_slot_set(0);
    }
    /* pause menu: rows Resume, Save, Load, Swap, Fullscreen, Graphics, Quit */
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
        CHECK(m.sel == PM_GRAPHICS && psxrt_menu_input(&m, PM_IN_ACCEPT, 2) == MA_GRAPHICS && m.page == 1 && m.gsel == 0);
        CHECK(psxrt_menu_input(&m, PM_IN_BACK, 2) == MA_NONE && m.page == 0 && m.sel == PM_GRAPHICS);   /* back returns to the main page */
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
    /* graphics page: navigation, value cycling, settings.ini key/value pairs */
    {
        PsxrtMenu m; char k[32], v[32];
        psxrt_menu_reset(&m, 1, 1);
        m.sel = PM_GRAPHICS; psxrt_menu_input(&m, PM_IN_ACCEPT, 1);
        m.g.fs = 0; m.g.width = 960; m.g.mode = 0; m.g.linear = 1; m.g.ires = 1; m.g.scan = 0;
        CHECK(psxrt_menu_input(&m, PM_IN_UP, 1) == MA_NONE && m.gsel == GR_BACK);          /* wraps */
        CHECK(psxrt_menu_input(&m, PM_IN_ACCEPT, 1) == MA_NONE && m.page == 0);            /* Back row leaves */
        m.sel = PM_GRAPHICS; psxrt_menu_input(&m, PM_IN_ACCEPT, 1);
        CHECK(psxrt_menu_input(&m, PM_IN_ACCEPT, 1) == MA_GFX && m.g.fs == 1);             /* accept cycles */
        CHECK(psxrt_gfx_kv(GR_WINMODE, &m.g, k, v, 32) && !strcmp(k, "window_mode") && !strcmp(v, "borderless"));
        CHECK(psxrt_menu_input(&m, PM_IN_LEFT, 1) == MA_GFX && m.g.fs == 0);
        psxrt_menu_input(&m, PM_IN_DOWN, 1);
        CHECK(psxrt_menu_input(&m, PM_IN_RIGHT, 1) == MA_GFX && m.g.width == 1440);
        psxrt_menu_input(&m, PM_IN_RIGHT, 1); psxrt_menu_input(&m, PM_IN_RIGHT, 1); CHECK(m.g.width == 2880);
        psxrt_menu_input(&m, PM_IN_RIGHT, 1); CHECK(m.g.width == 640);                      /* wraps */
        psxrt_menu_input(&m, PM_IN_LEFT, 1); CHECK(m.g.width == 2880);
        m.g.width = 1000; psxrt_gfx_step(&m.g, GR_WINSIZE, 1); CHECK(m.g.width == 1440);   /* custom size -> neighbours */
        m.g.width = 1000; psxrt_gfx_step(&m.g, GR_WINSIZE, -1); CHECK(m.g.width == 960);
        psxrt_menu_input(&m, PM_IN_DOWN, 1);
        psxrt_menu_input(&m, PM_IN_LEFT, 1); CHECK(m.g.mode == 2 && psxrt_gfx_kv(GR_SCALE, &m.g, k, v, 32) && !strcmp(v, "stretch"));
        psxrt_menu_input(&m, PM_IN_RIGHT, 1); CHECK(m.g.mode == 0);
        psxrt_menu_input(&m, PM_IN_DOWN, 1); psxrt_menu_input(&m, PM_IN_ACCEPT, 1);
        CHECK(!m.g.linear && psxrt_gfx_kv(GR_FILTER, &m.g, k, v, 32) && !strcmp(k, "filter") && !strcmp(v, "nearest"));
        psxrt_menu_input(&m, PM_IN_DOWN, 1);
        psxrt_menu_input(&m, PM_IN_LEFT, 1); CHECK(m.g.ires == 8);                          /* 1 wraps to 8 */
        CHECK(psxrt_gfx_kv(GR_IRES, &m.g, k, v, 32) && !strcmp(k, "render_scale") && !strcmp(v, "8"));
        psxrt_menu_input(&m, PM_IN_RIGHT, 1); CHECK(m.g.ires == 1);
        psxrt_menu_input(&m, PM_IN_DOWN, 1); psxrt_menu_input(&m, PM_IN_ACCEPT, 1);
        CHECK(m.g.scan == 1 && psxrt_gfx_kv(GR_SCAN, &m.g, k, v, 32) && !strcmp(k, "scanlines") && !strcmp(v, "on"));
        CHECK(!psxrt_gfx_kv(GR_BACK, &m.g, k, v, 32));
        CHECK(psxrt_menu_input(&m, PM_IN_TOGGLE, 1) == MA_CLOSE);
        /* every width the page can produce is valid for play.c */
        for (int i = 0; i < 5; i++) { snprintf(v, sizeof v, "%d", psxrt_widths[i]); CHECK(psxrt_ini_valid("window_width", v)); }
    }
    /* settings.ini rewrite */
    {
        const char *src = "; PLAY settings\r\n[play]\r\nbios = auto\r\n; window_width = 111\r\n  Window_Width = 960  \r\nfilter=linear\r\nwindow_width = 1280\r\nkey.up = W";
        char *o = psxrt_ini_set(src, "window_width", "1440");
        CHECK(o && strstr(o, "; window_width = 111\r\n") && strstr(o, "  Window_Width = 960  \r\n"));   /* comment and the earlier line stay */
        CHECK(o && strstr(o, "\r\nwindow_width = 1440\r\nkey.up = W") && !strstr(o, "1280"));              /* last one wins, CRLF kept */
        free(o);
        o = psxrt_ini_set(src, "filter", "nearest");
        CHECK(o && strstr(o, "\r\nfilter= nearest\r\n") && strstr(o, "bios = auto\r\n"));
        free(o);
        o = psxrt_ini_set(src, "scanlines", "on");                                                       /* absent: appended, no trailing newline in src */
        CHECK(o && strstr(o, "key.up = W\r\nscanlines = on\r\n") && !strcmp(o + strlen(o) - 15, "scanlines = on\r\n") + 1);
        free(o);
        o = psxrt_ini_set("a = 1\n", "scale_mode", "integer");
        CHECK(o && !strcmp(o, "a = 1\nscale_mode = integer\n"));
        free(o);
        o = psxrt_ini_set("", "render_scale", "3"); CHECK(o && !strcmp(o, "render_scale = 3\n")); free(o);
        o = psxrt_ini_set("x = 1\r\n", "key.up = ", "W"); CHECK(!o);                                       /* only known keys */
        CHECK(!psxrt_ini_set(src, "window_width", "639") && !psxrt_ini_set(src, "window_width", "7681") && !psxrt_ini_set(src, "window_width", "abc"));
        CHECK(!psxrt_ini_set(src, "render_scale", "9") && !psxrt_ini_set(src, "render_scale", "0") && !psxrt_ini_set(src, "render_scale", "10"));
        CHECK(!psxrt_ini_set(src, "scale_mode", "zoom") && !psxrt_ini_set(src, "filter", "x") && !psxrt_ini_set(src, "scanlines", "1"));
        CHECK(psxrt_ini_valid("window_height", "0") && psxrt_ini_valid("window_height", "1440") && !psxrt_ini_valid("window_height", "359") && !psxrt_ini_valid("window_height", "4321"));
        CHECK(psxrt_ini_valid("side_fill", "art") && !psxrt_ini_valid("side_fill", "on"));
        CHECK(psxrt_ini_valid("window_width", "640") && psxrt_ini_valid("window_width", "7680") && psxrt_ini_valid("hint_shown", "1"));
        /* persist: temp + replace on a real file, and nothing configured is not an error */
        {
            const char *path = "psxrt_io_selftest.ini";
            FILE *f = fopen(path, "wb"); fputs("[play]\n; keep me\nfilter = linear\n", f); fclose(f);
            _putenv_s("PSXRT_SETTINGS_INI", path);
            CHECK(psxrt_ini_persist("filter", "nearest") == 1);
            char b[128] = ""; f = fopen(path, "rb"); size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
            CHECK(!strcmp(b, "[play]\n; keep me\nfilter = nearest\n"));
            CHECK(psxrt_ini_persist("filter", "bogus") == 0);
            _putenv_s("PSXRT_SETTINGS_INI", "no_such_dir/x.ini");
            CHECK(psxrt_ini_persist("filter", "linear") == 0);
            _putenv_s("PSXRT_SETTINGS_INI", "");
            CHECK(psxrt_ini_persist("filter", "linear") == -1);
        }
        /* UTF-16 files (BOM either order, or NULs) are refused and left byte for byte */
        {
            const char *path = "psxrt_io_selftest16.ini";
            static const unsigned char le[] = { 0xFF, 0xFE, 'f', 0, 'i', 0, 'l', 0, 't', 0, 'e', 0, 'r', 0, ' ', 0, '=', 0, ' ', 0, 'x', 0 };
            static const unsigned char be[] = { 0xFE, 0xFF, 0, 'f', 0, '=', 0, 'x' };
            static const unsigned char nobom[] = { 'a', 0, '=', 0, '1', 0 };
            const unsigned char *src[3] = { le, be, nobom };
            const size_t len[3] = { sizeof le, sizeof be, sizeof nobom };
            _putenv_s("PSXRT_SETTINGS_INI", path);
            for (int i = 0; i < 3; i++) {
                FILE *f = fopen(path, "wb"); fwrite(src[i], 1, len[i], f); fclose(f);
                CHECK(psxrt_ini_persist("filter", "nearest") == -2);
                unsigned char b[64]; f = fopen(path, "rb"); size_t n = fread(b, 1, sizeof b, f); fclose(f);
                CHECK(n == len[i] && !memcmp(b, src[i], n));
            }
            _putenv_s("PSXRT_SETTINGS_INI", "");
        }
        /* a game folder with characters outside the ANSI code page: the path comes in wide and must still be written */
        {
            wchar_t dir[MAX_PATH], file[MAX_PATH];
            GetTempPathW(MAX_PATH, dir);
            wcscat(dir, L"psxrt_io_t");
            CreateDirectoryW(dir, NULL);
            wcscat(dir, L"\\\u00e9\u4e2d\u0416");
            CreateDirectoryW(dir, NULL);
            swprintf(file, MAX_PATH, L"%ls\\settings.ini", dir);
            FILE *f = _wfopen(file, L"wb"); CHECK(f != NULL);
            if (f) {
                char b[64] = ""; size_t n;
                fputs("[play]\nfilter = linear\n", f); fclose(f);
                _wputenv_s(L"PSXRT_SETTINGS_INI", file);
                CHECK(psxrt_ini_persist("filter", "nearest") == 1);
                f = _wfopen(file, L"rb"); n = f ? fread(b, 1, sizeof b - 1, f) : 0; if (f) fclose(f); b[n] = 0;
                CHECK(!strcmp(b, "[play]\nfilter = nearest\n"));
                /* debounce: queued values wait 400 ms, the latest per key wins, force writes at once */
                psxrt_ini_queue("filter", "linear", 5000); psxrt_ini_queue("filter", "nearest", 5100); psxrt_ini_queue("scanlines", "on", 5200);
                CHECK(psxrt_ini_flush(5500, 0) == 2);                               /* only 300 ms since the last change */
                CHECK(psxrt_ini_flush(5600, 0) == 1);
                f = _wfopen(file, L"rb"); n = f ? fread(b, 1, sizeof b - 1, f) : 0; if (f) fclose(f); b[n] = 0;
                CHECK(!strcmp(b, "[play]\nfilter = nearest\nscanlines = on\n"));
                CHECK(psxrt_ini_flush(9000, 1) == 2);                               /* nothing pending */
                psxrt_ini_queue("scanlines", "off", 9000);
                CHECK(psxrt_ini_flush(9001, 1) == 1);
                f = _wfopen(file, L"rb"); n = f ? fread(b, 1, sizeof b - 1, f) : 0; if (f) fclose(f); b[n] = 0;
                CHECK(strstr(b, "scanlines = off\n") != NULL);
                _wputenv_s(L"PSXRT_SETTINGS_INI", L"");
            }
        }
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
